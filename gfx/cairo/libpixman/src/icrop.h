/*
 * Id: $
 *
 * Copyright © 1998 Keith Packard
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Keith Packard not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Keith Packard makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * KEITH PACKARD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL KEITH PACKARD BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _ICROP_H_
#define _ICROP_H_

typedef struct _mergeRopBits {
    pixman_bits_t   ca1, cx1, ca2, cx2;
} IcMergeRopRec, *IcMergeRopPtr;

extern const IcMergeRopRec IcMergeRopBits[16] pixman_private;

#define IcDeclareMergeRop() pixman_bits_t   _ca1, _cx1, _ca2, _cx2;
#define IcDeclarePrebuiltMergeRop()	pixman_bits_t	_cca, _ccx;

#define IcInitializeMergeRop(alu,pm) {\
    const IcMergeRopRec  *_bits; \
    _bits = &IcMergeRopBits[alu]; \
    _ca1 = _bits->ca1 &  pm; \
    _cx1 = _bits->cx1 | ~pm; \
    _ca2 = _bits->ca2 &  pm; \
    _cx2 = _bits->cx2 &  pm; \
}

#define IcDestInvarientRop(alu,pm)  ((pm) == IC_ALLONES && \
				     (((alu) >> 1 & 5) == ((alu) & 5)))

#define IcDestInvarientMergeRop()   (_ca1 == 0 && _cx1 == 0)

/* AND has higher precedence than XOR */

#define IcDoMergeRop(src, dst) \
    (((dst) & (((src) & _ca1) ^ _cx1)) ^ (((src) & _ca2) ^ _cx2))

#define IcDoDestInvarientMergeRop(src)	(((src) & _ca2) ^ _cx2)

#define IcDoMaskMergeRop(src, dst, mask) \
    (((dst) & ((((src) & _ca1) ^ _cx1) | ~(mask))) ^ ((((src) & _ca2) ^ _cx2) & (mask)))

#define IcDoLeftMaskByteMergeRop(dst, src, lb, l) { \
    pixman_bits_t  __xor = ((src) & _ca2) ^ _cx2; \
    IcDoLeftMaskByteRRop(dst,lb,l,((src) & _ca1) ^ _cx1,__xor); \
}

#define IcDoRightMaskByteMergeRop(dst, src, rb, r) { \
    pixman_bits_t  __xor = ((src) & _ca2) ^ _cx2; \
    IcDoRightMaskByteRRop(dst,rb,r,((src) & _ca1) ^ _cx1,__xor); \
}

#define IcDoRRop(dst, and, xor)	(((dst) & (and)) ^ (xor))

#define IcDoMaskRRop(dst, and, xor, mask) \
    (((dst) & ((and) | ~(mask))) ^ (xor & mask))

/*
 * Take a single bit (0 or 1) and generate a full mask
 */
#define IcFillFromBit(b,t)	(~((t) ((b) & 1)-1))

#define IcXorT(rop,fg,pm,t) ((((fg) & IcFillFromBit((rop) >> 1,t)) | \
			      (~(fg) & IcFillFromBit((rop) >> 3,t))) & (pm))

#define IcAndT(rop,fg,pm,t) ((((fg) & IcFillFromBit (rop ^ (rop>>1),t)) | \
			      (~(fg) & IcFillFromBit((rop>>2) ^ (rop>>3),t))) | \
			     ~(pm))

#define IcXor(rop,fg,pm)	IcXorT(rop,fg,pm,pixman_bits_t)

#define IcAnd(rop,fg,pm)	IcAndT(rop,fg,pm,pixman_bits_t)

#define IcXorStip(rop,fg,pm)    IcXorT(rop,fg,pm,IcStip)

#define IcAndStip(rop,fg,pm)	IcAndT(rop,fg,pm,IcStip)

/*
 * Stippling operations; 
 */

/* half of table */
extern const pixman_bits_t icStipple16Bits[256] pixman_private;
#define IcStipple16Bits(b) \
    (icStipple16Bits[(b)&0xff] | icStipple16Bits[(b) >> 8] << IC_HALFUNIT)

pixman_private const pixman_bits_t *
IcStippleTable(int bits);

#define IcStippleRRop(dst, b, fa, fx, ba, bx) \
    (IcDoRRop(dst, fa, fx) & b) | (IcDoRRop(dst, ba, bx) & ~b)

#define IcStippleRRopMask(dst, b, fa, fx, ba, bx, m) \
    (IcDoMaskRRop(dst, fa, fx, m) & (b)) | (IcDoMaskRRop(dst, ba, bx, m) & ~(b))
						       
#define IcDoLeftMaskByteStippleRRop(dst, b, fa, fx, ba, bx, lb, l) { \
    pixman_bits_t  __xor = ((fx) & (b)) | ((bx) & ~(b)); \
    IcDoLeftMaskByteRRop(dst, lb, l, ((fa) & (b)) | ((ba) & ~(b)), __xor); \
}

#define IcDoRightMaskByteStippleRRop(dst, b, fa, fx, ba, bx, rb, r) { \
    pixman_bits_t  __xor = ((fx) & (b)) | ((bx) & ~(b)); \
    IcDoRightMaskByteRRop(dst, rb, r, ((fa) & (b)) | ((ba) & ~(b)), __xor); \
}

#define IcOpaqueStipple(b, fg, bg) (((fg) & (b)) | ((bg) & ~(b)))
    
/*
 * Compute rop for using tile code for 1-bit dest stipples; modifies
 * existing rop to flip depending on pixel values
 */
#define IcStipple1RopPick(alu,b)    (((alu) >> (2 - (((b) & 1) << 1))) & 3)

#define IcOpaqueStipple1Rop(alu,fg,bg)    (IcStipple1RopPick(alu,fg) | \
					   (IcStipple1RopPick(alu,bg) << 2))

#define IcStipple1Rop(alu,fg)	    (IcStipple1RopPick(alu,fg) | 4)

#endif
