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
/*	kinsukof.c	*/

#include "intlpriv.h"

/* The table is defined in kinsukod.c */
extern const char *ProhibitBegin_SJIS[];
extern const char *ProhibitBegin_EUCJP[];
extern const char *ProhibitBegin_BIG5[];
extern const char *ProhibitBegin_GB[];
extern const char *ProhibitBegin_KSC[];
extern const char *ProhibitBegin_UTF8[];
extern const char *ProhibitBegin_CNS[];

extern const char *ProhibitEnd_SJIS[];
extern const char *ProhibitEnd_EUCJP[];
extern const char *ProhibitEnd_BIG5[];
extern const char *ProhibitEnd_GB[];
extern const char *ProhibitEnd_KSC[];
extern const char *ProhibitEnd_UTF8[];
extern const char *ProhibitEnd_CNS[];

PUBLIC const char *INTL_NonBreakingSpace(uint16 win_csid)
{

#ifdef XP_MAC
        return "\07";		/* 0x07 */
#else
        return "\240";		/* 0xA0 */
#endif

}
/*
    INTL_CharClass is used for multibyte to divide character to different type
*/
#define IN_BETWEEN(a,b,c)	(((a) <= (b)) && ((b) <= (c)))
PUBLIC int
INTL_CharClass(int charset, unsigned char *pstr)
{
	int	c1, c2, c3;

	c1 = *pstr;

	switch (charset)
	{
	case CS_SJIS:
		/*
			SEVEN_BIT_CHAR:					[0x00-0x7F]
			HALFWIDTH_PRONOUNCE_CHAR:		[0xA0-0xE0]
			FULLWIDTH_ASCII_CHAR:			[0x82]		[0x60-0x9A]
											[0x83]		[0x9f-0xB6]	( Really no ASCII but Greek and Cyrillic )
											[0x83]		[0xBF-0x8F]	
											[0x84]		[0x40-0x60]	
											[0x84]		[0x70-0x8F]	
			FULLWIDTH_PRONOUNCE_CHAR:		[0x82]		[0x9F-0xF1]
											[0x83]		[0x40-0x96]
											[0x81]		[0x5B-0x5D]
			KANJI_CHAR:						[0x88-0xFC] [xxxxxxxxx] (Except above)

			Note:	We count Cyrillic and Greek as FULLWIDTH_ASCII_CHAR

		*/
        if (c1 < 0x80)
            return  SEVEN_BIT_CHAR;

		if (IN_BETWEEN(0xA0, c1, 0xE0))
            return HALFWIDTH_PRONOUNCE_CHAR;

        c2 = *(pstr + 1);

		switch(c1)
		{
			case 0x81:
				if(IN_BETWEEN(0x5B, c2, 0x5D)) 
            		return FULLWIDTH_PRONOUNCE_CHAR;
				break;

			case 0x82:
				if(IN_BETWEEN(0x60, c2, 0x9A))
           			return FULLWIDTH_ASCII_CHAR;

				if(IN_BETWEEN(0x9F, c2, 0xF1))
            		return FULLWIDTH_PRONOUNCE_CHAR;
				break;

			case 0x83:
				if(IN_BETWEEN(0x9F, c2, 0xB6) || IN_BETWEEN(0xBF, c2, 0xD0))
           			return FULLWIDTH_ASCII_CHAR;

				if(IN_BETWEEN(0x40, c2, 0x96))
            		return FULLWIDTH_PRONOUNCE_CHAR;
				break;

			case 0x84:
				if(IN_BETWEEN(0x40, c2, 0x8F) || IN_BETWEEN(0xBF, c2, 0xD0)) 
           			return FULLWIDTH_ASCII_CHAR;
				break;
		}


		if (IN_BETWEEN(0x88, c1, 0xFC))
            return KANJI_CHAR;

        return UNCLASSIFIED_CHAR;

    case CS_EUCJP:		/* TO BE TEST ON UNIX */
		/*
			SEVEN_BIT_CHAR:					[0x00-0x7F]
			HALFWIDTH_PRONOUNCE_CHAR:		[0x8E]
			FULLWIDTH_ASCII_CHAR:			[0xA3]		[0xC1-0xDA]
														[0xE1-0xFA]
											[0xA6]		[0xA1-0xB8]
														[0xC1-0xD8]
											[0xA7]		[0xA1-0xC1]
														[0xD1-0xF1]
											[0x8F]		[0xA6-0xAF]
			FULLWIDTH_PRONOUNCE_CHAR:		[0xA4]		[xxxxxxx]
											[0xA5]		[xxxxxxx]
											[0x81]		[0x5B-0x5D]
			KANJI_CHAR:						[0xB0-0xFF] [xxxx]
											[0x8F]		[>0xB0]

			Note:	We count Cyrillic and Greek as FULLWIDTH_ASCII_CHAR

		*/
        if (c1 < 0x80)
            return  SEVEN_BIT_CHAR;

        c2 = *(pstr + 1);

		switch(c1)
		{
			case 0x8E:
            	return HALFWIDTH_PRONOUNCE_CHAR;

			case 0x8F:
				if(IN_BETWEEN(0xA6, c2, 0xAF))
           			return FULLWIDTH_ASCII_CHAR;
				break;

			case 0xA3:
				if(IN_BETWEEN(0xC1, c2, 0xDA) || IN_BETWEEN(0xE1, c2, 0xFA))
           			return FULLWIDTH_ASCII_CHAR;
				break;

			case 0xA4: 	case 0xA5:
            	return FULLWIDTH_PRONOUNCE_CHAR;

			case 0xA6:
				if(IN_BETWEEN(0xA1, c2, 0xB8) || IN_BETWEEN(0xC1, c2, 0xD8))
           			return FULLWIDTH_ASCII_CHAR;
				break;

			case 0xA7:
				if(IN_BETWEEN(0xA1, c2, 0xC1) || IN_BETWEEN(0xD1, c2, 0xF1))
           			return FULLWIDTH_ASCII_CHAR;
				break;
		}


		if( 
			(c1 >= 0xB0) || 
			((c1 == 0x8F) &&  (c2 > 0xB0))
		  )
		{
            return KANJI_CHAR;
		}

        return UNCLASSIFIED_CHAR;

    case CS_KSC_8BIT:
		/*
			SEVEN_BIT_CHAR:					[0x00-0x80]
			HALFWIDTH_PRONOUNCE_CHAR:		None
			FULLWIDTH_ASCII_CHAR:			[0xA3]		[0xC1-0xDA]
														[0xE1-0xFA]
											[0xA5]		[0xC1-0xD8]
														[0xE1-0xF8]
											[0xAC]		[0xA1-0xC2]
														[0xD1-0xF2]
			FULLWIDTH_PRONOUNCE_CHAR:		[0xA4]		[0xA1-0xFE]
											[0xB0-0xC8]	[xxxxxxxxx]
			KANJI_CHAR:						[0xCA-0xFD] [xxxxxxxxx]

			Note:	 We didn't handle Hiragana and Katakana here
					 We count Cyrillic and Greek as FULLWIDTH_ASCII_CHAR

		*/        
		if (c1 < 0x80)
            return  SEVEN_BIT_CHAR;

        c2 = *(pstr + 1);
		if (
			((c1== 0xA3) && (IN_BETWEEN(0xC1, c2, 0xDA) || IN_BETWEEN(0xE1, c2, 0xFA))) ||
			((c1== 0xA5) && (IN_BETWEEN(0xC1, c2, 0xD8) || IN_BETWEEN(0xE1, c2, 0xF8))) ||
			((c1== 0xAC) && (IN_BETWEEN(0xA1, c2, 0xC2) || IN_BETWEEN(0xD1, c2, 0xF2))) 
		   )
        {
           return FULLWIDTH_ASCII_CHAR;
        }

		if (
			((c1== 0xA4) && 			(IN_BETWEEN(0xA1, c2, 0xFE))) ||
			 (IN_BETWEEN(0xB0, c1, 0xC8))
		   )
        {
            return FULLWIDTH_PRONOUNCE_CHAR;
        }

		if (IN_BETWEEN(0xCA, c1, 0xFD))
            return KANJI_CHAR;

        return UNCLASSIFIED_CHAR;

    case CS_GB_8BIT:
 		/*
			SEVEN_BIT_CHAR:						[0x00-0x7F]
			HALFWIDTH_PRONOUNCE_CHAR:			
			FULLWIDTH_ASCII_CHAR:				[0xA3]		[0xC1-0xDA]
															[0xE1-0xFA]
												[0xA6]		[0xA1-0xB8]		Greek
															[0xC1-0xD8]
												[0xA7]		[0xA1-0xC1]		Cyrillic
															[0xD1-0xF1]		
												[0xA8]		[0xA1-0xBA]		European
			FULLWIDTH_PRONOUNCE_CHAR:			[0xA4,0xA5,0xA8] [xxxx]
			KANJI_CHAR:
		*/       
		if (c1 < 0x80)
            return  SEVEN_BIT_CHAR;

        c2 = *(pstr + 1);
		if (
			((c1== 0xA3) && (IN_BETWEEN(0xC1, c2, 0xDA) || IN_BETWEEN(0xE1, c2, 0xFA))) ||
			((c1== 0xA6) && (IN_BETWEEN(0xA1, c2, 0xB8) || IN_BETWEEN(0xC1, c2, 0xD8))) ||
			((c1== 0xA7) && (IN_BETWEEN(0xA1, c2, 0xC1) || IN_BETWEEN(0xD1, c2, 0xF1))) ||
			((c1== 0xA8) && (IN_BETWEEN(0xA1, c2, 0xBA)) ) 
		   )
        { 
            return FULLWIDTH_ASCII_CHAR;
        }

        if ((c1 == 0xA4) || (c1 == 0xA5) || (c1 == 0xA8)) 
            return FULLWIDTH_PRONOUNCE_CHAR;

		if (IN_BETWEEN(0xB0, c1, 0xF7))
            return KANJI_CHAR;

        return UNCLASSIFIED_CHAR;

    case CS_BIG5:
		/*
			SEVEN_BIT_CHAR:				[0x00-0x7F]
			HALFWIDTH_PRONOUNCE_CHAR:	
			FULLWIDTH_ASCII_CHAR:		[0xA2]		[0xCF-0xFF]
										[0xA3]		[0x40-0x73]
			FULLWIDTH_PRONOUNCE_CHAR:	[0xA3]		[0x74-0x7E]
													[0xA1-0xBF]
			KANJI_CHAR:					[0xA4-0xFF]	[xxxxxxxxx]
		*/        
		if (c1 < 0x80)
            return  SEVEN_BIT_CHAR;

        c2 = *(pstr + 1);

		switch(c1)
		{
			case 0xA2:
				if (IN_BETWEEN(0xCF, c2, 0xFF))
            		return FULLWIDTH_ASCII_CHAR;
				break;

			case 0xA3:
				if (IN_BETWEEN(0x74, c2, 0x7E) || IN_BETWEEN(0xA1, c2, 0xBF))
            		return FULLWIDTH_PRONOUNCE_CHAR;

				if (IN_BETWEEN(0x40, c2, 0x73)) 
            		return FULLWIDTH_ASCII_CHAR;

				break;
		}

        if (c1 >= 0xA4)
            return KANJI_CHAR;

        return UNCLASSIFIED_CHAR;

    case CS_CNS_8BIT:		/* TO BE TEST ON UNIX */
		/*
			SEVEN_BIT_CHAR:				[0x00-0x7F]
			HALFWIDTH_PRONOUNCE_CHAR:	
			FULLWIDTH_ASCII_CHAR:		[0xA4]		[0xC1-0xFE]	
										[0xA5]		[0xA1-0xC6]
			FULLWIDTH_PRONOUNCE_CHAR:	[0xA5]		[0xC7-0xF0]		
			KANJI_CHAR:					[0xC4-0xFF]	[xxxxxxxxx]		
										[0x8E]
		*/        
		if (c1 < 0x80)
            return  SEVEN_BIT_CHAR;

        c2 = *(pstr + 1);

		switch(c1)
		{
			case 0xA4:
				if(IN_BETWEEN(0xC1, c2, 0xFE))
            		return FULLWIDTH_ASCII_CHAR;
				break;

			case 0xA5:
				if(IN_BETWEEN(0xC7, c2, 0xF0)) 
            		return FULLWIDTH_PRONOUNCE_CHAR;

				if(IN_BETWEEN(0xA1, c2, 0xC6))
            		return FULLWIDTH_ASCII_CHAR;
				break;
		}

		if (IN_BETWEEN(0xC4, c1, 0x8E))
            return KANJI_CHAR;

        return UNCLASSIFIED_CHAR;  

    case CS_UTF8:
		/*
			SEVEN_BIT_CHAR:
			
			FULLWIDTH_ASCII_CHAR:
				U+0000 - U+10FF
									[C0-E0] [xxxx]				Done
									[E1]	[80-83]	[xxxx]		Done
				U+1E00 - U+1FFF
									[E1]	[B8-BF]				Done
				U+FF21 - U+FF3A
									[EF]	[BC]	[A1-BA]		Done
				U+FF41 - U+FF5A
									[EF]	[BD]	[81-9A]		Done
									
			FULLWIDTH_PRONOUNCE_CHAR:
				U+1100 - U+11FF
									[E1]	[84-87]				Done
				U+3040 - U+318F
									[E3]	[81-85] [xx]		Done
									[E3]	[86]    [80-8F]		Done
				U+FF66 - U+FFDC
									[EF]	[BD]    [AC-]
									[EF]	[BE]    
									[EF]	[BF]    [-9C]
				U+AC00 - U+D7FF
									[EA]	[B0-]				Done
									[EB-EC]	[xxx]				Done
									[ED]	[-9F]				Done

			KANJI_CHAR:
				U+4E00 - U+9FFF
									[E4]	[B8-]				Done
									[E5-E9] [xx]				Done
		*/        
		if (c1 < 0x80)
            return  SEVEN_BIT_CHAR;
		
		if (IN_BETWEEN(0xC0, c1, 0xE0))
		{
            return FULLWIDTH_ASCII_CHAR;
		}

        c2 = *(pstr + 1);

		switch(c1)
		{
			case 0xE1:
				if (IN_BETWEEN(0x80, c2, 0x83) || IN_BETWEEN(0xB8, c2, 0xBF))
            		return FULLWIDTH_ASCII_CHAR;
				if (IN_BETWEEN(0x84, c2, 0x87))
            		return FULLWIDTH_PRONOUNCE_CHAR;

				break;

			case 0xE3:
				if (IN_BETWEEN(0x81, c2, 0x85))
            		return FULLWIDTH_PRONOUNCE_CHAR;

				if (c2 == 0x86)
				{
        			c3 = *(pstr + 2);
					if (IN_BETWEEN(0x80, c3, 0x8F))
            			return FULLWIDTH_PRONOUNCE_CHAR;
				}
				
				break;

			case 0xE4:
				if (c2 >= 0xB8)
            		return KANJI_CHAR;
				break;

			case 0xE5: case 0xE6: case 0xE7: case 0xE8: case 0xE9:
            	return KANJI_CHAR;
				break;

			case 0xEA:
				if (c2 >= 0xB0)
            		return FULLWIDTH_PRONOUNCE_CHAR;
				break;

			case 0xEB: case 0xEC:
            	return FULLWIDTH_PRONOUNCE_CHAR;
				break;

			case 0xED:
				if (c2 <= 0x9F)
            		return FULLWIDTH_PRONOUNCE_CHAR;
				break;

			case 0xEF:
        		c3 = *(pstr + 2);
				switch(c2)
				{
					case 0xBC:
						if (IN_BETWEEN(0xA1, c3, 0xBA))
            				return FULLWIDTH_ASCII_CHAR;
						break;

					case 0xBD:
						if (IN_BETWEEN(0x81, c3, 0x9A))
            				return FULLWIDTH_ASCII_CHAR;
						if (c3 >= 0xAC)
   	         				return FULLWIDTH_PRONOUNCE_CHAR;
						break;

					case 0xBE:
            			return FULLWIDTH_PRONOUNCE_CHAR;
						break;

					case 0xBF:
						if (c3 <= 0x9C)
   	         				return FULLWIDTH_PRONOUNCE_CHAR;
						break;
				}
				break;
		}

        return UNCLASSIFIED_CHAR;  
	default:
		break;
	}

	return UNCLASSIFIED_CHAR;
}

#define IF_A_IN_ARRAY_B_THEN_RETURN_C(a,b,c)	\
	{	\
		int j;	\
        for (j = 0; (b)[j][0]; j++)	\
            if (XP_STRNCMP((char *)a, (b)[j], XP_STRLEN((b)[j])) == 0)	\
                return  (c);	\
	}

#define IF_PROHIBIT_CLASS_THEN_RETURN(a,ba,ea)	\
	{ \
		IF_A_IN_ARRAY_B_THEN_RETURN_C(a,ba,PROHIBIT_BEGIN_OF_LINE);	\
		IF_A_IN_ARRAY_B_THEN_RETURN_C(a,ea,PROHIBIT_END_OF_LINE);	\
	}

PUBLIC int INTL_KinsokuClass(int16 win_csid, unsigned char *pstr)
{
	switch (win_csid)
	{
	case CS_SJIS:
		IF_PROHIBIT_CLASS_THEN_RETURN(pstr,ProhibitBegin_SJIS,ProhibitEnd_SJIS);
		break;
	case CS_EUCJP:
		IF_PROHIBIT_CLASS_THEN_RETURN(pstr,ProhibitBegin_EUCJP,ProhibitEnd_EUCJP);
		break;
	case CS_GB_8BIT:
		IF_PROHIBIT_CLASS_THEN_RETURN(pstr,ProhibitBegin_GB,ProhibitEnd_GB);
		break;
	case CS_BIG5:
		IF_PROHIBIT_CLASS_THEN_RETURN(pstr,ProhibitBegin_BIG5,ProhibitEnd_BIG5);
		break;
	case CS_CNS_8BIT:
		IF_PROHIBIT_CLASS_THEN_RETURN(pstr,ProhibitBegin_CNS,ProhibitEnd_CNS);
		break;
	case CS_KSC_8BIT:
		IF_PROHIBIT_CLASS_THEN_RETURN(pstr,ProhibitBegin_KSC,ProhibitEnd_KSC);
		break;
	case CS_UTF8:
		IF_PROHIBIT_CLASS_THEN_RETURN(pstr,ProhibitBegin_UTF8,ProhibitEnd_UTF8);
		if( *pstr  <= 0xE2)	 /* UCS2 < 0x2000 */
			return PROHIBIT_WORD_BREAK;
		break;
	}

    return  PROHIBIT_NOWHERE;
}



