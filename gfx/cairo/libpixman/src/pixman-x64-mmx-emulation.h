#ifndef MMX_X64_H_INCLUDED
#define MMX_X64_H_INCLUDED

/* Implementation of x64 MMX substitition functions, before
 * pixman is reimplemented not to use __m64 type on Visual C++
 *
 * Copyright (C)2009 by George Yohng
 * Released in public domain.
 */

#include <intrin.h>

#define M64C(a) (*(const __m64 *)(&a))
#define M64U(a) (*(const unsigned long long *)(&a))

__inline __m64
_m_from_int (int a)
{
    long long i64 = a;

    return M64C (i64);
}

__inline __m64
_mm_setzero_si64 ()
{
    long long i64 = 0;

    return M64C (i64);
}

__inline __m64
_mm_set_pi32 (int i1,   int i0)
{
    unsigned long long i64 = ((unsigned)i0) + (((unsigned long long)(unsigned)i1) << 32);

    return M64C (i64);
}

__inline void
_m_empty ()
{
}

__inline __m64
_mm_set1_pi16 (short w)
{
    unsigned long long i64 = ((unsigned long long)(unsigned short)(w)) * 0x0001000100010001ULL;

    return M64C (i64);
}

__inline int
_m_to_int (__m64 m)
{
    return m.m64_i32[0];
}

__inline __m64
_mm_movepi64_pi64 (__m128i a)
{
    return M64C (a.m128i_i64[0]);
}

__inline __m64
_m_pand (__m64 a, __m64 b)
{
    unsigned long long i64 = M64U (a) & M64U (b);

    return M64C (i64);
}

__inline __m64
_m_por (__m64 a, __m64 b)
{
    unsigned long long i64 = M64U (a) | M64U (b);

    return M64C (i64);
}

__inline __m64
_m_pxor (__m64 a, __m64 b)
{
    unsigned long long i64 = M64U (a) ^ M64U (b);

    return M64C (i64);
}

__inline __m64
_m_pmulhuw (__m64 a, __m64 b)        /* unoptimized */
{
    unsigned short d[4] =
    {
	(unsigned short)((((unsigned)a.m64_u16[0]) * b.m64_u16[0]) >> 16),
	(unsigned short)((((unsigned)a.m64_u16[1]) * b.m64_u16[1]) >> 16),
	(unsigned short)((((unsigned)a.m64_u16[2]) * b.m64_u16[2]) >> 16),
	(unsigned short)((((unsigned)a.m64_u16[3]) * b.m64_u16[3]) >> 16)
    };

    return M64C (d[0]);
}

__inline __m64
_m_pmullw2 (__m64 a, __m64 b)        /* unoptimized */
{
    unsigned short d[4] =
    {
	(unsigned short)((((unsigned)a.m64_u16[0]) * b.m64_u16[0])),
	(unsigned short)((((unsigned)a.m64_u16[1]) * b.m64_u16[1])),
	(unsigned short)((((unsigned)a.m64_u16[2]) * b.m64_u16[2])),
	(unsigned short)((((unsigned)a.m64_u16[3]) * b.m64_u16[3]))
    };

    return M64C (d[0]);
}

__inline __m64
_m_pmullw (__m64 a, __m64 b)        /* unoptimized */
{
    unsigned long long x =
	((unsigned long long)(unsigned short)((((unsigned)a.m64_u16[0]) * b.m64_u16[0])))  +
	(((unsigned long long)(unsigned short)((((unsigned)a.m64_u16[1]) * b.m64_u16[1]))) << 16)  +
	(((unsigned long long)(unsigned short)((((unsigned)a.m64_u16[2]) * b.m64_u16[2]))) << 32)  +
	(((unsigned long long)(unsigned short)((((unsigned)a.m64_u16[3]) * b.m64_u16[3]))) << 48);

    return M64C (x);
}

__inline __m64
_m_paddusb (__m64 a, __m64 b)        /* unoptimized */
{
    unsigned long long x = (M64U (a) & 0x00FF00FF00FF00FFULL) +
                           (M64U (b) & 0x00FF00FF00FF00FFULL);

    unsigned long long y = ((M64U (a) >> 8) & 0x00FF00FF00FF00FFULL) +
                           ((M64U (b) >> 8) & 0x00FF00FF00FF00FFULL);

    x |= ((x & 0xFF00FF00FF00FF00ULL) >> 8) * 0xFF;
    y |= ((y & 0xFF00FF00FF00FF00ULL) >> 8) * 0xFF;

    x = (x & 0x00FF00FF00FF00FFULL) | ((y & 0x00FF00FF00FF00FFULL) << 8);

    return M64C (x);
}

__inline __m64
_m_paddusw (__m64 a, __m64 b)        /* unoptimized */
{
    unsigned long long x = (M64U (a) & 0x0000FFFF0000FFFFULL) +
                           (M64U (b) & 0x0000FFFF0000FFFFULL);

    unsigned long long y = ((M64U (a) >> 16) & 0x0000FFFF0000FFFFULL) +
                           ((M64U (b) >> 16) & 0x0000FFFF0000FFFFULL);

    x |= ((x & 0xFFFF0000FFFF0000) >> 16) * 0xFFFF;
    y |= ((y & 0xFFFF0000FFFF0000) >> 16) * 0xFFFF;

    x = (x & 0x0000FFFF0000FFFFULL) | ((y & 0x0000FFFF0000FFFFULL) << 16);

    return M64C (x);
}

__inline __m64
_m_pshufw (__m64 a, int n)         /* unoptimized */
{
    unsigned short d[4] =
    {
	a.m64_u16[n & 3],
	a.m64_u16[(n >> 2) & 3],
	a.m64_u16[(n >> 4) & 3],
	a.m64_u16[(n >> 6) & 3]
    };

    return M64C (d[0]);
}

__inline unsigned char
sat16 (unsigned short d)
{
    if (d > 0xFF) return 0xFF;
    else return d & 0xFF;
}

__inline __m64
_m_packuswb (__m64 m1, __m64 m2)          /* unoptimized */
{
    unsigned char d[8] =
    {
	sat16 (m1.m64_u16[0]),
	sat16 (m1.m64_u16[1]),
	sat16 (m1.m64_u16[2]),
	sat16 (m1.m64_u16[3]),
	sat16 (m2.m64_u16[0]),
	sat16 (m2.m64_u16[1]),
	sat16 (m2.m64_u16[2]),
	sat16 (m2.m64_u16[3])
    };

    return M64C (d[0]);
}

__inline __m64 _m_punpcklbw (__m64 m1, __m64 m2)          /* unoptimized */
{
    unsigned char d[8] =
    {
	m1.m64_u8[0],
	m2.m64_u8[0],
	m1.m64_u8[1],
	m2.m64_u8[1],
	m1.m64_u8[2],
	m2.m64_u8[2],
	m1.m64_u8[3],
	m2.m64_u8[3],
    };

    return M64C (d[0]);
}

__inline __m64 _m_punpckhbw (__m64 m1, __m64 m2)          /* unoptimized */
{
    unsigned char d[8] =
    {
	m1.m64_u8[4],
	m2.m64_u8[4],
	m1.m64_u8[5],
	m2.m64_u8[5],
	m1.m64_u8[6],
	m2.m64_u8[6],
	m1.m64_u8[7],
	m2.m64_u8[7],
    };

    return M64C (d[0]);
}

__inline __m64 _m_psrlwi (__m64 a, int n)       /* unoptimized */
{
    unsigned short d[4] =
    {
	a.m64_u16[0] >> n,
	a.m64_u16[1] >> n,
	a.m64_u16[2] >> n,
	a.m64_u16[3] >> n
    };

    return M64C (d[0]);
}

__inline __m64 _m_psrlqi (__m64 m, int n)
{
    unsigned long long x = M64U (m) >> n;

    return M64C (x);
}

__inline __m64 _m_psllqi (__m64 m, int n)
{
    unsigned long long x = M64U (m) << n;

    return M64C (x);
}

#endif /* MMX_X64_H_INCLUDED */
