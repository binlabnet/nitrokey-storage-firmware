/* otp_sha1.c */
/*
   This file is part of the ARM-Crypto-Lib. Copyright (C) 2006-2010 Daniel Otte (daniel.otte@rub.de)

   This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free
   Software Foundation, either version 3 of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
   FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along with this program.  If not, see <http://www.gnu.org/licenses/>. */
/**
 * \file	sha1.c
 * \author	Daniel Otte
 * \date	2006-10-08
 * \license GPLv3 or later
 * \brief SHA-1 implementation.
 *
 */

// #ifdef NOT_USED

#include "global.h"
#include "tools.h"

#include <string.h> /* memcpy & co */
#include <stdint.h>
#include "sha1.h"

#ifdef DEBUG
#  undef DEBUG
#endif

// #define LITTLE_ENDIAN

#define BIG_ENDIAN

/********************************************************************************************************/

/**
 * \brief initialises given SHA-1 context
 *
 */
void sha1_init (sha1_ctx_t * state)
{
    state->h[0] = 0x67452301;
    state->h[1] = 0xefcdab89;
    state->h[2] = 0x98badcfe;
    state->h[3] = 0x10325476;
    state->h[4] = 0xc3d2e1f0;
    state->length = 0;
}

/********************************************************************************************************/
/* some helping functions */
static const u32 rotl32 (u32 n, u8 bits)
{
    return ((n << bits) | (n >> (32 - bits)));
}

static const u32 change_endian32 (u32 x)
{
#if defined BIG_ENDIAN
    return (x);
#elif defined LITTLE_ENDIAN
    return (((x) << 24) | ((x) >> 24) | (((x) & 0x0000ff00) << 8) | (((x) & 0x00ff0000) >> 8));
#endif

}


/* three SHA-1 inner functions */
const u32 ch (u32 x, u32 y, u32 z)
{
    return ((x & y) ^ ((~x) & z));
}

const u32 maj (u32 x, u32 y, u32 z)
{
    return ((x & y) ^ (x & z) ^ (y & z));
}

const u32 parity (u32 x, u32 y, u32 z)
{
    return ((x ^ y) ^ z);
}

/********************************************************************************************************/
/**
 * \brief "add" a block to the hash
 * This is the core function of the hash algorithm. To understand how it's working
 * and what thoese variables do, take a look at FIPS-182. This is an "alternativ" implementation
 */

#define MASK 0x0000000f

typedef const u32 (*pf_t) (u32 x, u32 y, u32 z);

void sha1_nextBlock (sha1_ctx_t * state, const void* block)
{
    u32 a[5];
    u32 w[16];
    u32 temp;
    u8 t, s, fi, fib;
    pf_t f[] = { ch, parity, maj, parity };
    u32 k[4] = { 0x5a827999,
        0x6ed9eba1,
        0x8f1bbcdc,
        0xca62c1d6
    };

    /* load the w array (changing the endian and so) */
    for (t = 0; t < 16; ++t)
    {
        w[t] = change_endian32 (((u32 *) block)[t]);
    }

#if DEBUG
    u8 dbgi;
    for (dbgi = 0; dbgi < 16; ++dbgi)
    {
        cli_putstr ("\r\nBlock:");
        cli_hexdump (&dbgi, 1);
        cli_putc (':');
        cli_hexdump (&(w[dbgi]), 4);
    }
#endif

    /* load the state */
    memcpy (a, state->h, 5 * sizeof (u32));


    /* the fun stuff */
    for (fi = 0, fib = 0, t = 0; t <= 79; ++t)
    {
        s = t & MASK;
        if (t >= 16)
        {
            w[s] = rotl32 (w[(s + 13) & MASK] ^ w[(s + 8) & MASK] ^ w[(s + 2) & MASK] ^ w[s], 1);
        }

    u32 dtemp;
        temp = rotl32 (a[0], 5) + (dtemp = f[fi] (a[1], a[2], a[3])) + a[4] + k[fi] + w[s];
        memmove (&(a[1]), &(a[0]), 4 * sizeof (u32));   /* e=d; d=c; c=b; b=a; */
        a[0] = temp;
        a[2] = rotl32 (a[2], 30);   /* we might also do rotr32(c,2) */
        fib++;
        if (fib == 20)
        {
            fib = 0;
            fi = (fi + 1) % 4;
        }
    }

    /* update the state */
    for (t = 0; t < 5; ++t)
    {
        state->h[t] += a[t];
    }
    state->length += 512;
}

/********************************************************************************************************/

void sha1_lastBlock (sha1_ctx_t * state, const void* block, u16 length)
{
    u8 lb[SHA1_BLOCK_BYTES];    /* local block */

    while (length >= SHA1_BLOCK_BITS)
    {
        sha1_nextBlock (state, block);
        length -= SHA1_BLOCK_BITS;
        block = (u8 *) block + SHA1_BLOCK_BYTES;
    }
    state->length += length;
    memset (lb, 0, SHA1_BLOCK_BYTES);
    memcpy (lb, block, (length + 7) >> 3);

    /* set the final one bit */
    lb[length >> 3] |= 0x80 >> (length & 0x07);

    if (length > 512 - 64 - 1)
    {   /* not enouth space for 64bit length value */
        sha1_nextBlock (state, lb);
        state->length -= 512;
        memset (lb, 0, SHA1_BLOCK_BYTES);
    }

    /* store the 64bit length value */
#if defined LITTLE_ENDIAN
    /* this is now rolled up */
    u8 i;
    for (i = 0; i < 8; ++i)
    {
        lb[56 + i] = ((u8 *) & (state->length))[7 - i];
    }
#elif defined BIG_ENDIAN
    u8 i;
    for (i = 0; i < 8; ++i)
    {
        lb[56 + i] = ((u8 *) & (state->length))[i];
    }

    // *((u64)&(lb[56])) = state->length;
#endif
    sha1_nextBlock (state, lb);
}

/********************************************************************************************************/

void sha1_ctx2hash (void* dest, sha1_ctx_t * state)
{
#if defined LITTLE_ENDIAN
    u8 i;
    for (i = 0; i < 5; ++i)
    {
        ((u32 *) dest)[i] = change_endian32 (state->h[i]);
    }
#elif defined BIG_ENDIAN
    if (dest != state->h)
        memcpy (dest, state->h, SHA1_HASH_BITS / 8);
#else
# error unsupported endian type!
#endif
}

/********************************************************************************************************/
/**
 *
 *
 */
void otp_sha1 (void* dest, const void* msg, u32 length)
{
    sha1_ctx_t s;
    sha1_init (&s);

    while (length & (~0x0001ff))
    {   /* length>=512 */
        sha1_nextBlock (&s, msg);
        msg = (u8 *) msg + SHA1_BLOCK_BITS / 8; /* increment pointer to next block */
        length -= SHA1_BLOCK_BITS;
    }
    sha1_lastBlock (&s, msg, length);
    sha1_ctx2hash (dest, &s);
}

// #endif // #ifdef NOT_USED