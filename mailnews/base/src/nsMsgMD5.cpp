/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/*
 * MD5 digest implementation
 * 
 * contributed by mrsam@geocities.com
 *
 */

/* for endian-ness */
#include "prtypes.h"

#include "nsMsgMD5.h"

#define	MD5_BYTE	unsigned char
#define	MD5_WORD	unsigned int

typedef	union md5_endian {
#ifdef IS_LITTLE_ENDIAN
	MD5_WORD	m_word;
	struct	{
		MD5_BYTE m_0, m_1, m_2, m_3;
		}	m_bytes;
#endif
#ifdef IS_BIG_ENDIAN
	MD5_WORD	m_word;
	struct	{
		MD5_BYTE m_3, m_2, m_1, m_0;
		}	m_bytes;
#endif
	} ;

static const MD5_BYTE	*m_msg;
static MD5_WORD	m_msglen;
static MD5_WORD	m_msgpaddedlen;
static MD5_BYTE	m_pad[72];
static MD5_BYTE	m_digest[16];

#define	MD5_MSGBYTE(n)	((MD5_BYTE)((n) < m_msglen?m_msg[n]:m_pad[n-m_msglen]))

inline void MD5_MSGWORD(MD5_WORD &n, MD5_WORD i)
{
union	md5_endian e;

	i *= 4;
	e.m_bytes.m_0=MD5_MSGBYTE(i); ++i;
	e.m_bytes.m_1=MD5_MSGBYTE(i); ++i;
	e.m_bytes.m_2=MD5_MSGBYTE(i); ++i;
	e.m_bytes.m_3=MD5_MSGBYTE(i);
	n=e.m_word;
}

inline MD5_WORD	MD5_ROL(MD5_WORD w, int n)
	{
		return ( w << n | ( (w) >> (32-n) ) );
	}

static	MD5_WORD	T[64]={
0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,
0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
0xd62f105d, 0x2441453, 0xd8a1e681, 0xe7d3fbc8,
0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x4881d05,
0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,
0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391};

const void *nsMsgMD5Digest(const void *msg, unsigned int len)
{
MD5_WORD	i,j;
union md5_endian	e;
MD5_WORD	hilen, lolen;
MD5_BYTE	padlen[8];

	m_msg=(const MD5_BYTE *)msg;
	m_msglen=len;
	m_msgpaddedlen = len+72;
	m_msgpaddedlen &= ~63;
	for (i=0; i<72; i++)	m_pad[i]=0;
	m_pad[0]=0x80;

	lolen=len << 3;
	hilen=len >> 29;

	e.m_word=lolen;
	padlen[0]=e.m_bytes.m_0;
	padlen[1]=e.m_bytes.m_1;
	padlen[2]=e.m_bytes.m_2;
	padlen[3]=e.m_bytes.m_3;
	e.m_word=hilen;
	padlen[4]=e.m_bytes.m_0;
	padlen[5]=e.m_bytes.m_1;
	padlen[6]=e.m_bytes.m_2;
	padlen[7]=e.m_bytes.m_3;

	memcpy( &m_pad[m_msgpaddedlen - m_msglen - 8], padlen, 8);

MD5_WORD	A=0x67452301;
MD5_WORD	B=0xefcdab89;
MD5_WORD	C=0x98badcfe;
MD5_WORD	D=0x10325476;

#define	F(X,Y,Z)	( ((X) & (Y)) | ( (~(X)) & (Z)))
#define	G(X,Y,Z)	( ((X) & (Z)) | ( (Y) & (~(Z))))
#define	H(X,Y,Z)	( (X) ^ (Y) ^ (Z) )
#define	I(X,Y,Z)	( (Y) ^ ( (X) | (~(Z))))
 
MD5_WORD	nwords= m_msgpaddedlen / 4, k=0;
MD5_WORD	x[16];

	for (i=0; i<nwords; i += 16)
	{
		for (j=0; j<16; j++)
		{
			MD5_MSGWORD(x[j],k);
			++k;
		}

MD5_WORD	AA=A, BB=B, CC=C, DD=D;

#define	ROUND1(a,b,c,d,k,s,i)	\
	a = b + MD5_ROL((a + F(b,c,d) + x[k] + T[i]),s)

		ROUND1(A,B,C,D,0,7,0);
		ROUND1(D,A,B,C,1,12,1);
		ROUND1(C,D,A,B,2,17,2);
		ROUND1(B,C,D,A,3,22,3);
		ROUND1(A,B,C,D,4,7,4);
		ROUND1(D,A,B,C,5,12,5);
		ROUND1(C,D,A,B,6,17,6);
		ROUND1(B,C,D,A,7,22,7);
		ROUND1(A,B,C,D,8,7,8);
		ROUND1(D,A,B,C,9,12,9);
		ROUND1(C,D,A,B,10,17,10);
		ROUND1(B,C,D,A,11,22,11);
		ROUND1(A,B,C,D,12,7,12);
		ROUND1(D,A,B,C,13,12,13);
		ROUND1(C,D,A,B,14,17,14);
		ROUND1(B,C,D,A,15,22,15);
 
#define	ROUND2(a,b,c,d,k,s,i)	\
	a = b + MD5_ROL((a + G(b,c,d) + x[k] + T[i]),s)

		ROUND2(A,B,C,D,1,5,16);
		ROUND2(D,A,B,C,6,9,17);
		ROUND2(C,D,A,B,11,14,18);
		ROUND2(B,C,D,A,0,20,19);
		ROUND2(A,B,C,D,5,5,20);
		ROUND2(D,A,B,C,10,9,21);
		ROUND2(C,D,A,B,15,14,22);
		ROUND2(B,C,D,A,4,20,23);
		ROUND2(A,B,C,D,9,5,24);
		ROUND2(D,A,B,C,14,9,25);
		ROUND2(C,D,A,B,3,14,26);
		ROUND2(B,C,D,A,8,20,27);
		ROUND2(A,B,C,D,13,5,28);
		ROUND2(D,A,B,C,2,9,29);
		ROUND2(C,D,A,B,7,14,30);
		ROUND2(B,C,D,A,12,20,31);

#define	ROUND3(a,b,c,d,k,s,i)	\
	a = b + MD5_ROL((a + H(b,c,d) + x[k] + T[i]),s)

		ROUND3(A,B,C,D,5,4,32);
		ROUND3(D,A,B,C,8,11,33);
		ROUND3(C,D,A,B,11,16,34);
		ROUND3(B,C,D,A,14,23,35);
		ROUND3(A,B,C,D,1,4,36);
		ROUND3(D,A,B,C,4,11,37);
		ROUND3(C,D,A,B,7,16,38);
		ROUND3(B,C,D,A,10,23,39);
		ROUND3(A,B,C,D,13,4,40);
		ROUND3(D,A,B,C,0,11,41);
		ROUND3(C,D,A,B,3,16,42);
		ROUND3(B,C,D,A,6,23,43);
		ROUND3(A,B,C,D,9,4,44);
		ROUND3(D,A,B,C,12,11,45);
		ROUND3(C,D,A,B,15,16,46);
		ROUND3(B,C,D,A,2,23,47);

#define	ROUND4(a,b,c,d,k,s,i)	\
	a = b + MD5_ROL((a + I(b,c,d) + x[k] + T[i]),s)

		ROUND4(A,B,C,D,0,6,48);
		ROUND4(D,A,B,C,7,10,49);
		ROUND4(C,D,A,B,14,15,50);
		ROUND4(B,C,D,A,5,21,51);
		ROUND4(A,B,C,D,12,6,52);
		ROUND4(D,A,B,C,3,10,53);
		ROUND4(C,D,A,B,10,15,54);
		ROUND4(B,C,D,A,1,21,55);
		ROUND4(A,B,C,D,8,6,56);
		ROUND4(D,A,B,C,15,10,57);
		ROUND4(C,D,A,B,6,15,58);
		ROUND4(B,C,D,A,13,21,59);
		ROUND4(A,B,C,D,4,6,60);
		ROUND4(D,A,B,C,11,10,61);
		ROUND4(C,D,A,B,2,15,62);
		ROUND4(B,C,D,A,9,21,63);

		A += AA;
		B += BB;
		C += CC;
		D += DD;
	}

union md5_endian	ea, eb, ec, ed;

	ea.m_word=A;
	eb.m_word=B;
	ec.m_word=C;
	ed.m_word=D;

	m_digest[0]=ea.m_bytes.m_0;
	m_digest[1]=ea.m_bytes.m_1;
	m_digest[2]=ea.m_bytes.m_2;
	m_digest[3]=ea.m_bytes.m_3;
	m_digest[4]=eb.m_bytes.m_0;
	m_digest[5]=eb.m_bytes.m_1;
	m_digest[6]=eb.m_bytes.m_2;
	m_digest[7]=eb.m_bytes.m_3;
	m_digest[8]=ec.m_bytes.m_0;
	m_digest[9]=ec.m_bytes.m_1;
	m_digest[10]=ec.m_bytes.m_2;
	m_digest[11]=ec.m_bytes.m_3;
	m_digest[12]=ed.m_bytes.m_0;
	m_digest[13]=ed.m_bytes.m_1;
	m_digest[14]=ed.m_bytes.m_2;
	m_digest[15]=ed.m_bytes.m_3;
	return (m_digest);
}
