/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
/*	intlpriv.h	*/

#ifndef _INTLPRIV_H_
#define _INTLPRIV_H_

#include "xp.h"
#include "intl_csi.h"
#include "libi18n.h"


#define UNCVTBUF_SIZE 8    /* At least: longest ESC Seq + Char Bytes - 1 */
#define DOC_CSID_KNOWN(x) (((x) != CS_DEFAULT) && ((x) != CS_UNKNOWN))

/* Some constants for UCS-2 detection */
#define BYTE_ORDER_MARK               0xFEFF
#define NEEDS_SWAP_MARK               0xFFFE    

/*
 * UTF8 defines and macros
 */
#define ONE_OCTET_BASE			0x00	/* 0xxxxxxx */
#define ONE_OCTET_MASK			0x7F	/* x1111111 */
#define CONTINUING_OCTET_BASE	0x80	/* 10xxxxxx */
#define CONTINUING_OCTET_MASK	0x3F	/* 00111111 */
#define TWO_OCTET_BASE			0xC0	/* 110xxxxx */
#define TWO_OCTET_MASK			0x1F	/* 00011111 */
#define THREE_OCTET_BASE		0xE0	/* 1110xxxx */
#define THREE_OCTET_MASK		0x0F	/* 00001111 */
#define FOUR_OCTET_BASE			0xF0	/* 11110xxx */
#define FOUR_OCTET_MASK			0x07	/* 00000111 */
#define FIVE_OCTET_BASE			0xF8	/* 111110xx */
#define FIVE_OCTET_MASK			0x03	/* 00000011 */
#define SIX_OCTET_BASE			0xFC	/* 1111110x */
#define SIX_OCTET_MASK			0x01	/* 00000001 */

#define IS_UTF8_1ST_OF_1(x)	(( (x)&~ONE_OCTET_MASK  ) == ONE_OCTET_BASE)
#define IS_UTF8_1ST_OF_2(x)	(( (x)&~TWO_OCTET_MASK  ) == TWO_OCTET_BASE)
#define IS_UTF8_1ST_OF_3(x)	(( (x)&~THREE_OCTET_MASK) == THREE_OCTET_BASE)
#define IS_UTF8_1ST_OF_4(x)	(( (x)&~FOUR_OCTET_MASK ) == FOUR_OCTET_BASE)
#define IS_UTF8_1ST_OF_5(x)	(( (x)&~FIVE_OCTET_MASK ) == FIVE_OCTET_BASE)
#define IS_UTF8_1ST_OF_6(x)	(( (x)&~SIX_OCTET_MASK  ) == SIX_OCTET_BASE)
#define IS_UTF8_2ND_THRU_6TH(x) \
					(( (x)&~CONTINUING_OCTET_MASK  ) == CONTINUING_OCTET_BASE)
#define IS_UTF8_1ST_OF_UCS2(x) \
			IS_UTF8_1ST_OF_1(x) \
			|| IS_UTF8_1ST_OF_2(x) \
			|| IS_UTF8_1ST_OF_3(x)



/* Some constants for UCS-2 detection */
#define BYTE_ORDER_MARK               0xFEFF
#define NEEDS_SWAP_MARK               0xFFFE

/* exported functions from unicvt.c */

MODULE_PRIVATE UNICVTAPI unsigned char *mz_ucs2utf8(CCCDataObject, const unsigned char *s, int32 l);
MODULE_PRIVATE UNICVTAPI unsigned char *mz_ucs2utf7(CCCDataObject, const unsigned char *s, int32 l);
MODULE_PRIVATE UNICVTAPI unsigned char *mz_utf82ucs(CCCDataObject, const unsigned char *s, int32 l);
MODULE_PRIVATE UNICVTAPI unsigned char *mz_utf82ucsswap(CCCDataObject, const unsigned char *s, int32 l);
MODULE_PRIVATE UNICVTAPI unsigned char *mz_utf72utf8(CCCDataObject, const unsigned char *s, int32 l);
MODULE_PRIVATE UNICVTAPI unsigned char *mz_utf82utf7(CCCDataObject, const unsigned char *s, int32 l);
MODULE_PRIVATE UNICVTAPI unsigned char *mz_imap4utf72utf8(CCCDataObject, const unsigned char *s, int32 l);
MODULE_PRIVATE UNICVTAPI unsigned char *mz_utf82imap4utf7(CCCDataObject, const unsigned char *s, int32 l);
MODULE_PRIVATE UNICVTAPI int16 utf8_to_ucs2_char(const unsigned char *utf8p, int16 buflen, uint16 *ucs2p);
MODULE_PRIVATE UNICVTAPI int32 utf8_to_ucs2_buffer(const unsigned char *utf8p, int16 utf8len, int *parsed_cnt, int *invalid_cnt, uint16 *ucs2p, int32 ucs2len);

 
					/* values for ASCII chars	*/
#define ESC		0x1B		/* ESC character		*/ 
#define NL		0x0A		/* newline			*/ 
#undef CR
#define CR		0x0D		/* carriage return		*/
#define DOLLAR		0x24		/* carriage return		*/

					/* values for EUC shift chars	*/
#define SS2		0x8E		/* Single Shift 2		*/
#define SS3		0x8F		/* Single Shift 3		*/
 
		 			/* JIS encoding mode flags	*/
#define JIS_Roman	0
#define JIS_208_83	1
#define JIS_HalfKana	2
#define JIS_212_90	3

/* I am using low nibble for the ESC flag and the next high nibble for Shift */
#define KSC_5601_87	0x04

/* Default state is SHIFT_OUT when we begin.
 * So SHIFT_IN should have value 0x0- */
#define SHIFT_IN	0x00
#define SHIFT_OUT	0x10

/* The actual values to be output for SHIFTING */
#define SO		0x0e
#define SI		0x0f

/* Some masks for computation */
#define ESC_MASK	0x0F
#define SHIFT_MASK	0xF0
 

/*
 * Shift JIS Encoding
 *				1st Byte Range	2nd Byte Range
 * ASCII/JIS-Roman		0x21-0x7F	n/a
 * 2-Byte Char(low range)	0x81-0x9F	0x40-0x7E, 0x80-0xFC
 * Half-width space(non-std)	0xA0		n/a
 * Half-width katakana		0xA1-0xDF	n/a
 * 2-Byte Char(high range)	0xE0-0xEF	0x40-0x7E, 0x80-0xFC
 * User Defined(non-std)	0xF0-0xFC	0x40-0x7E, 0x80-0xFC
 *
 * JIS Encoding
 *				1st Byte Range	2nd Byte Range
 * ASCII/JIS-Roman		0x21-0x7E	n/a
 * Half-width katakana(non-std)	0x21-0x5F	n/a
 * 2-Byte Char			0x21-7E		0x21-7E
 *
 * Japanese EUC Encoding
 *				1st Byte Range	2nd Byte Range	3rd Byte Range
 * ASCII/JIS-Roman		0x21-0x7E	n/a		n/a
 * JIS X 0208-1990		0xA0-0xFF	0xA0-0xFF	n/a
 * Half-width katakana		SS2		0xA0-0xFF	n/a
 * JIS X 0212-1990		SS3		0xA0-0xFF	0xA0-0xFF
 *
 *
 *	List of ISO2022-INT Escape Sequences:
 *
 *		SUPPORTED:
 *		ASCII			ESC ( B		G0
 *		JIS X 0201-Roman	ESC ( J		G0
 *		Half-width Katakana	ESC ( I
 *		JIS X 0208-1978		ESC $ @		G0
 *		JIS X 0208-1983		ESC $ B		G0
 *		JIS X 0212-1990		ESC $ ( D	G0	(to EUC only)
 *		ISO8859-1		ESC - A		G1
 *
 *		UNSUPPORTED:
 *		GB 2312-80		ESC $ A		G0
 *		KS C 5601-1987		ESC $ ) C	G1
 *		CNS 11643-1986-1	ESC $ ( G	G0
 *		CNS 11643-1986-2	ESC $ ( H	G0
 *		ISO8859-7(Greek)	ESC - F		G1
 *
 * Added right parens: ))))) to balance editors' showmatch...
 */

 
	/* JIS-Roman mode enabled by 3 char ESC sequence: <ESC> ( J	*/
#define InsRoman_ESC(cp, obj) {				\
 	INTL_SetCCCJismode(obj, JIS_Roman);			\
 	*cp++ = ESC;					\
 	*cp++ = '(';					\
 	*cp++ = 'J';					\
}
	/* ASCII mode enabled by 3 char ESC sequence: <ESC> ( B	*/
#define InsASCII_ESC(cp, obj) {				\
 	INTL_SetCCCJismode(obj, JIS_Roman);			\
 	*cp++ = ESC;					\
 	*cp++ = '(';					\
 	*cp++ = 'B';					\
}
	/* JIS x208-1983 mode enabled by 3 char ESC sequence: <ESC> $ B	*/
#define Ins208_83_ESC(cp, obj) {			\
	INTL_SetCCCJismode(obj, JIS_208_83);			\
 	*cp++ = ESC;					\
 	*cp++ = '$';					\
 	*cp++ = 'B';					\
}
	/* JIS Half-width katakana mode enabled by 3 char ESC seq.: ESC ( I */
#define InsHalfKana_ESC(cp, obj) {			\
 	INTL_SetCCCJismode(obj, JIS_HalfKana);			\
 	*cp++ = ESC;					\
 	*cp++ = '(';					\
 	*cp++ = 'I';					\
}
	/* JIS x212-1990 mode enabled by 4 char ESC sequence: <ESC> $ ( D */
#define Ins212_90_ESC(cp, obj) {			\
	INTL_SetCCCJismode(obj, JIS_212_90);			\
 	*cp++ = ESC;					\
 	*cp++ = '$';					\
 	*cp++ = '(';					\
 	*cp++ = 'D';					\
}

	/* KSC 5601-1987 mode enabled by 4 char ESC sequence: <ESC> $ ) D */
#define Ins5601_87_ESC(cp, obj) {	\
	INTL_SetCCCJismode(obj, (INTL_GetCCCJismode(obj) & ~ESC_MASK) | KSC_5601_87);	\
 	*cp++ = ESC;							\
 	*cp++ = '$';							\
 	*cp++ = ')';							\
 	*cp++ = 'C';							\
}
#define Ins5601_87_SI(cp, obj)	{					\
	INTL_SetCCCJismode(obj, ((INTL_GetCCCJismode(obj) & ~SHIFT_MASK) | SHIFT_IN));	\
	*cp++ = SI;							\
}
#define Ins5601_87_SO(cp, obj)	{					\
	INTL_SetCCCJismode(obj, ((INTL_GetCCCJismode(obj) & ~SHIFT_MASK) | SHIFT_OUT));	\
	*cp++ = SO;							\
}
#define IsIns5601_87_ESC(obj)	((INTL_GetCCCJismode(obj) & ESC_MASK) == KSC_5601_87)
#define IsIns5601_87_SI(obj)	((INTL_GetCCCJismode(obj) & SHIFT_MASK) == SHIFT_IN)
#define IsIns5601_87_SO(obj)	((INTL_GetCCCJismode(obj) & SHIFT_MASK) == SHIFT_OUT)

	/* Added right parens: )))))) to balance editors' showmatch...	*/

	/* Maximum Length of Escape Sequence and Character Bytes per Encoding */
#define MAX_SJIS	2
#define MAX_EUC		3
#define	MAX_JIS		6



#define MAX_CSNAME	64
typedef struct _csname2id_t {
	unsigned char	cs_name[MAX_CSNAME];
	unsigned char	java_name[MAX_CSNAME];
	int16		cs_id;
	unsigned char	fill[3];
} csname2id_t;

typedef struct {
	int16		from_csid;	/* "from" codeset ID		*/
	int16		to_csid;	/* "to" codeset ID		*/
	int16		autoselect;	/* autoselectable		*/
	CCCFunc		cvtmethod;	/* char. codeset conv method	*/
	int32		cvtflag;	/* conv func dependent flag	*/
} cscvt_t;


#ifdef XP_UNIX
typedef struct {
	int16	win_csid;
	char	*locale;
	char	*fontlist;
} cslocale_t;
#endif /* XP_UNIX */

XP_BEGIN_PROTOS

MODULE_PRIVATE int	      net_1to1CCC(CCCDataObject, const unsigned char *s, int32 l);
MODULE_PRIVATE int	      net_sjis2jis(CCCDataObject,const unsigned char *s,int32 l);
MODULE_PRIVATE int	      net_sjis2euc(CCCDataObject,const unsigned char *s,int32 l);
MODULE_PRIVATE int	      net_euc2jis(CCCDataObject,const unsigned char *s, int32 l);
MODULE_PRIVATE int	      net_euc2sjis(CCCDataObject,const unsigned char *s,int32 l);
MODULE_PRIVATE int	      net_jis2other(CCCDataObject, const unsigned char *s, int32 l);

MODULE_PRIVATE unsigned char *mz_euc2jis(CCCDataObject, const unsigned char *s, int32 l);
MODULE_PRIVATE unsigned char *mz_sjis2euc(CCCDataObject, const unsigned char *s, int32 l);
MODULE_PRIVATE unsigned char *mz_sjis2jis(CCCDataObject, const unsigned char *s, int32 l);
MODULE_PRIVATE unsigned char *One2OneCCC(CCCDataObject, const unsigned char *s, int32 l);
MODULE_PRIVATE unsigned char *mz_euc2sjis(CCCDataObject, const unsigned char *s, int32 l);
MODULE_PRIVATE unsigned char *mz_euckr2iso(CCCDataObject, const unsigned char *s, int32 l);
MODULE_PRIVATE unsigned char *mz_iso2euckr(CCCDataObject, const unsigned char *s, int32 l);
MODULE_PRIVATE unsigned char *mz_b52cns(CCCDataObject, const unsigned char *s, int32 l);
MODULE_PRIVATE unsigned char *mz_cns2b5(CCCDataObject, const unsigned char *s, int32 l);
MODULE_PRIVATE unsigned char *jis2other(CCCDataObject, const unsigned char *s, int32 l);


MODULE_PRIVATE unsigned char *autoJCCC (CCCDataObject, const unsigned char *s, int32 l);
MODULE_PRIVATE unsigned char *autoKCCC (CCCDataObject, const unsigned char *s, int32 l);

MODULE_PRIVATE int16	      intl_detect_JCSID (uint16, const unsigned char *, int32);
MODULE_PRIVATE int16		  intl_detect_KCSID (uint16, const unsigned char *, int32);




void	      FE_fontchange(iDocumentContext window_id, int16  csid);
void          FE_ChangeURLCharset(const char *newCharset);
char **		FE_GetSingleByteTable(int16 from_csid, int16 to_csid, int32 resourceid);
char *		FE_LockTable(char **cvthdl);
void 		FE_FreeSingleByteTable(char **cvthdl);

/* void CCCReportAutoDetect(CCCDataObject obj, uint16 detected_doc_csid); */

unsigned char *ConvHeader(unsigned char *subject);

#ifdef XP_UNIX
int16	      FE_WinCSID(iDocumentContext );
#else /* XP_UNIX */
#define	      FE_WinCSID(window_id) 0
#endif /* XP_UNIX */

int16 *intl_GetFontCharSets(void);


/** 
 * Access a conversion flag for hankaku->zenkaku kana conversion for mail. 
 * 
 * The conversion flag for JIS, set if converting hankaku (1byte) kana to zenkaku (2byte).
 * The flag is needed in order to control the conversion. Kana conversion should be applied
 * only when sending a mail and converters do not know if they are called for mail sending.
 * 
 * @param obj          Character code converter. 
 * @return TRUE if convert to zenkaku (2byte). 
 * @see INTL_SetCCCCvtflag_SendHankakuKana  
 */ 
MODULE_PRIVATE XP_Bool INTL_GetCCCCvtflag_SendHankakuKana(CCCDataObject obj);
/** 
 * Access a conversion flag for hankaku->zenkaku kana conversion for mail. 
 * 
 * The conversion flag for JIS, set if converting hankaku (1byte) kana to zenkaku (2byte).
 * The flag is needed in order to control the conversion. Kana conversion should be applied
 * only when sending a mail and converters do not know if they are called for mail sending.
 * 
 * @param obj          Character code converter. 
 * @see INTL_GetCCCCvtflag_SendHankakuKana  
 */ 
MODULE_PRIVATE void INTL_SetCCCCvtflag_SendHankakuKana(CCCDataObject obj, XP_Bool flag);


XP_END_PROTOS



#define MAXCSIDINTBL	64



#endif /* _INTLPRIV_H_ */
