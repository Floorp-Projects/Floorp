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

#include "pa_parse.h"
#include <stdio.h>

#include "libi18n.h"

#ifdef PROFILE
#pragma profile on
#endif
#define PA_REMAP_128_TO_160_ILLEGAL_NCR 1

#define NOT_USED 0xfffd
static uint16 PA_HackTable[] = {
	NOT_USED,
	NOT_USED,
	0x201a,  /* SINGLE LOW-9 QUOTATION MARK */
	0x0192,  /* LATIN SMALL LETTER F WITH HOOK */
	0x201e,  /* DOUBLE LOW-9 QUOTATION MARK */
	0x2026,  /* HORIZONTAL ELLIPSIS */
	0x2020,  /* DAGGER */
	0x2021,  /* DOUBLE DAGGER */
	0x02c6,  /* MODIFIER LETTER CIRCUMFLEX ACCENT */
	0x2030,  /* PER MILLE SIGN */
	0x0160,  /* LATIN CAPITAL LETTER S WITH CARON */
	0x2039,  /* SINGLE LEFT-POINTING ANGLE QUOTATION MARK */
	0x0152,  /* LATIN CAPITAL LIGATURE OE */
	NOT_USED,
	NOT_USED,
	NOT_USED,
	NOT_USED,
	0x2018,  /* LEFT SINGLE QUOTATION MARK */
	0x2019,  /* RIGHT SINGLE QUOTATION MARK */
	0x201c,  /* LEFT DOUBLE QUOTATION MARK */
	0x201d,  /* RIGHT DOUBLE QUOTATION MARK */
	0x2022,  /* BULLET */
	0x2013,  /* EN DASH */
	0x2014,  /* EM DASH */
	0x02dc,  /* SMALL TILDE */
	0x2122,  /* TRADE MARK SIGN */
	0x0161,  /* LATIN SMALL LETTER S WITH CARON */
	0x203a,  /* SINGLE RIGHT-POINTING ANGLE QUOTATION MARK */
	0x0153,  /* LATIN SMALL LIGATURE OE */
	NOT_USED,
	NOT_USED,
	0x0178   /* LATIN CAPITAL LETTER Y WITH DIAERESIS */
};


typedef struct PA_N2U_struct {
        char* NE;            /* Name Entity, "copy" */
		intn  len;            /* 4, the length of NE */
		int16 unicode;        /* 0x00a4 */
} PA_N2U;                     /* Name->Unicode */

/*  NE		LEN		UNICODE  */
static PA_N2U PA_Name2Unicode[] = {
	{"quot",	4,	0x0022 },
	{"QUOT",	4,	0x0022 },
	{"amp",		3,	0x0026 },
	{"AMP",		3,	0x0026 },
	{"lt",		2,	0x003c },
	{"LT",		2,	0x003c },
	{"gt",		2,	0x003e },
	{"GT",		2,	0x003e },
	{"nbsp",	4,	0x00a0 },
	{"iexcl",	5,	0x00a1 },
	{"cent",	4,	0x00a2 },
	{"pound",	5,	0x00a3 },
	{"curren",	6,	0x00a4 },
	{"yen",		3,	0x00a5 },
	{"brvbar",	6,	0x00a6 },
	{"sect",	4,	0x00a7 },
	{"uml",		3,	0x00a8 },
	{"copy",	4,	0x00a9 },
	{"COPY",	4,	0x00a9 },
	{"ordf",	4,	0x00aa },
	{"laquo",	5,	0x00ab },
	{"not",		3,	0x00ac },
	{"shy",		3,	0x00ad },
	{"reg",		3,	0x00ae },
	{"REG",		3,	0x00ae },
	{"macr",	4,	0x00af },
	{"deg",		3,	0x00b0 },
	{"plusmn",	6,	0x00b1 },
	{"sup2",	4,	0x00b2 },
	{"sup3",	4,	0x00b3 },
	{"acute",	5,	0x00b4 },
	{"micro",	5,	0x00b5 },
	{"para",	4,	0x00b6 },
	{"middot",	6,	0x00b7 },
	{"cedil",	5,	0x00b8 },
	{"sup1",	4,	0x00b9 },
	{"ordm",	4,	0x00ba },
	{"raquo",	5,	0x00bb },
	{"frac14",	6,	0x00bc },
	{"frac12",	6,	0x00bd },
	{"frac34",	6,	0x00be },
	{"iquest",	6,	0x00bf },
	{"Agrave",	6,	0x00c0 },
	{"Aacute",	6,	0x00c1 },
	{"Acirc",	5,	0x00c2 },
	{"Atilde",	6,	0x00c3 },
	{"Auml",	4,	0x00c4 },
	{"Aring",	5,	0x00c5 },
	{"AElig",	5,	0x00c6 },
	{"Ccedil",	6,	0x00c7 },
	{"Egrave",	6,	0x00c8 },
	{"Eacute",	6,	0x00c9 },
	{"Ecirc",	5,	0x00ca },
	{"Euml",	4,	0x00cb },
	{"Igrave",	6,	0x00cc },
	{"Iacute",	6,	0x00cd },
	{"Icirc",	5,	0x00ce },
	{"Iuml",	4,	0x00cf },
	{"ETH",		3,	0x00d0 },
	{"Ntilde",	6,	0x00d1 },
	{"Ograve",	6,	0x00d2 },
	{"Oacute",	6,	0x00d3 },
	{"Ocirc",	5,	0x00d4 },
	{"Otilde",	6,	0x00d5 },
	{"Ouml",	4,	0x00d6 },
	{"times",	5,	0x00d7 },
	{"Oslash",	6,	0x00d8 },
	{"Ugrave",	6,	0x00d9 },
	{"Uacute",	6,	0x00da },
	{"Ucirc",	5,	0x00db },
	{"Uuml",	4,	0x00dc },
	{"Yacute",	6,	0x00dd },
	{"THORN",	5,	0x00de },
	{"szlig",	5,	0x00df },
	{"agrave",	6,	0x00e0 },
	{"aacute",	6,	0x00e1 },
	{"acirc",	5,	0x00e2 },
	{"atilde",	6,	0x00e3 },
	{"auml",	4,	0x00e4 },
	{"aring",	5,	0x00e5 },
	{"aelig",	5,	0x00e6 },
	{"ccedil",	6,	0x00e7 },
	{"egrave",	6,	0x00e8 },
	{"eacute",	6,	0x00e9 },
	{"ecirc",	5,	0x00ea },
	{"euml",	4,	0x00eb },
	{"igrave",	6,	0x00ec },
	{"iacute",	6,	0x00ed },
	{"icirc",	5,	0x00ee },
	{"iuml",	4,	0x00ef },
	{"eth",		3,	0x00f0 },
	{"ntilde",	6,	0x00f1 },
	{"ograve",	6,	0x00f2 },
	{"oacute",	6,	0x00f3 },
	{"ocirc",	5,	0x00f4 },
	{"otilde",	6,	0x00f5 },
	{"ouml",	4,	0x00f6 },
	{"divide",	6,	0x00f7 },
	{"oslash",	6,	0x00f8 },
	{"ugrave",	6,	0x00f9 },
	{"uacute",	6,	0x00fa },
	{"ucirc",	5,	0x00fb },
	{"uuml",	4,	0x00fc },
	{"yacute",	6,	0x00fd },
	{"thorn",	5,	0x00fe },
	{"yuml",	4,	0x00ff },
	{"OElig",	5,	0x0152 },
	{"oelig",	5,	0x0153 },
	{"Scaron",	6,	0x0160 },
	{"scaron",	6,	0x0161 },
	{"Yuml",	4,	0x0178 },
	{"fnof",	4,	0x0192 },
	{"circ",	4,	0x02c6 },
	{"tilde",	5,	0x02dc },
	{"Alpha",	5,	0x0391 },
	{"Beta",	4,	0x0392 },
	{"Gamma",	5,	0x0393 },
	{"Delta",	5,	0x0394 },
	{"Epsilon",	7,	0x0395 },
	{"Zeta",	4,	0x0396 },
	{"Eta",		3,	0x0397 },
	{"Theta",	5,	0x0398 },
	{"Iota",	4,	0x0399 },
	{"Kappa",	5,	0x039a },
	{"Lambda",	6,	0x039b },
	{"Mu",		2,	0x039c },
	{"Nu",		2,	0x039d },
	{"Xi",		2,	0x039e },
	{"Omicron",	7,	0x039f },
	{"Pi",		2,	0x03a0 },
	{"Rho",		3,	0x03a1 },
	{"Sigma",	5,	0x03a3 },
	{"Tau",		3,	0x03a4 },
	{"Upsilon",	7,	0x03a5 },
	{"Phi",		3,	0x03a6 },
	{"Chi",		3,	0x03a7 },
	{"Psi",		3,	0x03a8 },
	{"Omega",	5,	0x03a9 },
	{"alpha",	5,	0x03b1 },
	{"beta",	4,	0x03b2 },
	{"gamma",	5,	0x03b3 },
	{"delta",	5,	0x03b4 },
	{"epsilon",	7,	0x03b5 },
	{"zeta",	4,	0x03b6 },
	{"eta",		3,	0x03b7 },
	{"theta",	5,	0x03b8 },
	{"iota",	4,	0x03b9 },
	{"kappa",	5,	0x03ba },
	{"lambda",	6,	0x03bb },
	{"mu",		2,	0x03bc },
	{"nu",		2,	0x03bd },
	{"xi",		2,	0x03be },
	{"omicron",	7,	0x03bf },
	{"pi",		2,	0x03c0 },
	{"rho",		3,	0x03c1 },
	{"sigmaf",	6,	0x03c2 },
	{"sigma",	5,	0x03c3 },
	{"tau",		3,	0x03c4 },
	{"upsilon",	7,	0x03c5 },
	{"phi",		3,	0x03c6 },
	{"chi",		3,	0x03c7 },
	{"psi",		3,	0x03c8 },
	{"omega",	5,	0x03c9 },
	{"thetasym",	8,	0x03d1 },
	{"upsih",	5,	0x03d2 },
	{"piv",		3,	0x03d6 },
	{"ensp",	4,	0x2002 },
	{"emsp",	4,	0x2003 },
	{"thinsp",	6,	0x2009 },
	{"zwnj",	4,	0x200c },
	{"zwj",		3,	0x200d },
	{"lrm",		3,	0x200e },
	{"rlm",		3,	0x200f },
	{"ndash",	5,	0x2013 },
	{"mdash",	5,	0x2014 },
	{"lsquo",	5,	0x2018 },
	{"rsquo",	5,	0x2019 },
	{"sbquo",	5,	0x201a },
	{"ldquo",	5,	0x201c },
	{"rdquo",	5,	0x201d },
	{"bdquo",	5,	0x201e },
	{"dagger",	6,	0x2020 },
	{"Dagger",	6,	0x2021 },
	{"bull",	4,	0x2022 },
	{"hellip",	6,	0x2026 },
	{"permil",	6,	0x2030 },
	{"prime",	5,	0x2032 },
	{"Prime",	5,	0x2033 },
	{"lsaquo",	6,	0x2039 },
	{"rsaquo",	6,	0x203a },
	{"oline",	5,	0x203e },
	{"frasl",	5,	0x2044 },
	{"euro",	4,	0x20ac },
	{"image",	5,	0x2111 },
	{"weierp",	6,	0x2118 },
	{"real",	4,	0x211c },
	{"trade",	5,	0x2122 },
	{"alefsym",	7,	0x2135 },
	{"larr",	4,	0x2190 },
	{"uarr",	4,	0x2191 },
	{"rarr",	4,	0x2192 },
	{"darr",	4,	0x2193 },
	{"harr",	4,	0x2194 },
	{"crarr",	5,	0x21b5 },
	{"lArr",	4,	0x21d0 },
	{"uArr",	4,	0x21d1 },
	{"rArr",	4,	0x21d2 },
	{"dArr",	4,	0x21d3 },
	{"hArr",	4,	0x21d4 },
	{"forall",	6,	0x2200 },
	{"part",	4,	0x2202 },
	{"exist",	5,	0x2203 },
	{"empty",	5,	0x2205 },
	{"nabla",	5,	0x2207 },
	{"isin",	4,	0x2208 },
	{"notin",	5,	0x2209 },
	{"ni",		2,	0x220b },
	{"prod",	4,	0x220f },
	{"sum",		3,	0x2211 },
	{"minus",	5,	0x2212 },
	{"lowast",	6,	0x2217 },
	{"radic",	5,	0x221a },
	{"prop",	4,	0x221d },
	{"infin",	5,	0x221e },
	{"ang",		3,	0x2220 },
	{"and",		3,	0x2227 },
	{"or",		2,	0x2228 },
	{"cap",		3,	0x2229 },
	{"cup",		3,	0x222a },
	{"int",		3,	0x222b },
	{"there4",	6,	0x2234 },
	{"sim",		3,	0x223c },
	{"cong",	4,	0x2245 },
	{"asymp",	5,	0x2248 },
	{"ne",		2,	0x2260 },
	{"equiv",	5,	0x2261 },
	{"le",		2,	0x2264 },
	{"ge",		2,	0x2265 },
	{"sub",		3,	0x2282 },
	{"sup",		3,	0x2283 },
	{"nsub",	4,	0x2284 },
	{"sube",	4,	0x2286 },
	{"supe",	4,	0x2287 },
	{"oplus",	5,	0x2295 },
	{"otimes",	6,	0x2297 },
	{"perp",	4,	0x22a5 },
	{"sdot",	4,	0x22c5 },
	{"lceil",	5,	0x2308 },
	{"rceil",	5,	0x2309 },
	{"lfloor",	6,	0x230a },
	{"rfloor",	6,	0x230b },
	{"lang",	4,	0x2329 },
	{"rang",	4,	0x232a },
	{"loz",		3,	0x25ca },
	{"spades",	6,	0x2660 },
	{"clubs",	5,	0x2663 },
	{"hearts",	6,	0x2665 },
	{"diams",	5,	0x2666 },

	{NULL,		0,	0x0000 },
};


uint16
pa_NumericCharacterReference(unsigned char *in, uint32 inlen, uint32 *inread,
	Bool force )
{
	unsigned char	b;
	uint32		i;
	uint32	ncr;


	for( i = 0; i < inlen; i++ ){
		b = in[i];
		if ((b < '0') || (b > '9'))  break;
	}

	if( i == 0 ){
		*inread = 0;
		return '?';
	}

	if( (i >= inlen) && (!force) ){
		*inread = (inlen + 1);
		return '?';
	}
	inlen = i;

	ncr = 0;
	for (i = 0; i < inlen; i++){
		ncr = 10 * ncr + ( in[i] - '0' );
	}
	*inread = i;

	/* if it is an illegal NCR, then place the '?' instead of,
		if it is an illegal NE, then treat is as plain text */
	if( ncr & 0xffff0000 )	return '?';

	return (uint16)ncr;
}



/*************************************
 * Function: pa_map_escape
 *
 * Description: This function maps the passed in amperstand
 * 		escape sequence to a single 8 bit char (ISO 8859-1).
 *
 * Params: Takes a buffer, and a length for that buffer. (buf is NOT
 *         a \0 terminated string).  Also takes a pointer to an escape length
 *	   which is home much of the original buffer was used.
 *	   And an out buffer, and it's outbuflen.
 *
 * Returns: On error, which means it was not passed
 *          a valid amperstand escape sequence, it stores the
 *	    char value '\0' in "out".  It also returns the length of
 *	    the part of the passed buffer that was replaced.
 *	    Also, the number of bytes written to the out buffer (outlen).
 *************************************/
static void
pa_map_escape(char *buf, int32 len, int32 *esc_len, unsigned char *out,
	uint16 outbuflen, uint16 *outlen, Bool force, uint16 win_csid)
{
	uint16 unicode;
	char val;
	int32 cnt;

	*esc_len = 0;
	val = 0;

	/* Navigator View->source does not support Unicode yet */
	if( win_csid == 0 )   win_csid = CS_FE_ASCII;

	/*
	 * Skip the amperstand character at the start.
	 */
	buf++;
	len--;

	/*
	 * Ampersand followed by number sign specifies the decimal
	 * value of the character to be placed here.
	 */
	if (*buf == '#')
	{
		unicode = pa_NumericCharacterReference((unsigned char *) (buf + 1),
			(uint32) (len - 1), (uint32 *) esc_len, force );
		*esc_len += 1;
		
#ifdef PA_REMAP_128_TO_160_ILLEGAL_NCR
		/* for some illegal, but popular usage */
		if( unicode >= 0x0080 && unicode <= 0x009f )
			unicode = PA_HackTable[ unicode - 0x0080 ];
#endif
	}
	/*
	 * Else we have to look this escape up in
	 * the array of valid escapes.
	 */
	else
	{
		/*
		 * Else we have to look this escape up in
		 * the array of valid escapes.
		 * Since we are searching on the PA_Name2Unicode[].NE string,
		 * and not on the escape entered, this lets us partial
		 * match if PA_Name2Unicode[].NE matches a prefix of the escape.
		 * This means no one PA_Name2Unicode[].NE should be a prefix
		 * of any other PA_Name2Unicode[].NE.
		 */
		cnt = 0;
		while( PA_Name2Unicode[cnt].NE != NULL )
		{
			if (*buf == *(PA_Name2Unicode[cnt].NE)
			    && (XP_STRNCMP(buf+1, PA_Name2Unicode[cnt].NE+1,
				PA_Name2Unicode[cnt].len-1) == 0))
			{
				/*
				 * Only match a prefix if the next
				 * character is NOT an alphanumeric
				 */
				if ((!XP_IS_ALPHA(*(buf +
					PA_Name2Unicode[cnt].len)))&&
				    (!XP_IS_DIGIT(*(buf +
					PA_Name2Unicode[cnt].len))))
				{
					unicode = PA_Name2Unicode[cnt].unicode;
					*esc_len = PA_Name2Unicode[cnt].len;
					break;
				}
			}
			cnt++;
		}

		/*
		 * for invalid escapes, return '\0'
		 */
		if( PA_Name2Unicode[cnt].NE == NULL )
			unicode = NOT_USED;  /* treated as plain text, just for NE */
	}


	if( unicode == NOT_USED )
	{
		out[0] = 0;
		return;
	}


	/*	if( outbuflen >= INTL_UnicodeToStrLen( win_csid, &unicode, 1 ) ) 
		I needn't to do that, for the outbuflen is always larget than outlen ,
		the result of generated "out" through NE/NCR is one byte or two bytes */
	INTL_UnicodeToStr( win_csid, &unicode, 1, out, outbuflen );

	/*	Remap  NBSP to international version 	*/
	/*	We need to do this in any case 		*/
	if(unicode == 0x00a0)
	{
		strcpy((char*)out, INTL_NonBreakingSpace(win_csid));
	}

	*outlen = XP_STRLEN((char*) out );

	/* 	Do fallback for Copyright, Tradmark and Register Sign */
	/*	We try those non-standard secret platform specific	  */
	/*  codepoint first. 									  */
	/*	If there are no such, do an ASCII fallback 			  */
	if((*outlen == 1) && (*out == '?'))
	{
		char* r = NULL;
#if defined(XP_WIN) || defined(XP_OS2)
		/*		
			For Winodows
			All the CP 125x single byte pages encode
				Copyright Signed as A9
				Register Signed as 	AE
				Tradmark Signed as 	99
			Two Byte Asian Font do not have those sign 

		*/
		switch(unicode)
		{
			case 0x00A9:
				switch(win_csid)
				{
					case CS_LATIN1: 	case CS_CP_1250: 	
					case CS_CP_1251: 	case CS_CP_1253: 	
					case CS_CP_1254: 	case CS_8859_7:
					case CS_8859_9:
						r = "\xa9"; 
						break;
					default:
						r = "(C)"; 	
						break;
				}	
				break;
			case 0x00AE:
				switch(win_csid)
				{
					case CS_LATIN1: 	case CS_CP_1250:
					case CS_CP_1251: 	case CS_CP_1253:
					case CS_CP_1254: 	case CS_8859_7:
					case CS_8859_9:
						r = "\xae"; 
						break;
					default:
						r = "(R)"; 	
						break;
				}	
				break;
			case 0x2122:
				switch(win_csid)
				{
					case CS_LATIN1: 	case CS_CP_1250:
					case CS_CP_1251: 	case CS_CP_1253:
					case CS_CP_1254: 	case CS_8859_7:
					case CS_8859_9:
						r = "\x99"; 
						break;
					default:
						r = "[tm]"; 	
						break;
				}	
				break;
		}
#endif
#ifdef XP_MAC
	/*
		For Mac:
			Japanese and Chinese font usually have secret glyph 
				Copyright Signed at 0xfd
				Tradmark Signed at 0xff

	*/
		switch(unicode)
		{
			case 0x00A9:
				switch(win_csid)
				{
					case CS_MAC_ROMAN: 	
					case CS_MAC_GREEK:
						r = "\xd9"; 
						break;
					case CS_MAC_CE: 	
					case CS_MAC_CYRILLIC:
						r = "\xa9"; 
						break;
					case CS_KSC_8BIT:
						r = "\x93"; 
						break;
					case CS_SJIS: 		
					case CS_GB_8BIT:
					case CS_CNS_8BIT:
						r = "\xfd"; 
						break;
					case CS_TIS620:
						r = "\xfb"; 
						break;
					default:
						r = "(C)"; 	
						break;
				}	
				break;
			case 0x00AE:
				switch(win_csid)
				{
					case CS_MAC_ROMAN: 	
					case CS_MAC_GREEK:
					case CS_MAC_CE: 	
					case CS_MAC_CYRILLIC:
						r = "\xa8"; 
						break;
					case CS_TIS620:
						r = "\xfa"; 
						break;
					default:
						r = "(R)"; 	
						break;
				}	
				break;
			case 0x2122:
				switch(win_csid)
				{
					case CS_MAC_ROMAN: 	
					case CS_MAC_GREEK:
					case CS_MAC_CE: 	
					case CS_MAC_CYRILLIC:
						r = "\xaa"; 
						break;
					case CS_SJIS: 		
					case CS_GB_8BIT:
					case CS_CNS_8BIT:
						r = "\xfe"; 
						break;
					case CS_TIS620:
						r = "\xee"; 
						break;
					default:
						r = "[tm]"; 	break;
				}	
				break;
		}
#endif 

#ifdef XP_UNIX 
		switch(unicode)
		{
			case 0x00A9:
				switch(win_csid)
				{
					default:
						r = "(C)"; 	
						break;
				}	
				break;
			case 0x00AE:
				switch(win_csid)
				{
					default:
						r = "(R)"; 	
						break;
				}	
				break;
			case 0x0152:
				switch(win_csid)
				{
					default:
						r = "OE"; 	
						break;
				}	
				break;
			case 0x0153:
				switch(win_csid)
				{
					default:
						r = "oe"; 	
						break;
				}	
				break;
			case 0x0160:
				switch(win_csid)
				{
					default:
						r = "S"; 	
						break;
				}	
				break;
			case 0x0161:
				switch(win_csid)
				{
					default:
						r = "s"; 	
						break;
				}	
				break;
			case 0x0178:
				switch(win_csid)
				{
					default:
						r = "Y"; 	
						break;
				}	
				break;
			case 0x0192:
				switch(win_csid)
				{
					default:
						r = "f"; 	
						break;
				}	
				break;
			case 0x02C6:
				switch(win_csid)
				{
					default:
						r = "^"; 	
						break;
				}	
				break;
			case 0x02DC:
				switch(win_csid)
				{
					default:
						r = "~"; 	
						break;
				}	
				break;
			case 0x2013:
				switch(win_csid)
				{
					default:
						r = "-"; 	
						break;
				}	
				break;
			case 0x2014:
				switch(win_csid)
				{
					default:
						r = "-"; 	
						break;
				}	
				break;
			case 0x2018:
				switch(win_csid)
				{
					default:
						r = "`"; 	
						break;
				}	
				break;
			case 0x2019:
				switch(win_csid)
				{
					default:
						r = "'"; 	
						break;
				}	
				break;
			case 0x201C:
				switch(win_csid)
				{
					default:
						r = "\""; 	
						break;
				}	
				break;
			case 0x201D:
				switch(win_csid)
				{
					default:
						r = "\""; 	
						break;
				}	
				break;
			case 0x201A:
				switch(win_csid)
				{
					default:
						r = ","; 	
						break;
				}	
				break;
			case 0x201E:
				switch(win_csid)
				{
					default:
						r = ",,"; 	
						break;
				}	
				break;
			case 0x2022:
				switch(win_csid)
				{
					default:
						r = "*"; 	
						break;
				}	
				break;
			case 0x2026:
				switch(win_csid)
				{
					default:
						r = "..."; 	
						break;
				}	
				break;
			case 0x2039:
				switch(win_csid)
				{
					default:
						r = "<"; 	
						break;
				}	
				break;
			case 0x203A:
				switch(win_csid)
				{
					default:
						r = ">"; 	
						break;
				}	
				break;
			case 0x2122:
				switch(win_csid)
				{
					default:
						r = "[tm]"; 	
						break;
				}	
				break;
		}
#endif
		if(r != NULL )
		{
			strcpy((char*)out, r);
			*outlen = XP_STRLEN((char*)out);
		}
	}
} 


/*************************************
 * Function: pa_ExpandEscapes
 *
 * Description: Go through the passed in buffer of text, find and replace
 *		all amperstand escape sequences with their character
 * 		equivilants.  The buffer is modified in place,
 * 		each escape replacement shortens the contents of the
 * 		buffer.
 *
 * Params: Takes a buffer, and a length for that buffer. (buf is NOT
 *         a \0 terminated string).  Also takes a pointer to a new length.
 *         This is the new (possibly shorter) length of the buffer
 *         after the escapes have been replaced.  Finally takes a boolean
 *	   that specifies whether partial escapes are allowed or not.
 *
 * Returns: The new length of the string in the nlen parameter. Also
 * 	    returns a NULL character pointer on success.  
 *          If the buffer passed contained a possibly truncated
 *	    amperstand escape, that is considered a partial success.
 *	    Escapes are replace in all the buffer up to the truncated
 *	    escape, nlen is the length of the new sub-buffer, and
 * 	    the return code is a pointer to the begining of the truncated
 * 	    escape in the original buffer.
 *************************************/

char *
pa_ExpandEscapes(char *buf, int32 len, int32 *nlen, Bool force, int16 win_csid)
{
	int32 cnt;
	char *from_ptr;
	char *to_ptr;
	unsigned char out[10];
	uint16 outlen;
	unsigned char *p;

	*nlen = 0;

	/*
	 * A NULL buffer translates to a NULL buffer.
	 */
	if (buf == NULL)
	{
		return(NULL);
	}

	/*
	 * Look through the passed buf for the beginning of amperstand
	 * escapes.  They begin with an amperstand followed by a letter
	 * or a number sign.
	 */
	from_ptr = buf;
	to_ptr = buf;
	cnt = 0;
	while (cnt < len)
	{
		char *tptr2;
		int32 cnt2;
		int32 esc_len;

		if (*from_ptr == '&')
		{
			/*
			 * If this is the last character in the string
			 * we don't know if it is an amperstand escape yet
			 * or not.  We want to save it until the next buffer
			 * to make sure.  However, if force is true this
			 * can't be a partial escape, so asuume an error
			 * and cope as best you can.
			 */
			if ((cnt + 1) == len)
			{
				if (force != FALSE)
				{
					*to_ptr++ = *from_ptr++;
					cnt++;
					continue;
				}
				/*
				 * This might be a partial amperstand escape.
				 * we will have to wait until we have
				 * more data.  Break the while loop
				 * here.
				 */
				break;
			}
			/*
			 * else is the next character is a letter or
			 * a '#' , this should be some kind of amperstand
			 * escape.
			 */
			else if ((XP_IS_ALPHA(*(from_ptr + 1)))||
				(*(from_ptr + 1) == '#'))
			{
				/*
				 * This is an amperstand escape!  Drop
				 * through to the rest of the while loop.
				 */
			}
			/*
			 * else this character does not begin an
			 * amperstand escape increment pointer, and go
			 * back to the start of the while loop.
			 */
			else
			{
				*to_ptr++ = *from_ptr++;
				cnt++;
				continue;
			}
		}
		/*
		 * else this character does not begin an amperstand escape
		 * increment pointer, and go back to the start of the
		 * while loop.
		 */
		else
		{
			*to_ptr++ = *from_ptr++;
			cnt++;
			continue;
		}

		/*
		 * If we got here, it is because we think we have
		 * an amperstand escape.  Amperstand escapes are of arbitrary
		 * length, terminated by either a semi-colon, or white space.
		 *
		 * Find the end of the amperstand escape.
		 */
		tptr2 = from_ptr;
		cnt2 = cnt;
		while (cnt2 < len)
		{
			if ((*tptr2 == ';')||(XP_IS_SPACE(*tptr2)))
			{
				break;
			}
			tptr2++;
			cnt2++;
		}
		/*
		 * We may have failed to find the end because this is
		 * only a partial amperstand escape.  However, if force is
		 * true this can't be a partial escape, so asuume and error
		 * and cope as best you can.
		 */
		if ((cnt2 == len)&&(force == FALSE))
		{
			/*
			 * This might be a partial amperstand escape.
			 * we will have to wait until we have
			 * more data.  Break the while loop
			 * here.
			 */
			break;
		}

		esc_len = 0;
		pa_map_escape(from_ptr, (intn)(tptr2 - from_ptr + 1),
			&esc_len, out, sizeof(out), &outlen, force, win_csid);
		if (esc_len > (tptr2 - from_ptr + 1))
		{
			/*
			 * This is a partial numeric char ref.
			 */
			break;
		}
		/*
		 * invalid escape sequences return the end of string
		 * character.  Treat this not as an amperstand escape, but as
		 * normal text.
		 */
		if (out[0] == '\0')
		{
			*to_ptr++ = *from_ptr++;
			cnt++;
			continue;
		}

		/*
		 * valid escape sequences are replaced in place, with the
		 * rest of the buffer copied forward.  For improper escape
		 * sequences that we think we can recover from, we may have
		 * just used part of the beginning of the escape sequence.
		 */
		p = out;
		while (outlen--)
		{
			*to_ptr++ = *p++;
		}
		tptr2 = (char *)(from_ptr + esc_len + 1);
		cnt2 = cnt + esc_len + 1;

		/*
		 * for semi-colon terminated escapes, eat the semi-colon.
		 * The test on cnt2 prevents up walking off the end of the
		 * buffer on a broken escape we are coping with.
		 */
		if ((cnt2 < len)&&(*tptr2 == ';'))
		{
			tptr2++;
			cnt2++;
		}
		from_ptr = tptr2; 
		cnt = cnt2;
	}

	/*
	 * If we broke the while loop early,
	 * it is because we may be holding a
	 * partial amperstand escape.
	 * return a pointer to that partial escape.
	 */
	if (cnt < len)
	{
		*nlen = (int32)(to_ptr - buf);
		return(from_ptr);
	}
	else
	{
		*nlen = (int32)(to_ptr - buf);
		return(NULL);
	}
}

#ifdef PROFILE
#pragma profile off
#endif

