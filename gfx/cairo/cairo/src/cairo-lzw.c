/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2006 Red Hat, Inc
 *
 * This library is free software; you can redistribute it and/or
 * modify it either under the terms of the GNU Lesser General Public
 * License version 2.1 as published by the Free Software Foundation
 * (the "LGPL") or, at your option, under the terms of the Mozilla
 * Public License Version 1.1 (the "MPL"). If you do not alter this
 * notice, a recipient may use your version of this file under either
 * the MPL or the LGPL.
 *
 * You should have received a copy of the LGPL along with this library
 * in the file COPYING-LGPL-2.1; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 * You should have received a copy of the MPL along with this library
 * in the file COPYING-MPL-1.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY
 * OF ANY KIND, either express or implied. See the LGPL or the MPL for
 * the specific language governing rights and limitations.
 *
 * The Original Code is the cairo graphics library.
 *
 * Contributor(s):
 *	Alexander Larsson <alexl@redhat.com>
 *
 * This code is derived from tif_lzw.c in libtiff 3.8.0.
 * The original copyright notice appears below in its entirety.
 */

#include "cairoint.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <string.h>

/*
 * Copyright (c) 1988-1997 Sam Leffler
 * Copyright (c) 1991-1997 Silicon Graphics, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and 
 * its documentation for any purpose is hereby granted without fee, provided
 * that (i) the above copyright notices and this permission notice appear in
 * all copies of the software and related documentation, and (ii) the names of
 * Sam Leffler and Silicon Graphics may not be used in any advertising or
 * publicity relating to the software without the specific, prior written
 * permission of Sam Leffler and Silicon Graphics.
 * 
 * THE SOFTWARE IS PROVIDED "AS-IS" AND WITHOUT WARRANTY OF ANY KIND, 
 * EXPRESS, IMPLIED OR OTHERWISE, INCLUDING WITHOUT LIMITATION, ANY 
 * WARRANTY OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.  
 * 
 * IN NO EVENT SHALL SAM LEFFLER OR SILICON GRAPHICS BE LIABLE FOR
 * ANY SPECIAL, INCIDENTAL, INDIRECT OR CONSEQUENTIAL DAMAGES OF ANY KIND,
 * OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 * WHETHER OR NOT ADVISED OF THE POSSIBILITY OF DAMAGE, AND ON ANY THEORY OF 
 * LIABILITY, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE 
 * OF THIS SOFTWARE.
 */

/*
 * TIFF Library.
 * Rev 5.0 Lempel-Ziv & Welch Compression Support
 *
 * This code is derived from the compress program whose code is
 * derived from software contributed to Berkeley by James A. Woods,
 * derived from original work by Spencer Thomas and Joseph Orost.
 *
 * The original Berkeley copyright notice appears below in its entirety.
 */

#define MAXCODE(n)	((1L<<(n))-1)
/*
 * The TIFF spec specifies that encoded bit
 * strings range from 9 to 12 bits.
 */
#define	BITS_MIN	9		/* start with 9 bits */
#define	BITS_MAX	12		/* max of 12 bit strings */
/* predefined codes */
#define	CODE_CLEAR	256		/* code to clear string table */
#define	CODE_EOI	257		/* end-of-information code */
#define CODE_FIRST	258		/* first free code entry */
#define	CODE_MAX	MAXCODE(BITS_MAX)
#define	HSIZE		9001L		/* 91% occupancy */
#define	HSHIFT		(13-8)
#ifdef LZW_COMPAT
/* NB: +1024 is for compatibility with old files */
#define	CSIZE		(MAXCODE(BITS_MAX)+1024L)
#else
#define	CSIZE		(MAXCODE(BITS_MAX)+1L)
#endif

typedef uint16_t hcode_t;			/* codes fit in 16 bits */
typedef struct {
	long	hash;
	hcode_t	code;
} hash_t;

typedef struct {
	/* Out buffer */
	unsigned char  *out_buffer;		/* compressed out buffer */
	size_t		out_buffer_size;	/* # of allocated bytes in out buffer */
	unsigned char  *out_buffer_pos;		/* current spot in out buffer */
	size_t		out_buffer_bytes;	/* # of data bytes in out buffer */
	unsigned char  *out_buffer_end;		/* bound on out_buffer */
	
	unsigned short	nbits;		/* # of bits/code */
	unsigned short	maxcode;	/* maximum code for lzw_nbits */
	unsigned short	free_ent;	/* next free entry in hash table */
	long		nextdata;	/* next bits of i/o */
	long		nextbits;	/* # of valid bits in lzw_nextdata */

	int	enc_oldcode;		/* last code encountered */
	long	enc_checkpoint;		/* point at which to clear table */
#define CHECK_GAP	10000		/* enc_ratio check interval */
	long	enc_ratio;		/* current compression ratio */
	long	enc_incount;		/* (input) data bytes encoded */
	long	enc_outcount;		/* encoded (output) bytes */
	hash_t*	enc_hashtab;		/* kept separate for small machines */
} LZWCodecState;

static  void cl_hash(LZWCodecState*);

/*
 * LZW Encoding.
 */

static unsigned char *
grow_out_buffer (LZWCodecState *sp, unsigned char *op)
{
    size_t cc;
    
    cc = (size_t)(op - sp->out_buffer);
    
    sp->out_buffer_size = sp->out_buffer_size * 2;
    sp->out_buffer = realloc (sp->out_buffer, sp->out_buffer_size);
    /*
     * The 4 here insures there is space for 2 max-sized
     * codes in LZWEncode and LZWPostDecode.
     */
    sp->out_buffer_end = sp->out_buffer + sp->out_buffer_size-1 - 4;
    
    return sp->out_buffer + cc;
}

static int
LZWSetupEncode (LZWCodecState* sp)
{
    memset (sp, 0, sizeof (LZWCodecState));
    sp->enc_hashtab = (hash_t*) malloc (HSIZE * sizeof (hash_t));
    if (sp->enc_hashtab == NULL)
	return 0;
    return 1;
}

static void
LZWFreeEncode (LZWCodecState* sp)
{
    if (sp->enc_hashtab)
	free (sp->enc_hashtab);
}


/*
 * Reset encoding state at the start of a strip.
 */
static void
LZWPreEncode (LZWCodecState *sp)
{
    sp->nbits = BITS_MIN;
    sp->maxcode = MAXCODE(BITS_MIN);
    sp->free_ent = CODE_FIRST;
    sp->nextbits = 0;
    sp->nextdata = 0;
    sp->enc_checkpoint = CHECK_GAP;
    sp->enc_ratio = 0;
    sp->enc_incount = 0;
    sp->enc_outcount = 0;
    /*
     * The 4 here insures there is space for 2 max-sized
     * codes in LZWEncode and LZWPostDecode.
     */
    sp->out_buffer_end = sp->out_buffer + sp->out_buffer_size-1 - 4;
    cl_hash(sp); 			/* clear hash table */
    sp->enc_oldcode = (hcode_t) -1;	/* generates CODE_CLEAR in LZWEncode */
}

#define	CALCRATIO(sp, rat) {					\
    if (incount > 0x007fffff) { /* NB: shift will overflow */	\
	rat = outcount >> 8;					\
	rat = (rat == 0 ? 0x7fffffff : incount/rat);		\
    } else							\
	rat = (incount << 8) / outcount;			\
}
#define	PutNextCode(op, c) {					\
    nextdata = (nextdata << nbits) | c;				\
    nextbits += nbits;						\
    *op++ = (unsigned char)(nextdata >> (nextbits-8));		\
    nextbits -= 8;						\
    if (nextbits >= 8) {					\
	*op++ = (unsigned char)(nextdata >> (nextbits-8));	\
	nextbits -= 8;						\
    }								\
    outcount += nbits;						\
}

/*
 * Encode a chunk of pixels.
 *
 * Uses an open addressing double hashing (no chaining) on the 
 * prefix code/next character combination.  We do a variant of
 * Knuth's algorithm D (vol. 3, sec. 6.4) along with G. Knott's
 * relatively-prime secondary probe.  Here, the modular division
 * first probe is gives way to a faster exclusive-or manipulation. 
 * Also do block compression with an adaptive reset, whereby the
 * code table is cleared when the compression ratio decreases,
 * but after the table fills.  The variable-length output codes
 * are re-sized at this point, and a CODE_CLEAR is generated
 * for the decoder. 
 */
static int
LZWEncode (LZWCodecState *sp,
	   unsigned char *bp,
	   size_t cc)
{
    register long fcode;
    register hash_t *hp;
    register int h, c;
    hcode_t ent;
    long disp;
    long incount, outcount, checkpoint;
    long nextdata, nextbits;
    int free_ent, maxcode, nbits;
    unsigned char *op;
    
    /*
     * Load local state.
     */
    incount = sp->enc_incount;
    outcount = sp->enc_outcount;
    checkpoint = sp->enc_checkpoint;
    nextdata = sp->nextdata;
    nextbits = sp->nextbits;
    free_ent = sp->free_ent;
    maxcode = sp->maxcode;
    nbits = sp->nbits;
    op = sp->out_buffer_pos;
    ent = sp->enc_oldcode;
    
    if (ent == (hcode_t) -1 && cc > 0) {
	/*
	 * NB: This is safe because it can only happen
	 *     at the start of a strip where we know there
	 *     is space in the data buffer.
	 */
	PutNextCode(op, CODE_CLEAR);
	ent = *bp++; cc--; incount++;
    }
    while (cc > 0) {
	c = *bp++; cc--; incount++;
	fcode = ((long)c << BITS_MAX) + ent;
	h = (c << HSHIFT) ^ ent;	/* xor hashing */
#ifdef _WINDOWS
	/*
	 * Check hash index for an overflow.
	 */
	if (h >= HSIZE)
	    h -= HSIZE;
#endif
	hp = &sp->enc_hashtab[h];
	if (hp->hash == fcode) {
	    ent = hp->code;
	    continue;
	}
	if (hp->hash >= 0) {
	    /*
	     * Primary hash failed, check secondary hash.
	     */
	    disp = HSIZE - h;
	    if (h == 0)
		disp = 1;
	    do {
		/*
		 * Avoid pointer arithmetic 'cuz of
		 * wraparound problems with segments.
		 */
		if ((h -= disp) < 0)
		    h += HSIZE;
		hp = &sp->enc_hashtab[h];
		if (hp->hash == fcode) {
		    ent = hp->code;
		    goto hit;
		}
	    } while (hp->hash >= 0);
	}
	/*
	 * New entry, emit code and add to table.
	 */
	/*
	 * Verify there is space in the buffer for the code
	 * and any potential Clear code that might be emitted
	 * below.  The value of limit is setup so that there
	 * are at least 4 bytes free--room for 2 codes.
	 */
	if (op > sp->out_buffer_end) {
	    op = grow_out_buffer (sp, op);
	    if (sp->out_buffer == NULL) {
		return 0;
	    }
	}
	PutNextCode(op, ent);
	ent = c;
	hp->code = free_ent++;
	hp->hash = fcode;
	if (free_ent == CODE_MAX-1) {
	    /* table is full, emit clear code and reset */
	    cl_hash(sp);
	    sp->enc_ratio = 0;
	    incount = 0;
	    outcount = 0;
	    free_ent = CODE_FIRST;
	    PutNextCode(op, CODE_CLEAR);
	    nbits = BITS_MIN;
	    maxcode = MAXCODE(BITS_MIN);
	} else {
	    /*
	     * If the next entry is going to be too big for
	     * the code size, then increase it, if possible.
	     */
	    if (free_ent > maxcode) {
		nbits++;
		assert(nbits <= BITS_MAX);
		maxcode = (int) MAXCODE(nbits);
	    } else if (incount >= checkpoint) {
		long rat;
		/*
		 * Check compression ratio and, if things seem
		 * to be slipping, clear the hash table and
		 * reset state.  The compression ratio is a
		 * 24+8-bit fractional number.
		 */
		checkpoint = incount+CHECK_GAP;
		CALCRATIO(sp, rat);
		if (rat <= sp->enc_ratio) {
		    cl_hash(sp);
		    sp->enc_ratio = 0;
		    incount = 0;
		    outcount = 0;
		    free_ent = CODE_FIRST;
		    PutNextCode(op, CODE_CLEAR);
		    nbits = BITS_MIN;
		    maxcode = MAXCODE(BITS_MIN);
		} else
		    sp->enc_ratio = rat;
	    }
	}
    hit:
	;
    }
    
    /*
     * Restore global state.
     */
    sp->enc_incount = incount;
    sp->enc_outcount = outcount;
    sp->enc_checkpoint = checkpoint;
    sp->enc_oldcode = ent;
    sp->nextdata = nextdata;
    sp->nextbits = nextbits;
    sp->free_ent = free_ent;
    sp->maxcode = maxcode;
    sp->nbits = nbits;
    sp->out_buffer_pos = op;
    return 1;
}

/*
 * Finish off an encoded strip by flushing the last
 * string and tacking on an End Of Information code.
 */
static int
LZWPostEncode (LZWCodecState *sp)
{
    unsigned char *op = sp->out_buffer_pos;
    long nextbits = sp->nextbits;
    long nextdata = sp->nextdata;
    long outcount = sp->enc_outcount;
    int nbits = sp->nbits;
    
    if (op > sp->out_buffer_end) {
	op = grow_out_buffer (sp, op);
	if (sp->out_buffer == NULL) {
	    return 0;
	}
    }
    if (sp->enc_oldcode != (hcode_t) -1) {
	PutNextCode(op, sp->enc_oldcode);
	sp->enc_oldcode = (hcode_t) -1;
    }
    PutNextCode(op, CODE_EOI);
    if (nextbits > 0) 
	*op++ = (unsigned char)(nextdata << (8-nextbits));
    sp->out_buffer_bytes = (size_t)(op - sp->out_buffer);
    return 1;
}

/*
 * Reset encoding hash table.
 */
static void
cl_hash (LZWCodecState* sp)
{
    register hash_t *hp = &sp->enc_hashtab[HSIZE-1];
    register long i = HSIZE-8;
    
    do {
	i -= 8;
	hp[-7].hash = -1;
	hp[-6].hash = -1;
	hp[-5].hash = -1;
	hp[-4].hash = -1;
	hp[-3].hash = -1;
	hp[-2].hash = -1;
	hp[-1].hash = -1;
	hp[ 0].hash = -1;
	hp -= 8;
    } while (i >= 0);
    for (i += 8; i > 0; i--, hp--)
	hp->hash = -1;
}

/*
 * Copyright (c) 1985, 1986 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * James A. Woods, derived from original work by Spencer Thomas
 * and Joseph Orost.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

void *
_cairo_compress_lzw (void *data, unsigned long data_size, unsigned long *compressed_size)
{
    LZWCodecState state;
    
    if (!LZWSetupEncode (&state))
	goto bail0;
    
    state.out_buffer_size = data_size/4;
    /* We need *some* space at least */
    if (state.out_buffer_size < 256)
	state.out_buffer_size = 256;
    state.out_buffer = malloc (state.out_buffer_size);
    if (state.out_buffer == NULL)
	goto bail1;
    
    state.out_buffer_pos = state.out_buffer;
    state.out_buffer_bytes = 0;
    
    LZWPreEncode (&state);
    if (!LZWEncode (&state, data, data_size))
	goto bail2;
    if (!LZWPostEncode(&state))
	goto bail2;
    
    LZWFreeEncode(&state);

    *compressed_size = state.out_buffer_bytes;
    return state.out_buffer;
    
 bail2:
    if (state.out_buffer)
	free (state.out_buffer);
 bail1:
    LZWFreeEncode(&state);
 bail0:    
    return NULL;
}
