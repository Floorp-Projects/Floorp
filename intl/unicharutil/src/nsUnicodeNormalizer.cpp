/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* This file is modified from JPNIC's mDNKit, it is under both MPL and 
 * JPNIC's license.
 */

 /* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Unicode case conversion helpers.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corp..
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * Copyright (c) 2000,2002 Japan Network Information Center.
 * All rights reserved.
 *  
 * By using this file, you agree to the terms and conditions set forth bellow.
 * 
 * 			LICENSE TERMS AND CONDITIONS 
 * 
 * The following License Terms and Conditions apply, unless a different
 * license is obtained from Japan Network Information Center ("JPNIC"),
 * a Japanese association, Kokusai-Kougyou-Kanda Bldg 6F, 2-3-4 Uchi-Kanda,
 * Chiyoda-ku, Tokyo 101-0047, Japan.
 * 
 * 1. Use, Modification and Redistribution (including distribution of any
 *    modified or derived work) in source and/or binary forms is permitted
 *    under this License Terms and Conditions.
 * 
 * 2. Redistribution of source code must retain the copyright notices as they
 *    appear in each source code file, this License Terms and Conditions.
 * 
 * 3. Redistribution in binary form must reproduce the Copyright Notice,
 *    this License Terms and Conditions, in the documentation and/or other
 *    materials provided with the distribution.  For the purposes of binary
 *    distribution the "Copyright Notice" refers to the following language:
 *    "Copyright (c) 2000-2002 Japan Network Information Center.  All rights reserved."
 * 
 * 4. The name of JPNIC may not be used to endorse or promote products
 *    derived from this Software without specific prior written approval of
 *    JPNIC.
 * 
 * 5. Disclaimer/Limitation of Liability: THIS SOFTWARE IS PROVIDED BY JPNIC
 *    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 *    PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL JPNIC BE LIABLE
 *    FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 *    BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 *    WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 *    OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 *    ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
 */

#include <stdlib.h>
#include <string.h>

#include "nsUnicharUtils.h"
#include "nsMemory.h"
#include "nsCRT.h"
#include "nsUnicodeNormalizer.h"
#include "nsString.h"
#include "nsReadableUtils.h"

NS_IMPL_ISUPPORTS1(nsUnicodeNormalizer, nsIUnicodeNormalizer)


nsUnicodeNormalizer::nsUnicodeNormalizer()
{
}

nsUnicodeNormalizer::~nsUnicodeNormalizer()
{
}



#define NS_ERROR_UNORM_MOREOUTPUT  \
        NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_GENERAL, 0x21)

#define NS_SUCCESS_UNORM_NOTFOUND  \
        NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_GENERAL, 0x11)


#define END_BIT		0x80000000


/*
 * Some constants for Hangul decomposition/composition.
 * These things were taken from unicode book. 
 */
#define SBase		0xac00
#define LBase		0x1100
#define VBase		0x1161
#define TBase		0x11a7
#define LCount		19
#define VCount		21
#define TCount		28
#define SLast		(SBase + LCount * VCount * TCount)

struct composition {
	PRUint32 c2;	/* 2nd character */
	PRUint32 comp;	/* composed character */
};


#include "normalization_data.h"

/*
 * Macro for multi-level index table.
 */
#define LOOKUPTBL(vprefix, mprefix, v) \
	DMAP(vprefix)[\
		IMAP(vprefix)[\
			IMAP(vprefix)[IDX0(mprefix, v)] + IDX1(mprefix, v)\
		]\
	].tbl[IDX2(mprefix, v)]

#define IDX0(mprefix, v) IDX_0(v, BITS1(mprefix), BITS2(mprefix))
#define IDX1(mprefix, v) IDX_1(v, BITS1(mprefix), BITS2(mprefix))
#define IDX2(mprefix, v) IDX_2(v, BITS1(mprefix), BITS2(mprefix))

#define IDX_0(v, bits1, bits2)	((v) >> ((bits1) + (bits2)))
#define IDX_1(v, bits1, bits2)	(((v) >> (bits2)) & ((1 << (bits1)) - 1))
#define IDX_2(v, bits1, bits2)	((v) & ((1 << (bits2)) - 1))

#define BITS1(mprefix)	mprefix ## _BITS_1
#define BITS2(mprefix)	mprefix ## _BITS_2

#define IMAP(vprefix)	vprefix ## _imap
#define DMAP(vprefix)	vprefix ## _table
#define SEQ(vprefix)	vprefix ## _seq

static PRInt32
canonclass(PRUint32 c) {
	/* Look up canonicalclass table. */
	return (LOOKUPTBL(canon_class, CANON_CLASS, c));
}

static PRInt32
decompose_char(PRUint32 c, const PRUint32 **seqp)
{
	/* Look up decomposition table. */
	PRInt32 seqidx = LOOKUPTBL(decompose, DECOMP, c);
	*seqp = SEQ(decompose) + (seqidx & ~DECOMP_COMPAT);
	return (seqidx);
}

static PRInt32
compose_char(PRUint32 c,
				const struct composition **compp)
{
	/* Look up composition table. */
	PRInt32 seqidx = LOOKUPTBL(compose, CANON_COMPOSE, c);
	*compp = SEQ(compose) + (seqidx & 0xffff);
	return (seqidx >> 16);
}

static nsresult
mdn__unicode_decompose(PRInt32 compat, PRUint32 *v, size_t vlen,
		       PRUint32 c, PRInt32 *decomp_lenp)
{
	PRUint32 *vorg = v;
	PRInt32 seqidx;
	const PRUint32 *seq;

	//assert(v != NULL && vlen >= 0 && decomp_lenp != NULL);

	/*
	 * First, check for Hangul.
	 */
	if (SBase <= c && c < SLast) {
		PRInt32 idx, t_offset, v_offset, l_offset;

		idx = c - SBase;
		t_offset = idx % TCount;
		idx /= TCount;
		v_offset = idx % VCount;
		l_offset = idx / VCount;
		if ((t_offset == 0 && vlen < 2) || (t_offset > 0 && vlen < 3))
			return (NS_ERROR_UNORM_MOREOUTPUT);
		*v++ = LBase + l_offset;
		*v++ = VBase + v_offset;
		if (t_offset > 0)
			*v++ = TBase + t_offset;
		*decomp_lenp = v - vorg;
		return (NS_OK);
	}

	/*
	 * Look up decomposition table.  If no decomposition is defined
	 * or if it is a compatibility decomosition when canonical
	 * decomposition requested, return 'NS_SUCCESS_UNORM_NOTFOUND'.
	 */
	seqidx = decompose_char(c, &seq);
	if (seqidx == 0 || (compat == 0 && (seqidx & DECOMP_COMPAT) != 0))
		return (NS_SUCCESS_UNORM_NOTFOUND);
	
	/*
	 * Copy the decomposed sequence.  The end of the sequence are
	 * marked with END_BIT.
	 */
	do {
		PRUint32 c;
		PRInt32 dlen;
		nsresult r;

		c = *seq & ~END_BIT;

		/* Decompose recursively. */
		r = mdn__unicode_decompose(compat, v, vlen, c, &dlen);
		if (r == NS_OK) {
			v += dlen;
			vlen -= dlen;
		} else if (r == NS_SUCCESS_UNORM_NOTFOUND) {
			if (vlen < 1)
				return (NS_ERROR_UNORM_MOREOUTPUT);
			*v++ = c;
			vlen--;
		} else {
			return (r);
		}

	} while ((*seq++ & END_BIT) == 0);
	
	*decomp_lenp = v - vorg;

	return (NS_OK);
}

static PRInt32
mdn__unicode_iscompositecandidate(PRUint32 c)
{
	const struct composition *dummy;

	/* Check for Hangul */
	if ((LBase <= c && c < LBase + LCount) || (SBase <= c && c < SLast))
		return (1);

	/*
	 * Look up composition table.  If there are no composition
	 * that begins with the given character, it is not a
	 * composition candidate.
	 */
	if (compose_char(c, &dummy) == 0)
		return (0);
	else
		return (1);
}

static nsresult
mdn__unicode_compose(PRUint32 c1, PRUint32 c2, PRUint32 *compp)
{
	PRInt32 n;
	PRInt32 lo, hi;
	const struct composition *cseq;

	//assert(compp != NULL);

	/*
	 * Check for Hangul.
	 */
	if (LBase <= c1 && c1 < LBase + LCount &&
	    VBase <= c2 && c2 < VBase + VCount) {
		/*
		 * Hangul L and V.
		 */
		*compp = SBase +
			((c1 - LBase) * VCount + (c2 - VBase)) * TCount;
		return (NS_OK);
	} else if (SBase <= c1 && c1 < SLast &&
		   TBase <= c2 && c2 < TBase + TCount &&
		   (c1 - SBase) % TCount == 0) {
		/*
		 * Hangul LV and T.
		 */
		*compp = c1 + (c2 - TBase);
		return (NS_OK);
	}

	/*
	 * Look up composition table.  If the result is 0, no composition
	 * is defined.  Otherwise, upper 16bits of the result contains
	 * the number of composition that begins with 'c1', and the lower
	 * 16bits is the offset in 'compose_seq'.
	 */
	if ((n = compose_char(c1, &cseq)) == 0)
		return (NS_SUCCESS_UNORM_NOTFOUND);

	/*
	 * The composite sequences are sorted by the 2nd character 'c2'.
	 * So we can use binary search.
	 */
	lo = 0;
	hi = n - 1;
	while (lo <= hi) {
		PRInt32 mid = (lo + hi) / 2;

		if (cseq[mid].c2 < c2) {
			lo = mid + 1;
		} else if (cseq[mid].c2 > c2) {
			hi = mid - 1;
		} else {
			*compp = cseq[mid].comp;
			return (NS_OK);
		}
	}
	return (NS_SUCCESS_UNORM_NOTFOUND);
}


#define WORKBUF_SIZE		128
#define WORKBUF_SIZE_MAX	10000

typedef struct {
	PRInt32 cur;		/* pointing now processing character */
	PRInt32 last;		/* pointing just after the last character */
	PRInt32 size;		/* size of UCS and CLASS array */
	PRUint32 *ucs;	/* UCS-4 characters */
	PRInt32 *cclass;		/* and their canonical classes */
	PRUint32 ucs_buf[WORKBUF_SIZE];	/* local buffer */
	PRInt32 class_buf[WORKBUF_SIZE];		/* ditto */
} workbuf_t;

static nsresult	decompose(workbuf_t *wb, PRUint32 c, PRInt32 compat);
static void		get_class(workbuf_t *wb);
static void		reorder(workbuf_t *wb);
static void		compose(workbuf_t *wb);
static nsresult flush_before_cur(workbuf_t *wb, nsAString& aToStr);
static void		workbuf_init(workbuf_t *wb);
static void		workbuf_free(workbuf_t *wb);
static nsresult	workbuf_extend(workbuf_t *wb);
static nsresult	workbuf_append(workbuf_t *wb, PRUint32 c);
static void		workbuf_shift(workbuf_t *wb, PRInt32 shift);
static void		workbuf_removevoid(workbuf_t *wb);


static nsresult
mdn_normalize(bool do_composition, bool compat,
	  const nsAString& aSrcStr, nsAString& aToStr)
{
	workbuf_t wb;
	nsresult r = NS_OK;
	/*
	 * Initialize working buffer.
	 */
	workbuf_init(&wb);

	nsAString::const_iterator start, end;
	aSrcStr.BeginReading(start); 
	aSrcStr.EndReading(end); 

	while (start != end) {
		PRUint32 c;
		PRUnichar curChar;

		//assert(wb.cur == wb.last);

		/*
		 * Get one character from 'from'.
		 */
		curChar= *start++;

		if (NS_IS_HIGH_SURROGATE(curChar) && start != end && NS_IS_LOW_SURROGATE(*(start)) ) {
			c = SURROGATE_TO_UCS4(curChar, *start);
			++start;
		} else {
			c = curChar;
		}

		/*
		 * Decompose it.
		 */
		if ((r = decompose(&wb, c, compat)) != NS_OK)
			break;

		/*
		 * Get canonical class.
		 */
		get_class(&wb);

		/*
		 * Reorder & compose.
		 */
		for (; wb.cur < wb.last; wb.cur++) {
			if (wb.cur == 0) {
				continue;
			} else if (wb.cclass[wb.cur] > 0) {
				/*
				 * This is not a starter. Try reordering.
				 * Note that characters up to it are
				 * already in canonical order.
				 */
				reorder(&wb);
				continue;
			}

			/*
			 * This is a starter character, and there are
			 * some characters before it.  Those characters
			 * have been reordered properly, and
			 * ready for composition.
			 */
			if (do_composition && wb.cclass[0] == 0)
				compose(&wb);

			/*
			 * If CUR points to a starter character,
			 * then process of characters before CUR are
			 * already finished, because any further
			 * reordering/composition for them are blocked
			 * by the starter CUR points.
			 */
			if (wb.cur > 0 && wb.cclass[wb.cur] == 0) {
				/* Flush everything before CUR. */
				r = flush_before_cur(&wb, aToStr);
				if (r != NS_OK)
					break;
			}
		}
	}

	if (r == NS_OK) {
		if (do_composition && wb.cur > 0 && wb.cclass[0] == 0) {
			/*
			 * There is some characters left in WB.
			 * They are ordered, but not composed yet.
			 * Now CUR points just after the last character in WB,
			 * and since compose() tries to compose characters
			 * between top and CUR inclusive, we must make CUR
			 * one character back during compose().
			 */
			wb.cur--;
			compose(&wb);
			wb.cur++;
		}
		/*
		 * Call this even when WB.CUR == 0, to make TO
		 * NUL-terminated.
		 */
		r = flush_before_cur(&wb, aToStr);
	}

	workbuf_free(&wb);

	return (r);
}

static nsresult
decompose(workbuf_t *wb, PRUint32 c, PRInt32 compat) {
	nsresult r;
	PRInt32 dec_len;

again:
	r = mdn__unicode_decompose(compat, wb->ucs + wb->last,
				   wb->size - wb->last, c, &dec_len);
	switch (r) {
	case NS_OK:
		wb->last += dec_len;
		return (NS_OK);
	case NS_SUCCESS_UNORM_NOTFOUND:
		return (workbuf_append(wb, c));
	case NS_ERROR_UNORM_MOREOUTPUT:
		if ((r = workbuf_extend(wb)) != NS_OK)
			return (r);
		if (wb->size > WORKBUF_SIZE_MAX) {
			// "mdn__unormalize_form*: " "working buffer too large\n"
			return (NS_ERROR_FAILURE);
		}
		goto again;
	default:
		return (r);
	}
	/* NOTREACHED */
}

static void		
get_class(workbuf_t *wb) {
	PRInt32 i;

	for (i = wb->cur; i < wb->last; i++)
		wb->cclass[i] = canonclass(wb->ucs[i]);
}

static void
reorder(workbuf_t *wb) {
	PRUint32 c;
	PRInt32 i;
	PRInt32 cclass;

	//assert(wb != NULL);

	i = wb->cur;
	c = wb->ucs[i];
	cclass = wb->cclass[i];

	while (i > 0 && wb->cclass[i - 1] > cclass) {
		wb->ucs[i] = wb->ucs[i - 1];
		wb->cclass[i] =wb->cclass[i - 1];
		i--;
		wb->ucs[i] = c;
		wb->cclass[i] = cclass;
	}
}

static void
compose(workbuf_t *wb) {
	PRInt32 cur;
	PRUint32 *ucs;
	PRInt32 *cclass;
	PRInt32 last_class;
	PRInt32 nvoids;
	PRInt32 i;

	//assert(wb != NULL && wb->cclass[0] == 0);

	cur = wb->cur;
	ucs = wb->ucs;
	cclass = wb->cclass;

	/*
	 * If there are no decomposition sequence that begins with
	 * the top character, composition is impossible.
	 */
	if (!mdn__unicode_iscompositecandidate(ucs[0]))
		return;

	last_class = 0;
	nvoids = 0;
	for (i = 1; i <= cur; i++) {
		PRUint32 c;
		PRInt32 cl = cclass[i];

		if ((last_class < cl || cl == 0) &&
		    mdn__unicode_compose(ucs[0], ucs[i],
					 &c) == NS_OK) {
			/*
			 * Replace the top character with the composed one.
			 */
			ucs[0] = c;
			cclass[0] = canonclass(c);

			cclass[i] = -1;	/* void this character */
			nvoids++;
		} else {
			last_class = cl;
		}
	}

	/* Purge void characters, if any. */
	if (nvoids > 0)
		workbuf_removevoid(wb);
}

static nsresult
flush_before_cur(workbuf_t *wb, nsAString& aToStr) 
{
	PRInt32 i;

	for (i = 0; i < wb->cur; i++) {
		if (!IS_IN_BMP(wb->ucs[i])) {
			aToStr.Append((PRUnichar)H_SURROGATE(wb->ucs[i]));
			aToStr.Append((PRUnichar)L_SURROGATE(wb->ucs[i]));
		} else {
			aToStr.Append((PRUnichar)(wb->ucs[i]));
		}
	}

	workbuf_shift(wb, wb->cur);

	return (NS_OK);
}

static void
workbuf_init(workbuf_t *wb) {
	wb->cur = 0;
	wb->last = 0;
	wb->size = WORKBUF_SIZE;
	wb->ucs = wb->ucs_buf;
	wb->cclass = wb->class_buf;
}

static void
workbuf_free(workbuf_t *wb) {
	if (wb->ucs != wb->ucs_buf) {
		nsMemory::Free(wb->ucs);
		nsMemory::Free(wb->cclass);
	}
}

static nsresult
workbuf_extend(workbuf_t *wb) {
	PRInt32 newsize = wb->size * 3;

	if (wb->ucs == wb->ucs_buf) {
		wb->ucs = (PRUint32*)nsMemory::Alloc(sizeof(wb->ucs[0]) * newsize);
		if (!wb->ucs)
			return NS_ERROR_OUT_OF_MEMORY;
		wb->cclass = (PRInt32*)nsMemory::Alloc(sizeof(wb->cclass[0]) * newsize);
		if (!wb->cclass) {
			nsMemory::Free(wb->ucs);
			wb->ucs = NULL;
			return NS_ERROR_OUT_OF_MEMORY;
		}
	} else {
		void* buf = nsMemory::Realloc(wb->ucs, sizeof(wb->ucs[0]) * newsize);
		if (!buf)
			return NS_ERROR_OUT_OF_MEMORY;
		wb->ucs = (PRUint32*)buf;
		buf = nsMemory::Realloc(wb->cclass, sizeof(wb->cclass[0]) * newsize);
		if (!buf)
			return NS_ERROR_OUT_OF_MEMORY;
		wb->cclass = (PRInt32*)buf;
	}
	return (NS_OK);
}

static nsresult
workbuf_append(workbuf_t *wb, PRUint32 c) {
	nsresult r;

	if (wb->last >= wb->size && (r = workbuf_extend(wb)) != NS_OK)
		return (r);
	wb->ucs[wb->last++] = c;
	return (NS_OK);
}

static void
workbuf_shift(workbuf_t *wb, PRInt32 shift) {
	PRInt32 nmove;

	//assert(wb != NULL && wb->cur >= shift);

	nmove = wb->last - shift;
	memmove(&wb->ucs[0], &wb->ucs[shift],
		      nmove * sizeof(wb->ucs[0]));
	memmove(&wb->cclass[0], &wb->cclass[shift],
		      nmove * sizeof(wb->cclass[0]));
	wb->cur -= shift;
	wb->last -= shift;
}

static void
workbuf_removevoid(workbuf_t *wb) {
	PRInt32 i, j;
	PRInt32 last = wb->last;

	for (i = j = 0; i < last; i++) {
		if (wb->cclass[i] >= 0) {
			if (j < i) {
				wb->ucs[j] = wb->ucs[i];
				wb->cclass[j] = wb->cclass[i];
			}
			j++;
		}
	}
	wb->cur -= last - j;
	wb->last = j;
}

nsresult  
nsUnicodeNormalizer::NormalizeUnicodeNFD( const nsAString& aSrc, nsAString& aDest)
{
  return mdn_normalize(PR_FALSE, PR_FALSE, aSrc, aDest);
}

nsresult  
nsUnicodeNormalizer::NormalizeUnicodeNFC( const nsAString& aSrc, nsAString& aDest)
{
  return mdn_normalize(PR_TRUE, PR_FALSE, aSrc, aDest);
}

nsresult  
nsUnicodeNormalizer::NormalizeUnicodeNFKD( const nsAString& aSrc, nsAString& aDest)
{
  return mdn_normalize(PR_FALSE, PR_TRUE, aSrc, aDest);
}

nsresult  
nsUnicodeNormalizer::NormalizeUnicodeNFKC( const nsAString& aSrc, nsAString& aDest)
{
  return mdn_normalize(PR_TRUE, PR_TRUE, aSrc, aDest);
}

