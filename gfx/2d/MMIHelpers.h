/*
 ============================================================================
 Name        : MMIHelpers.h
 Author      : Heiher <r@hev.cc>
 Version     : 0.0.1
 Copyright   : Copyright (c) 2015 everyone.
 Description : The helpers for x86 SSE to Loongson MMI.
 ============================================================================
 */

#ifndef __MMI_HELPERS_H__
#define __MMI_HELPERS_H__

#define __mm_packxxxx(_f, _D, _d, _s, _t)                   \
	#_f" %["#_t"], %["#_d"h], %["#_s"h] \n\t"           \
	#_f" %["#_D"l], %["#_d"l], %["#_s"l] \n\t"          \
	"punpckhwd %["#_D"h], %["#_D"l], %["#_t"] \n\t"     \
	"punpcklwd %["#_D"l], %["#_D"l], %["#_t"] \n\t"

#define _mm_or(_D, _d, _s)                                  \
	"or %["#_D"h], %["#_d"h], %["#_s"h] \n\t"           \
	"or %["#_D"l], %["#_d"l], %["#_s"l] \n\t"

#define _mm_xor(_D, _d, _s)                                 \
	"xor %["#_D"h], %["#_d"h], %["#_s"h] \n\t"          \
	"xor %["#_D"l], %["#_d"l], %["#_s"l] \n\t"

#define _mm_and(_D, _d, _s)                                 \
	"and %["#_D"h], %["#_d"h], %["#_s"h] \n\t"          \
	"and %["#_D"l], %["#_d"l], %["#_s"l] \n\t"

/* SSE: pandn */
#define _mm_pandn(_D, _d, _s)                               \
	"pandn %["#_D"h], %["#_d"h], %["#_s"h] \n\t"        \
	"pandn %["#_D"l], %["#_d"l], %["#_s"l] \n\t"

/* SSE: pshuflw */
#define _mm_pshuflh(_D, _d, _s)                             \
	"mov.d %["#_D"h], %["#_d"h] \n\t"                   \
	"pshufh %["#_D"l], %["#_d"l], %["#_s"] \n\t"

/* SSE: psllw (bits) */
#define _mm_psllh(_D, _d, _s)                               \
	"psllh %["#_D"h], %["#_d"h], %["#_s"] \n\t"         \
	"psllh %["#_D"l], %["#_d"l], %["#_s"] \n\t"

/* SSE: pslld (bits) */
#define _mm_psllw(_D, _d, _s)                               \
	"psllw %["#_D"h], %["#_d"h], %["#_s"] \n\t"         \
	"psllw %["#_D"l], %["#_d"l], %["#_s"] \n\t"

/* SSE: psllq (bits) */
#define _mm_pslld(_D, _d, _s)                               \
	"dsll %["#_D"h], %["#_d"h], %["#_s"] \n\t"          \
	"dsll %["#_D"l], %["#_d"l], %["#_s"] \n\t"

/* SSE: pslldq (bytes) */
#define _mm_psllq(_D, _d, _s, _s64, _tf)                    \
	"subu %["#_tf"], %["#_s64"], %["#_s"] \n\t"         \
	"dsrl %["#_tf"], %["#_d"l], %["#_tf"] \n\t"         \
	"dsll %["#_D"h], %["#_d"h], %["#_s"] \n\t"          \
	"dsll %["#_D"l], %["#_d"l], %["#_s"] \n\t"          \
	"or %["#_D"h], %["#_D"h], %["#_tf"] \n\t"

/* SSE: psrlw (bits) */
#define _mm_psrlh(_D, _d, _s)                               \
	"psrlh %["#_D"h], %["#_d"h], %["#_s"] \n\t"         \
	"psrlh %["#_D"l], %["#_d"l], %["#_s"] \n\t"

/* SSE: psrld (bits) */
#define _mm_psrlw(_D, _d, _s)                               \
	"psrlw %["#_D"h], %["#_d"h], %["#_s"] \n\t"         \
	"psrlw %["#_D"l], %["#_d"l], %["#_s"] \n\t"

/* SSE: psrlq (bits) */
#define _mm_psrld(_D, _d, _s)                               \
	"dsrl %["#_D"h], %["#_d"h], %["#_s"] \n\t"          \
	"dsrl %["#_D"l], %["#_d"l], %["#_s"] \n\t"

/* SSE: psrldq (bytes) */
#define _mm_psrlq(_D, _d, _s, _s64, _tf)                    \
	"subu %["#_tf"], %["#_s64"], %["#_s"] \n\t"         \
	"dsll %["#_tf"], %["#_d"h], %["#_tf"] \n\t"         \
	"dsrl %["#_D"h], %["#_d"h], %["#_s"] \n\t"          \
	"dsrl %["#_D"l], %["#_d"l], %["#_s"] \n\t"          \
	"or %["#_D"l], %["#_D"l], %["#_tf"] \n\t"

/* SSE: psrad */
#define _mm_psraw(_D, _d, _s)                               \
	"psraw %["#_D"h], %["#_d"h], %["#_s"] \n\t"         \
	"psraw %["#_D"l], %["#_d"l], %["#_s"] \n\t"

/* SSE: paddb */
#define _mm_paddb(_D, _d, _s)                               \
	"paddb %["#_D"h], %["#_d"h], %["#_s"h] \n\t"        \
	"paddb %["#_D"l], %["#_d"l], %["#_s"l] \n\t"

/* SSE: paddw */
#define _mm_paddh(_D, _d, _s)                               \
	"paddh %["#_D"h], %["#_d"h], %["#_s"h] \n\t"        \
	"paddh %["#_D"l], %["#_d"l], %["#_s"l] \n\t"

/* SSE: paddd */
#define _mm_paddw(_D, _d, _s)                               \
	"paddw %["#_D"h], %["#_d"h], %["#_s"h] \n\t"        \
	"paddw %["#_D"l], %["#_d"l], %["#_s"l] \n\t"

/* SSE: paddq */
#define _mm_paddd(_D, _d, _s)                               \
	"dadd %["#_D"h], %["#_d"h], %["#_s"h] \n\t"         \
	"dadd %["#_D"l], %["#_d"l], %["#_s"l] \n\t"

/* SSE: psubw */
#define _mm_psubh(_D, _d, _s)                               \
	"psubh %["#_D"h], %["#_d"h], %["#_s"h] \n\t"        \
	"psubh %["#_D"l], %["#_d"l], %["#_s"l] \n\t"

/* SSE: psubd */
#define _mm_psubw(_D, _d, _s)                               \
	"psubw %["#_D"h], %["#_d"h], %["#_s"h] \n\t"        \
	"psubw %["#_D"l], %["#_d"l], %["#_s"l] \n\t"

/* SSE: pmaxub */
#define _mm_pmaxub(_D, _d, _s)                              \
	"pmaxub %["#_D"h], %["#_d"h], %["#_s"h] \n\t"       \
	"pmaxub %["#_D"l], %["#_d"l], %["#_s"l] \n\t"

/* SSE: pmullw */
#define _mm_pmullh(_D, _d, _s)                              \
	"pmullh %["#_D"h], %["#_d"h], %["#_s"h] \n\t"       \
	"pmullh %["#_D"l], %["#_d"l], %["#_s"l] \n\t"

/* SSE: pmulhw */
#define _mm_pmulhh(_D, _d, _s)                              \
	"pmulhh %["#_D"h], %["#_d"h], %["#_s"h] \n\t"       \
	"pmulhh %["#_D"l], %["#_d"l], %["#_s"l] \n\t"

/* SSE: pmuludq */
#define _mm_pmuluw(_D, _d, _s)                              \
	"pmuluw %["#_D"h], %["#_d"h], %["#_s"h] \n\t"       \
	"pmuluw %["#_D"l], %["#_d"l], %["#_s"l] \n\t"

/* SSE: packsswb */
#define _mm_packsshb(_D, _d, _s, _t)			    \
	__mm_packxxxx(packsshb, _D, _d, _s, _t)

/* SSE: packssdw */
#define _mm_packsswh(_D, _d, _s, _t)			    \
	__mm_packxxxx(packsswh, _D, _d, _s, _t)

/* SSE: packuswb */
#define _mm_packushb(_D, _d, _s, _t)			    \
	__mm_packxxxx(packushb, _D, _d, _s, _t)

/* SSE: punpcklbw */
#define _mm_punpcklbh(_D, _d, _s)                           \
	"punpckhbh %["#_D"h], %["#_d"l], %["#_s"l] \n\t"    \
	"punpcklbh %["#_D"l], %["#_d"l], %["#_s"l] \n\t"

/* SSE: punpcklwd */
#define _mm_punpcklhw(_D, _d, _s)                           \
	"punpckhhw %["#_D"h], %["#_d"l], %["#_s"l] \n\t"    \
	"punpcklhw %["#_D"l], %["#_d"l], %["#_s"l] \n\t"

/* SSE: punpckldq */
#define _mm_punpcklwd(_D, _d, _s)                           \
	"punpckhwd %["#_D"h], %["#_d"l], %["#_s"l] \n\t"    \
	"punpcklwd %["#_D"l], %["#_d"l], %["#_s"l] \n\t"

/* SSE: punpcklqdq */
#define _mm_punpckldq(_D, _d, _s)                           \
	"mov.d %["#_D"h], %["#_s"l] \n\t"                   \
	"mov.d %["#_D"l], %["#_d"l] \n\t"

/* SSE: punpckhbw */
#define _mm_punpckhbh(_D, _d, _s)                           \
	"punpcklbh %["#_D"l], %["#_d"h], %["#_s"h] \n\t"    \
	"punpckhbh %["#_D"h], %["#_d"h], %["#_s"h] \n\t"

/* SSE: punpckhwd */
#define _mm_punpckhhw(_D, _d, _s)                           \
	"punpcklhw %["#_D"l], %["#_d"h], %["#_s"h] \n\t"    \
	"punpckhhw %["#_D"h], %["#_d"h], %["#_s"h] \n\t"

/* SSE: punpckhdq */
#define _mm_punpckhwd(_D, _d, _s)                           \
	"punpcklwd %["#_D"l], %["#_d"h], %["#_s"h] \n\t"    \
	"punpckhwd %["#_D"h], %["#_d"h], %["#_s"h] \n\t"

/* SSE: punpckhqdq */
#define _mm_punpckhdq(_D, _d, _s)                           \
	"mov.d %["#_D"l], %["#_d"h] \n\t"                   \
	"mov.d %["#_D"h], %["#_s"h] \n\t"

#endif /* __MMI_HELPERS_H__ */

