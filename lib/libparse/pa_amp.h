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
/* #include "xp.h" */
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
	{"lt",		2,	0x003c },
	{"LT",		2,	0x003c },
	{"gt",		2,	0x003e },
	{"GT",		2,	0x003e },
	{"amp",		3,	0x0026 },
	{"AMP",		3,	0x0026 },
	{"quot",	4,  0x0022 },
	{"QUOT",	4,	0x0022 },
	{"nbsp",    4,  0x00a0 },
	{"reg",		3,	0x00ae },
	{"REG",		3,	0x00ae },
	{"copy",	4,	0x00a9 },
	{"COPY",	4,	0x00a9 },

	{"iexcl",	5,	0x00a1 },
	{"cent",	4,	0x00a2 },
	{"pound",	5,	0x00a3 },
	{"curren",  6,	0x00a4 },
	{"yen",		3,	0x00a5 },
	{"brvbar",	6,	0x00a6 },
	{"sect",	4,	0x00a7 },

	{"uml",		3,	0x00a8 },
	{"ordf",	4,	0x00aa },
	{"laquo",	5,	0x00ab },
	{"not",		3,	0x00ac },
	{"shy",		3,	0x00ad },
	{"macr",	4,	0x00af },

	{"deg",		3,	0x00b0 },
	{"plusmn",	6,	0x00b1 },
	{"sup2",	4,	0x00b2 },
	{"sup3",	4,	0x00b3 },
	{"acute",	5,  0x00b4 },
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

	{NULL,		0,	0x0000 },
};

typedef struct PA_AmpEsc_struct {
        char *str;
        char value;
        intn len;
} PA_AmpEsc;

#ifndef XP_MAC
static PA_AmpEsc PA_AmpEscapes[] = {
	{"lt", '<', 2},
	{"LT", '<', 2},
	{"gt", '>', 2},
	{"GT", '>', 2},
	{"amp", '&', 3},
	{"AMP", '&', 3},
	{"quot", '\"', 4},
	{"QUOT", '\"', 4},
	{"nbsp", '\240', 4},
	{"reg", '\256', 3},
	{"REG", '\256', 3},
	{"copy", '\251', 4},
	{"COPY", '\251', 4},

	{"iexcl", '\241', 5},
	{"cent", '\242', 4},
	{"pound", '\243', 5},
	{"curren", '\244', 6},
	{"yen", '\245', 3},
	{"brvbar", '\246', 6},
	{"sect", '\247', 4},

	{"uml", '\250', 3},
	{"ordf", '\252', 4},
	{"laquo", '\253', 5},
	{"not", '\254', 3},
	{"shy", '\255', 3},
	{"macr", '\257', 4},

	{"deg", '\260', 3},
	{"plusmn", '\261', 6},
	{"sup2", '\262', 4},
	{"sup3", '\263', 4},
	{"acute", '\264', 5},
	{"micro", '\265', 5},
	{"para", '\266', 4},
	{"middot", '\267', 6},

	{"cedil", '\270', 5},
	{"sup1", '\271', 4},
	{"ordm", '\272', 4},
	{"raquo", '\273', 5},
	{"frac14", '\274', 6},
	{"frac12", '\275', 6},
	{"frac34", '\276', 6},
	{"iquest", '\277', 6},

	{"Agrave", '\300', 6},
	{"Aacute", '\301', 6},
	{"Acirc", '\302', 5},
	{"Atilde", '\303', 6},
	{"Auml", '\304', 4},
	{"Aring", '\305', 5},
	{"AElig", '\306', 5},
	{"Ccedil", '\307', 6},

	{"Egrave", '\310', 6},
	{"Eacute", '\311', 6},
	{"Ecirc", '\312', 5},
	{"Euml", '\313', 4},
	{"Igrave", '\314', 6},
	{"Iacute", '\315', 6},
	{"Icirc", '\316', 5},
	{"Iuml", '\317', 4},

	{"ETH", '\320', 3},
	{"Ntilde", '\321', 6},
	{"Ograve", '\322', 6},
	{"Oacute", '\323', 6},
	{"Ocirc", '\324', 5},
	{"Otilde", '\325', 6},
	{"Ouml", '\326', 4},
	{"times", '\327', 5},

	{"Oslash", '\330', 6},
	{"Ugrave", '\331', 6},
	{"Uacute", '\332', 6},
	{"Ucirc", '\333', 5},
	{"Uuml", '\334', 4},
	{"Yacute", '\335', 6},
	{"THORN", '\336', 5},
	{"szlig", '\337', 5},

	{"agrave", '\340', 6},
	{"aacute", '\341', 6},
	{"acirc", '\342', 5},
	{"atilde", '\343', 6},
	{"auml", '\344', 4},
	{"aring", '\345', 5},
	{"aelig", '\346', 5},
	{"ccedil", '\347', 6},

	{"egrave", '\350', 6},
	{"eacute", '\351', 6},
	{"ecirc", '\352', 5},
	{"euml", '\353', 4},
	{"igrave", '\354', 6},
	{"iacute", '\355', 6},
	{"icirc", '\356', 5},
	{"iuml", '\357', 4},

	{"eth", '\360', 3},
	{"ntilde", '\361', 6},
	{"ograve", '\362', 6},
	{"oacute", '\363', 6},
	{"ocirc", '\364', 5},
	{"otilde", '\365', 6},
	{"ouml", '\366', 4},
	{"divide", '\367', 6},

	{"oslash", '\370', 6},
	{"ugrave", '\371', 6},
	{"uacute", '\372', 6},
	{"ucirc", '\373', 5},
	{"uuml", '\374', 4},
	{"yacute", '\375', 6},
	{"thorn", '\376', 5},
	{"yuml", '\377', 4},

	{NULL, '\0', 0},
};
#else /* ! XP_MAC */
							/* Entities encoded in MacRoman.  */
static PA_AmpEsc PA_AmpEscapes[] = {
	{"lt", '<', 2},
	{"LT", '<', 2},
	{"gt", '>', 2},
	{"GT", '>', 2},
	{"amp", '&', 3},
	{"AMP", '&', 3},
	{"quot", '\"', 4},
	{"QUOT", '\"', 4},
	{"nbsp", '\007', 4},
	{"reg", '\250', 3},
	{"REG", '\250', 3},
	{"copy", '\251', 4},
	{"COPY", '\251', 4},

	{"iexcl", '\301', 5},
	{"cent", '\242', 4},
	{"pound", '\243', 5},
	{"curren", '\333', 6},
	{"yen", '\264', 3},

	/*
	 * Navigator Gold currently inverts this table in such a way that
	 * ASCII characters (less than 128) get converted to the names
	 * listed here.  For things like ampersand (&amp;) this is the
	 * right thing to do, but for this one (brvbar), it isn't since
	 * both broken vertical bar and vertical bar are mapped to the same
	 * character by the Latin-1 to Mac Roman table.
	 *
	 * Punt for now. This needs to be fixed later. -- erik
	 */
	/* {"brvbar", '\174', 6}, */

	{"sect", '\244', 4},

	{"uml", '\254', 3},
	{"ordf", '\273', 4},
	{"laquo", '\307', 5},
	{"not", '\302', 3},
	{"shy", '\320', 3},
	{"macr", '\370', 4},

	{"deg", '\241', 3},
	{"plusmn", '\261', 6},
	/* {"sup2", '\62', 4}, see comment above */
	/* {"sup3", '\63', 4}, see comment above */
	{"acute", '\253', 5},
	{"micro", '\265', 5},
	{"para", '\246', 4},
	{"middot", '\341', 6},

	{"cedil", '\374', 5},
	/* {"sup1", '\61', 4}, see comment above */
	{"ordm", '\274', 4},
	{"raquo", '\310', 5},
	{"frac14", '\271', 6},
	{"frac12", '\270', 6},
	{"frac34", '\262', 6},
	{"iquest", '\300', 6},

	{"Agrave", '\313', 6},
	{"Aacute", '\347', 6},
	{"Acirc", '\345', 5},
	{"Atilde", '\314', 6},
	{"Auml", '\200', 4},
	{"Aring", '\201', 5},
	{"AElig", '\256', 5},
	{"Ccedil", '\202', 6},

	{"Egrave", '\351', 6},
	{"Eacute", '\203', 6},
	{"Ecirc", '\346', 5},
	{"Euml", '\350', 4},
	{"Igrave", '\355', 6},
	{"Iacute", '\352', 6},
	{"Icirc", '\353', 5},
	{"Iuml", '\354', 4},

	{"ETH", '\334', 3},			/* Icelandic MacRoman: ETH ('D' w/horiz bar) */
	{"Ntilde", '\204', 6},
	{"Ograve", '\361', 6},
	{"Oacute", '\356', 6},
	{"Ocirc", '\357', 5},
	{"Otilde", '\315', 6},
	{"Ouml", '\205', 4},
	/* {"times", '\170', 5}, see comment above */

	{"Oslash", '\257', 6},
	{"Ugrave", '\364', 6},
	{"Uacute", '\362', 6},
	{"Ucirc", '\363', 5},
	{"Uuml", '\206', 4},
	{"Yacute", '\240', 6},		/* Icelandic MacRoman: Yacute */
	{"THORN", '\336', 5},		/* Icelandic MacRoman: THORN (kinda like 'P') */
	{"szlig", '\247', 5},

	{"agrave", '\210', 6},
	{"aacute", '\207', 6},
	{"acirc", '\211', 5},
	{"atilde", '\213', 6},
	{"auml", '\212', 4},
	{"aring", '\214', 5},
	{"aelig", '\276', 5},
	{"ccedil", '\215', 6},

	{"egrave", '\217', 6},
	{"eacute", '\216', 6},
	{"ecirc", '\220', 5},
	{"euml", '\221', 4},
	{"igrave", '\223', 6},
	{"iacute", '\222', 6},
	{"icirc", '\224', 5},
	{"iuml", '\225', 4},

	{"eth", '\335', 3},		/* Icelandic MacRoman: eth ('d' w/horiz bar) */
	{"ntilde", '\226', 6},
	{"ograve", '\230', 6},
	{"oacute", '\227', 6},
	{"ocirc", '\231', 5},
	{"otilde", '\233', 6},
	{"ouml", '\232', 4},
	{"divide", '\326', 6},

	{"oslash", '\277', 6},
	{"ugrave", '\235', 6},
	{"uacute", '\234', 6},
	{"ucirc", '\236', 5},
	{"uuml", '\237', 4},
	{"yacute", '\340', 6},		/* Icelandic MacRoman: yacute */
	{"thorn", '\337', 5},		/* Icelandic MacRoman: thorn (kinda like 'p') */
	{"yuml", '\330', 4},

	{NULL, '\0', 0},
};
#endif /* ! XP_MAC */

