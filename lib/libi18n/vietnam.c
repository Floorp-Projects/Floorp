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
/*      vietnam.c       */
 
#include "intlpriv.h"
 
extern int MK_OUT_OF_MEMORY;
 
 
/**************************< vietnam.c >*******************************/
/*** Purpose: Please put all conversion routines that related to    ***/
/*** -------  the vietnamese character sets here for easy maintenance**/
/**********************************************************************/
 
 
/**********************************************************************/
/*** The following section handles conversion from VIQR to VISCII   ***/
/*** Implementer:  Vuong Nguyen (vlangsj@pluto.ftos.net)            ***/
/**********************************************************************/
/*** VISCII - VIQR - VISCII - VIQR - VISCII - VIQR - VISCII - VIQR  ***/
/**********************************************************************/
 
       /***************************************************/
       /*** For Mozilla, I disable the state feature of ***/
       /*** VIQR  (\V \M \L) by not defining TRUE_VIQR  ***/
       /***************************************************/
 
#ifdef TRUE_VIQR
PRIVATE unsigned char VIQRStateName[6] =
              /*----0----1----2----3----4----5----*/
              {    'v', 'V', 'm', 'M', 'l', 'L'   };
#endif
 
PRIVATE unsigned char DefaultVIQR[10] =
              /*----0----1----2----3----4----5----6----7----8----9---*/
              /*----'----`----?----~----.----(----^----+----d----\---*/
              {    39,  96,  63, 126,  46,  40,  94,  43, 100,  92   };
 
 
PRIVATE unsigned char HotChars[66] =
     /*----a----a(---a^---e----e^---i----o----o^---o+---u----u+---y----*/
     {    97, 229, 226, 101, 234, 105, 111, 244, 189, 117, 223, 121,
     /*----a'---a`---a?---a~---a.---e'---e`---e?---e~---e.--------*/
         225, 224, 228, 227, 213, 233, 232, 235, 168, 169,
     /*----o'---o`---o?---o~---o.---u'---u`---u?---u~---u.---d----*/
         243, 242, 246, 245, 247, 250, 249, 252, 251, 248, 100,
     /*----A----A(---A^---E----E^---I----O----O^---O+---U----U+---Y----*/
          65, 197, 194,  69, 202,  73,  79, 212, 180,  85, 191,  89,
     /*----A'---A`---A?---A~---A.---E'---E`---E?---E~---E.--------*/
         193, 192, 196, 195, 128, 201, 200, 203, 136, 137,
     /*----O'---O`---O?---O~---O.---U'---U`---U?---U~---U.---D----*/
         211, 210, 153, 160, 154, 218, 217, 156, 157, 158,  68    };
 
PRIVATE unsigned char CombiToneChar[5][24] =
     /***************************  Acute ' ******************************/
     { /*----a----a(---a^---e----e^---i----o----o^---o+---u----u+---y---*/
       {   225, 161, 164, 233, 170, 237, 243, 175, 190, 250, 209, 253,
       /*----A----A(---A^---E----E^---I----O----O^---O+---U----U+---Y---*/
           193, 129, 132, 201, 138, 205, 211, 143, 149, 218, 186, 221   },
     /***************************  Grave ` ******************************/
       /*----a----a(---a^---e----e^---i----o----o^---o+---u----u+---y---*/
       {   224, 162, 165, 232, 171, 236, 242, 176, 182, 249, 215, 207,
       /*----A----A(---A^---E----E^---I----O----O^---O+---U----U+---Y---*/
           192, 130, 133, 200, 139, 204, 210, 144, 150, 217, 187, 159   },
     /***************************  Hook Above ? *************************/
       /*----a----a(---a^---e----e^---i----o----o^---o+---u----u+---y---*/
       {   228, 198, 166, 235, 172, 239, 246, 177, 183, 252, 216, 214,
       /*----A----A(---A^---E----E^---I----O----O^---O+---U----U+---Y---*/
           196,   2, 134, 203, 140, 155, 153, 145, 151, 156, 188,  20   },
     /***************************  Tilde ~ ******************************/
       /*----a----a(---a^---e----e^---i----o----o^---o+---u----u+---y---*/
       {   227, 199, 231, 168, 173, 238, 245, 178, 222, 251, 230,  219,
       /*----A----A(---A^---E----E^---I----O----O^---O+---U----U+---Y---*/
           195,   5,   6, 136, 141, 206, 160, 146, 179, 157, 255,  25   },
     /***************************  Dot Below . **************************/
       /*----a----a(---a^---e----e^---i----o----o^---o+---u----u+---y---*/
       {   213, 163, 167, 169, 174, 184, 247, 181, 254, 248, 241,  220,
       /*----A----A(---A^---E----E^---I----O----O^---O+---U----U+---Y---*/
           128, 131, 135, 137, 142, 152, 154, 147, 148, 158, 185,  30   }
     };
 
PRIVATE unsigned char CombiModiChar[3][66] =
     /***************************  Breve ( ******************************/
     { /*----a----a(---a^---e----e^---i----o----o^---o+---u----u+---y---*/
       {   229,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       /*----a'---a`---a?---a~---a.---e'---e`---e?---e~---e.--------*/
           161, 162, 198, 199, 163,   0,   0,   0,   0,   0,
       /*----o'---o`---o?---o~---o.---u'---u`---u?---u~---u.---d----*/
             0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       /*----A----A(---A^---E----E^---I----O----O^---O+---U----U+---Y---*/
           197,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       /*----A'---A`---A?---A~---A.---E'---E`---E?---E~---E.--------*/
           129, 130,   2,   5, 131,   0,   0,   0,   0,   0,
       /*----O'---O`---O?---O~---O.---U'---U`---U?---U~---U.---D----*/
             0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0    },
     /***************************  Circumflex ^ *************************/
       /*----a----a(---a^---e----e^---i----o----o^---o+---u----u+---y---*/
       {   226,   0,   0, 234,   0,   0, 244,   0,   0,   0,   0,   0,
       /*----a'---a`---a?---a~---a.---e'---e`---e?---e~---e.--------*/
           164, 165, 166, 231, 167, 170, 171, 172, 173, 174,
       /*----o'---o`---o?---o~---o.---u'---u`---u?---u~---u.---d----*/
           175, 176, 177, 178, 181,   0,   0,   0,   0,   0,   0,
       /*----A----A(---A^---E----E^---I----O----O^---O+---U----U+---Y---*/
           194,   0,   0, 202,   0,   0, 212,   0,   0,   0,   0,   0,
       /*----A'---A`---A?---A~---A.---E'---E`---E?---E~---E.--------*/
           183, 133, 134,   6, 135, 138, 139, 140, 141, 142,
       /*----O'---O`---O?---O~---O.---U'---U`---U?---U~---U.---D----*/
           143, 144, 145, 146, 147,   0,   0,   0,   0,   0,   0    },
     /***************************  Horn + *******************************/
       /*----a----a(---a^---e----e^---i----o----o^---o+---u----u+---y---*/
       {     0,   0,   0,   0,   0,   0, 189,   0,   0, 223,   0,   0,
       /*----a'---a`---a?---a~---a.---e'---e`---e?---e~---e.--------*/
             0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       /*----o'---o`---o?---o~---o.---u'---u`---u?---u~---u.---d----*/
           190, 182, 183, 222, 254, 209, 215, 216, 230, 241,   0,
       /*----A----A(---A^---E----E^---I----O----O^---O+---U----U+---Y---*/
             0,   0,   0,   0,   0,   0, 180,   0,   0, 191,   0,   0,
       /*----A'---A`---A?---A~---A.---E'---E`---E?---E~---E.--------*/
             0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       /*----O'---O`---O?---O~---O.---U'---U`---U?---U~---U.---D----*/
           149, 150, 151, 179, 148, 186, 187, 188, 255, 185,   0    }
     };
 
PRIVATE uint16 VIS_VIQ[128] =          /*** 128 to 255 ***/
     { 4161, 8513, 8769,12353,16705,16961,17473,20545, 2117, 4165,16709,16965,
      17477,18501,20549,16719,16975,17487,18511,20559,36943,33103,33359,33871,
       4169, 1103, 4175, 1097, 1109, 2133, 4181,  601, 2127, 8545, 8801,12385,
      16737,16993,17505,20577, 2149, 4197,16741,16997,17509,18533,20581,16751,
      17007,17519,18543,34895,32847,20591,33391,33903, 4201,36949,33109,33365,
      33877,32879,33135,32853,  577,  321,16449, 2113, 1089, 8257, 9313,10337,
        581,  325,16453, 1093,  585,  329, 2121,  633,    0,33141,  591,  335,
      16463, 4193, 1145,33397,33909,  597,  341, 2169, 4217,  345,34927,32885,
        609,  353,16481, 2145, 1121, 8289,34933,18529,  613,  357,16485, 1125,
        617,  361, 2153, 1129,    0,36981,  623,  367,16495, 2159, 1135, 4207,
       4213,  629,  373, 2165, 1141,  377,36975,34901 };
 
 
PRIVATE unsigned char VNI_VIS[256] =
     {   0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
        16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
        32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
        48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
        64,255, 66, 67, 68,255, 70, 71, 72, 73, 74, 75, 76, 77, 78,255,
        80, 81, 82, 83, 84,255, 86, 87, 88,255, 90, 91, 92, 93, 94, 95,
        96,255, 98, 99,100,255,102,103,104,105,106,107,108,109,110,255,
       112,113,114,115,116,255,118,119,120,255,122,123,124,125,126,127,
       128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,
       144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,
       160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,
       176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,
       192,193,194,195,196,197,155,199,200,201,202,203,204,205, 30,207,
       208,208,152,206,255,213,255,215,216,217,218,219,220,221,222,223,
       224,225,226,227,228,229,239,231,232,233,234,235,236,237,220,239,
       240,240,184,238,255,245,255,247,248,249,250,251,252,253,254,220 };
 
PRIVATE int32 VNIextended_VIS[118] =
     { 192133,193132,194194,195006,196135,197134,200130,201129,
       202197,203131,207128,213195,216192,217193,218002,219196,
       220005,192139,193138,194202,195141,196142,197140,207137,
       213136,216200,217201,219203,192144,193143,194212,195146,
       196147,197145,207154,213160,216210,217211,219153,207158,
       213157,216217,217218,219156,207030,213025,216159,217221,
       219020,224165,225164,226226,227231,228167,229166,232162,
       233161,234229,235163,239213,245227,248224,249225,250198,
       251228,252199,224171,225170,226234,227173,228174,229172,
       239169,245168,248232,249233,251235,224176,225175,226244,
       227178,228181,229177,239247,245245,248242,249243,251246,
       239248,245251,248249,249250,251252,239220,245219,248207,
       249253,251214,207148,213179,216150,217149,219151,207185,
       213255,216187,217186,219188,239254,245222,248182,249190,
       251183,239241,245230,248215,249209,251216 };
 
PRIVATE uint16 VIS_VNI[128] =          /*** 128 to 255 ***/
     { 0xcf41,0xc941,0xc841,0xcb41,0xc141,0xc041,0xc541,0xc441,
       0xd545,0xcf45,0xc145,0xc045,0xc545,0xc345,0xc445,0xc14f,
       0xc04f,0xc54f,0xc34f,0xc44f,0xcfd4,0xd9d4,0xd8d4,0xdbd4,
       0x00d2,0xdb4f,0xcf4f,0x00c6,0xdb55,0xd555,0xcf55,0xd859,
       0xd54f,0xe961,0xe861,0xeb61,0xe161,0xe061,0xe561,0xe461,
       0xf565,0xef65,0xe165,0xe065,0xe565,0xe365,0xe465,0xe16f,
       0xe06f,0xe56f,0xe36f,0xd5d4,0x00d4,0xe46f,0xf8f4,0xfbf4,
       0x00f2,0xcfd6,0xd9d6,0xd8d6,0xdbd6,0x00f4,0xf9f4,0x00d6,
       0xd841,0xd941,0xc241,0xd541,0xdb41,0xca41,0xfa61,0xfc61,
       0xd845,0xd945,0xc245,0xdb45,0x00cc,0x00cd,0x00d3,0xf879,
       0x00d1,0xf9f6,0xd84f,0xd94f,0xc24f,0xef61,0xfb79,0xf8f6,
       0xfbf6,0xd855,0xd955,0xf579,0x00ee,0xd959,0xf5f4,0x00f6,
       0xf861,0xf961,0xe261,0xf561,0xfb61,0xea61,0xf5f6,0xe361,
       0xf865,0xf965,0xe265,0xfb65,0x00ec,0x00ed,0x00f3,0x00e6,
       0x00f1,0xeff6,0xf86f,0xf96f,0xe26f,0xf56f,0xfb6f,0xef6f,
       0xef75,0xf875,0xf975,0xf575,0xfb75,0xf979,0xeff4,0xd5d6 };
 
PRIVATE unsigned char cVIQRType( unsigned char c);
PRIVATE unsigned char IsVIQRWord(const unsigned char *s,
				int32 len_s,
				int32 fromI,
				int32 *toI);
PRIVATE unsigned char C_VIQR_to_VISCII( unsigned char *HotKeys,
					unsigned char *CurState,
					unsigned char *LastChar,
					unsigned char  NewChar);
PRIVATE unsigned char C_VISCII_to_VIQR( unsigned char *HotKeys,
					unsigned char VISCII,
					unsigned char *char1,
					unsigned char *char2,
					unsigned char *char3);
PRIVATE int32 S_VIQR_to_VISCII( const unsigned char *frombuffer,
				unsigned char *tobuffer,
				int32 maxsz);
PRIVATE int32 S_VISCII_to_VIQR( const unsigned char *frombuffer,
				unsigned char *tobuffer,
				int32 maxsz);
PRIVATE int32 S_VNI_to_VISCII(  const unsigned char *frombuffer,
				unsigned char *tobuffer,
				int32 maxsz);
PRIVATE int32 S_VISCII_to_VNI( 	const unsigned char *frombuffer,
				unsigned char *tobuffer,
				int32 maxsz);
PRIVATE unsigned char *xlat_vis_ucs2_X(CCCDataObject obj,
                                              unsigned char *buf);
PRIVATE unsigned char *xlat_X_ucs2_vis(CCCDataObject obj,
                                              const unsigned char *buf,
                                              int32 bufsz);
PRIVATE unsigned char *xlat_viqr_2_viscii (
                                          CCCDataObject obj,
                                          const unsigned char *viqr_buf,
                                          int32 viqr_bufsz);
PRIVATE unsigned char *xlat_vni_2_viscii (
                                         CCCDataObject obj,
                                         const unsigned char *vni_buf,
                                         int32 vni_bufsz);
PRIVATE unsigned char *xlat_viscii_2_viqr (
                                          CCCDataObject obj,
                                          const unsigned char *vis_buf,
                                          int32 vis_bufsz);
PRIVATE unsigned char *xlat_viscii_2_vni ( CCCDataObject obj,
                                          const unsigned char *vis_buf,
                                          int32 vis_bufsz);
/**********************************************************************/
/*** Function: cVIQRType()                                          ***/
/*** Purpose:  Determine the type of a VIQR character.              ***/
/*** Returns:  0 - If c is not a VIQR character.                    ***/
/***           1 - If c is between A..Z, a..z                       ***/
/***           2 - If c is a diacritical mark                       ***/
/**********************************************************************/
PRIVATE unsigned char cVIQRType( unsigned char c)
{   if ((c < 0x27) || (c > 0x7e))  /*** below ' or above ~ ***/
           return(0);
    else { 
#if 0
 	   unsigned char s[2];
           s[0] = c;
           s[1] = 0;
           if (strcspn(s,"),-/0123456789:;<=>@[\\]_{|}") == 0) return(0);
#endif
           if (NULL == XP_STRCHR("),-/0123456789:;<=>@[\\]_{|}",c)) return(0);
           if (XP_IS_ALPHA(c)) return(1);
           else            return(2);
    }
}
 
/**********************************************************************/
/*** Function: IsVIQRWord()                                         ***/
/*** Purpose:  Parse a string, marks the location of the first word.***/
/*** Returns:  0: if the word is not a VIQR word                    ***/
/*** Implementer:    Vuong Nguyen (vlangsj@pluto.ftos.net)          ***/
/**********************************************************************/
PRIVATE unsigned char IsVIQRWord(const unsigned char *s,
				int32 len_s,
				int32 fromI,
				int32 *toI)
{   unsigned char c, t, flg;
    int32 i, ctr_letter, ctr_mark, ctr_afterperiod;
 
    flg = 0;
    ctr_afterperiod = - len_s;
    ctr_letter = ctr_mark = 0;
    for (i=fromI; ((i < len_s) && ('\0' != (c = s[i]))); i++) {
         t = cVIQRType(c);
         if (t == 0)
                flg = 1;        /**If it is not-viqr, mark the flag. **/
         else { if (flg == 1) { /**And if after the not-viqr is viqr,**/
                    break;      /**then done **/
                }
                if (t == 1) {
                       ctr_letter++;       /** Count the # of letters  **/
                       ctr_afterperiod++;
                }
                else { /** If a mark appears before any letter, ignore **/
                       if (ctr_letter > 0) ctr_mark++;
                       if (c == '.') { /** Pay attention to the period **/
                           if (ctr_afterperiod < 0) ctr_afterperiod=0;
                       }
                }
         }
    }
    *toI = i-1; /*** So the first word will be between fromI and toI ***/
 
    /*******************************************************************/
    /*** Now, the hard part, I should check the semantic of the word,***/
    /*** But it will take more comparisions, and I don't think it is ***/
    /*** necessary, so I temporarily use the common sense here to    ***/
    /*** detect a vietnamese word.  I'll implement semantic checking ***/
    /*** later if it's really necessary                              ***/
    if (*toI > fromI) {
        /*********************************************************/
        /**First: If there are more 2 letters after a "." -> NO **/
        if (ctr_afterperiod > 2) return(0);
        /*********************************************************/
        /**Then: Check for special cases of DD, Dd, dD, dd     ***/
        c = s[fromI];
        if ((c == 'D') || (c == 'd')) {
             c = s[fromI+1];
             if ((c == 'D') || (c == 'd')) {
                  ctr_letter--;
                  if (ctr_mark == 0) ctr_mark++;
             }
        }
        /*********************************************************/
        /**Then: Check for common knowledge: A vietnamese word ***/
        /**always has less than 8 letters and less than 4      ***/
        /**diacritical marks at the same time                  ***/
        if ((ctr_letter > 0) && (ctr_letter < 8)) {
             if ((ctr_mark > 0) && (ctr_mark < 4)) return(1);
        }
    }
    return(0);
}
 
/*********************************************************************/
/*** Function : C_VIQR_to_VISCII()                                 ***/
/*** Purpose:   Given 2 VIQR characters, combine into 1 VISCII     ***/
/*** -------    character if possible                              ***/
/*** Return:  0-FALSE: Couldn't combined.                          ***/
/*** ------:  1-TRUE:  if LastChar is modified to be a VISCII char ***/
/***          2-TRUE:  A state is changing (return BackSpace)      ***/
/*** Implementer:    Vuong Nguyen (vlangsj@pluto.ftos.net)         ***/
/*********************************************************************/
PRIVATE unsigned char C_VIQR_to_VISCII( unsigned char *HotKeys,
					unsigned char *CurState,
					unsigned char *LastChar,
					unsigned char  NewChar)
{   unsigned char idkey, idchar, ochar, nchar, converted;
 
    converted = 0;
    ochar=*LastChar;
 
#ifdef TRUE_VIQR
    unsigned char statechange, cs;
    /***********************************************************/
    statechange=(ochar==HotKeys[9]);
    cs = *CurState;
    if (cs > 0) {
        if  (cs >= 'a')  {
             cs -= 32;  /*** Convert from lowercase to uppercase ***/
            *CurState = cs;
            *LastChar=NewChar;
             return(1);
        }
        else if (statechange)  {
                 for (idkey=0;idkey < 6; idkey++)
                      if (NewChar==VIQRStateName[idkey]) break;
                 statechange=(idkey & 6);
                 if (statechange < 6) { /**A state change is detected**/
                     cs = VIQRStateName[statechange | 1];
                    *CurState=cs;
                    *LastChar=8;
                     return(2);
                 }
             }
    }
    /********************************************************************/
    if      (cs==VIQRStateName[5]) { } /******** Literal **************/
    else if (cs==VIQRStateName[3]) {   /******** English **************/
                       if (statechange) { cs = '\\';
                                         *CurState=cs;
                                         *LastChar=NewChar;
                                          converted=1;
                       }
         }
    else
#endif
         { /*************************************************************/
           /******** Vietnamese (V) or English+ESC (\) mode *************/
           /*************************************************************/
           /****** Solve the problem of DD (4 different cases ***********/
           nchar = NewChar;
           switch (nchar) {
                case 'D': /*** if (HotKeys[8]=='d') ***/
                          nchar = 'd';
                case 'd': if (ochar == '-') ochar = nchar;
                          break;
                case '*': nchar = '+'; break;
           }
           /*********************************************************/
           for (idkey=0; idkey < 9; idkey++)
                if (nchar == HotKeys[idkey]) {
#ifdef TRUE_VIQR
                    if (statechange) /*** Escape character ***/
                         converted=1;
                    else
#endif
                         for (idchar=0; idchar < 66; idchar++)
                              if (ochar==HotChars[idchar]) {
                                  if (idkey < 5) {
                                       /******** For '`?~. tones ****/
                                       ochar=idchar % 33;
                                       if (ochar < 12)
                                            NewChar=CombiToneChar[idkey]
                                                [ochar+(idchar / 33)*12];
                                       else NewChar=0;
                                  }    /*****************************/
                                  else
                                  if (idkey==8) { /****** For DD ****/
                                       if ((idchar % 33) == 32)
                                            NewChar=ochar+140;
                                       else NewChar=0;
                                  }    /*****************************/
                                  else /****** For ^(+ modifiers ****/
                                       NewChar=CombiModiChar[idkey-5]
                                                            [idchar];
                                  converted=(NewChar != 0);
                                  break;
                              }
                    if (converted) *LastChar=NewChar;
                    break;
                }
#ifdef TRUE_VIQR
           if ((!converted) && (cs=='\\')) *CurState=VIQRStateName[3];
#endif
         }
    /********************************************************************/
    return(converted);
}
 
/*********************************************************************/
/*** Function : C_VISCII_to_VIQR()                                 ***/
/*** Purpose:   Given 1 VISCII character, split to VIQR chars      ***/
/*** Return:    Number of returned characters.                     ***/
/*** Implementer: Vuong Nguyen (vlangsj@pluto.ftos.net)            ***/
/*********************************************************************/
PRIVATE unsigned char C_VISCII_to_VIQR( unsigned char *HotKeys,
					unsigned char VISCII,
					unsigned char *char1,
					unsigned char *char2,
					unsigned char *char3)
{   uint16 key;
    unsigned char mask, NumChars;
    NumChars=1;
    *char2=*char3=0;
    key=VISCII;
    /******************* Get the secret code *************************/
    if (VISCII < 128) {
        if (VISCII < 32)
            switch (VISCII) {
               case 2 : key= 9281; break;    case 20 : key=1113; break;
               case 5 : key=10305; break;    case 25 : key=2137; break;
               case 6 : key=18497; break;    case 30 : key=4185; break;
            }
    }
    else if ((VISCII==240) || (VISCII==208)) {
              key-=140;
              if (HotKeys[8]=='d') *char2=(unsigned char) key;
              else                 *char2=HotKeys[8];
              NumChars++;
         }
         else key=VIS_VIQ[VISCII-128];
    /*****************************************************************/
    *char1=key & 0xFF;      /*** Get the base letter *******/
    if ((key >>= 8))        /*** Get the diacritic marks ***/
         for (mask=0x80,VISCII=7; key ; mask >>= 1, VISCII--)
              if (key & mask) {
                  if (++NumChars ==2) *char2=HotKeys[VISCII];
                  else                *char3=HotKeys[VISCII];
                  key &= (~mask);
              }
    return(NumChars);
}
 
/**********************************************************************/
/*** Procedure : S_VIQR_to_VISCII()                                 ***/
/*** Purpose   : Convert a VIQR string to a VISCII string.          ***/
/*** -------     This routine will not convert any word that seems  ***/
/***             not to be a Vietnamese word. Doing this, we will   ***/
/***             avoid the problem of blind converting email, url,  ***/
/***             html tags.                                         ***/
/***             I would suggest all other vietnamese encodings     ***/
/***             using this routine to convert from VIQR to their   ***/
/***             codes (using VISCII as intermediate code)          ***/
/***             eg:   VIQR -> VISCII -> Vxxx                       ***/
/***             With this technique, then we will not need many    ***/
/***             (duplicate) VIQR conversion routines.              ***/
/***             Please see viet_viqr_2_vps() below as an example   ***/
/*** Implementer:    Vuong Nguyen (vlangsj@pluto.ftos.net)          ***/
/**********************************************************************/
PRIVATE int32 S_VIQR_to_VISCII( const unsigned char *frombuffer,
				unsigned char *tobuffer,
				int32 maxsz)
{  unsigned char oldc, newc, InitState, j;
   int32 fromI, toI, ctr;
 
   ctr=0;
   InitState = 0;
   fromI = 0;
   while (fromI < maxsz) {
      if ( IsVIQRWord(frombuffer, maxsz,fromI,&toI) ) {
             /*******************************************************/
             /*** This is a VIQR word, need conversion **************/
             j = 0;
             oldc = frombuffer[fromI++];
             while ((fromI <= toI) && ('\0' != (newc = frombuffer[fromI]))) {
                j = C_VIQR_to_VISCII(DefaultVIQR,&InitState,&oldc,newc);
                if (j == 0) {
                    tobuffer[ctr++] = oldc;
                    oldc = newc;
                }
                fromI++;
             }
             if (j != 2) tobuffer[ctr++] = oldc;
      }
      else { /*******************************************************/
             /*** This is not a VIQR word, simply copy it ***********/
             while ((fromI <= toI) && ('\0' != (newc = frombuffer[fromI]))) {
                tobuffer[ctr++] = newc;
                fromI++;
             }
      }
   }
   tobuffer[ctr]=0;
   return(ctr);
}
 
/**********************************************************************/
/*** Procedure :  S_VISCII_to_VIQR()                                ***/
/*** Purpose   :  Convert a VISCII string to a VIQR string.         ***/
/*** IMPORTANT :  The size of tobuffer must be bigger than the size ***/
/*** ---------    of frombuffer about 3 times (worst case)          ***/
/*** Implementer: Vuong Nguyen (vlangsj@pluto.ftos.net)             ***/
/**********************************************************************/
PRIVATE int32 S_VISCII_to_VIQR( const unsigned char *frombuffer,
				unsigned char *tobuffer,
				int32 maxsz)
{  int32 indx, i, n, ctr;
   unsigned char c, cc[3];
 
   indx = ctr = 0;
   for (indx=0;((indx < maxsz) && ('\0' != (c=frombuffer[indx])));indx++) {
          n = C_VISCII_to_VIQR(DefaultVIQR,c,cc,cc+1,cc+2);
          if ((n==2) && (cc[1]=='d')) cc[1]=cc[0];
          for (i=0; (i < n); i++) tobuffer[ctr++] = cc[i];
   }
   tobuffer[ctr]=0;
   return(ctr);
}
 
/**********************************************************************/
/*** Function: S_VNI_to_VISCII()                                    ***/
/*** Purpose:  Converts a VNI string to a VISCII string             ***/
/*** Implementer:    Vuong Nguyen (vlangsj@pluto.ftos.net)          ***/
/**********************************************************************/
PRIVATE int32 S_VNI_to_VISCII(  const unsigned char *frombuffer,
				unsigned char *tobuffer,
				int32 maxsz)
{    unsigned char  c, v;
     int32 i, j, fr_i, to_i, x, ctr, idex;
 
     ctr = 0;
     for (i = 0;(i < maxsz) && ('\0' != (c = (unsigned char) frombuffer[i])); i++) {
          v = VNI_VIS[c];
          if (v != 255) tobuffer[ctr++] = v;
          else {
                 switch (c) {
                    case  65: idex =     16; break;
                    case  69: idex =  17027; break;
                    case  79: idex =  28038; break;
                    case  85: idex =  39043; break;
                    case  89: idex =  44048; break;
                    case  97: idex =  49065; break;
                    case 101: idex =  66076; break;
                    case 111: idex =  77087; break;
                    case 117: idex =  88092; break;
                    case 121: idex =  93097; break;
                    case 212: idex =  98102; break;
                    case 214: idex = 103107; break;
                    case 244: idex = 108112; break;
                    case 246: idex = 113117; break;
                    default:  idex = 0;
                 }
                 if (idex != 0) {
                     if ('\0' != (v = (unsigned char) frombuffer[i+1])) {
                          fr_i = (int)(idex / 1000);
                          to_i = (int)(idex % 1000);
                          for (j = fr_i; j <= to_i; j++) {
                               idex = VNIextended_VIS[j];
                               x = (int)(idex / 1000);
                               if (x == v) {
                                   x = (int)(idex % 1000);
                                   c = (unsigned char) x;
                                   i++;
                                   break;
                               }
                          }
                          if (j > to_i) {
                              switch (c) {
                                case 212: c = 180; break;
                                case 214: c = 191; break;
                                case 244: c = 189; break;
                                case 246: c = 223; break;
                              }
                          }
                     }
                 }
                 tobuffer[ctr++] = c;
          }
     }
     tobuffer[ctr] = 0;
     return(ctr);
}
 
/**********************************************************************/
/*** Function: S_VISCII_to_VNI()                                    ***/
/*** Purpose:  Converts a VISCII string to a VNI string             ***/
/*** Note:     tobuffer must have size at least 2 times bigger than ***/
/*** ----      than frombuffer (1-byte charset -> 2-byte charset)   ***/
/*** Implementer:    Vuong Nguyen (vlangsj@pluto.ftos.net)          ***/
/**********************************************************************/
PRIVATE int32 S_VISCII_to_VNI( 	const unsigned char *frombuffer,
				unsigned char *tobuffer,
				int32 maxsz)
{    unsigned char  c;
     uint16 idex;
     int32 i, ctr;
 
     ctr = 0;
     for (i = 0;(i < maxsz) && ('\0' != (c = (unsigned char) frombuffer[i])); i++) {
          idex = 0;
          if (c < 32)
               switch (c) {
                   case  2: idex = 0xda41; break;
                   case  5: idex = 0xdc41; break;
                   case  6: idex = 0xc341; break;
                   case 20: idex = 0xdb59; break;
                   case 25: idex = 0xd559; break;
                   case 30: c = 0xCE;      break;
               }
          else if (c > 127) idex = VIS_VNI[c-128];
          if (idex) {
               tobuffer[ctr++] = (unsigned char) (idex & 0xFF);
               c = (unsigned char) (idex >> 8);
               if (c) tobuffer[ctr++] = c;
          }
          else tobuffer[ctr++] = c;
     }
     tobuffer[ctr] = 0;
     return(ctr);
}
 
/**********************************************************************/
/*** Function:  xlat_vis_ucs2_X()                                   ***/
/*** Purpose:   Converts a converted temporary VISCII buffer to a   ***/
/*** -------    to_charset via mz_AnyToAnyThroughUCS2               ***/
/**********************************************************************/
PRIVATE unsigned char *xlat_vis_ucs2_X(CCCDataObject obj,
                                              unsigned char *buf)
{   unsigned char *temp_buf;
    if (buf) {
        INTL_SetCCCFromCSID(obj, CS_VIET_VISCII);
        temp_buf = (unsigned char *) mz_AnyToAnyThroughUCS2(obj, buf, INTL_GetCCCLen(obj));
        XP_FREE(buf);
        buf = temp_buf;
    }
    return (buf);
}
 
/**********************************************************************/
/*** Function:  xlat_X_ucs2_vis()                                   ***/
/*** Purpose:   Converts a from_charset buffer to a temporary       ***/
/*** -------    VISCII buffer via mz_AnyToAnyThroughUCS2            ***/
/**********************************************************************/
PRIVATE unsigned char *xlat_X_ucs2_vis(CCCDataObject obj,
                                              const unsigned char *buf,
                                              int32 bufsz)
{   unsigned char *out_buf;
    INTL_SetCCCToCSID(obj, CS_VIET_VISCII);
    out_buf = (unsigned char *) mz_AnyToAnyThroughUCS2(obj, buf, bufsz);
    return (out_buf);
}
 
/**********************************************************************/
/*** Function:  xlat_viqr_2_viscii()                                ***/
/*** Purpose:   Converts a VIQR buffer to a VISCII buffer           ***/
/*** Arguments:   viqr_buf:     Ptr to a buf of VIQR chars          ***/
/*** ---------    viqr_bufsize: Size in bytes of viqr_buf           ***/
/***              uncvtbuf:     We temparoraly ignore the           ***/
/***                            un-converted buffer now.            ***/
/*** Returns:   Returns NULL on failure, otherwise it returns a     ***/
/*** -------    pointer to a buffer converted characters.           ***/
/***            Caller must XP_FREE() this memory.                  ***/
/*** Implementer:    Vuong Nguyen (vlangsj@pluto.ftos.net)          ***/
/**********************************************************************/
PRIVATE unsigned char *xlat_viqr_2_viscii (
                                          CCCDataObject obj,
                                          const unsigned char *viqr_buf,
                                          int32 viqr_bufsz)
{
    unsigned char *out_buf;
    int32          out_bufsz;
 
    out_bufsz = viqr_bufsz + 1; /* VIQR buffer always >= VISCII buffer */
    if ((out_buf = (unsigned char *)XP_ALLOC(out_bufsz)) == (unsigned char *)NULL) {
            INTL_SetCCCRetval(obj, MK_OUT_OF_MEMORY);
            return(NULL);
    }
    out_bufsz = S_VIQR_to_VISCII( viqr_buf, out_buf, viqr_bufsz);
    INTL_SetCCCLen(obj, out_bufsz);   /* length not counting null */
    return(out_buf);
}
 
/**********************************************************************/
/*** Function:  xlat_vni_2_viscii()                                 ***/
/*** Purpose:   Converts a VNI buffer to VISCII buffer              ***/
/*** Arguments:   vni_buf:      Ptr to a buf of VNI chars           ***/
/*** ---------    vni_bufsize:  Size in bytes of vni_buf            ***/
/***              uncvtbuf:     We temparoraly ignore the           ***/
/***                            un-converted buffer now.            ***/
/*** Returns:   Returns NULL on failure, otherwise it returns a     ***/
/*** -------    pointer to a buffer converted characters.           ***/
/***            Caller must XP_FREE() this memory.                  ***/
/*** Implementer:    Vuong Nguyen (vlangsj@pluto.ftos.net)          ***/
/**********************************************************************/
PRIVATE unsigned char *xlat_vni_2_viscii (
                                         CCCDataObject obj,
                                         const unsigned char *vni_buf,
                                         int32 vni_bufsz)
{
    unsigned char *vis_buf = NULL;
    int32          vis_bufsz;
 
    vis_bufsz = vni_bufsz + 1; /* VNI buffer always >= VISCII buffer */
    if ((vis_buf = (unsigned char *)XP_ALLOC(vis_bufsz)) == (unsigned char *)NULL) {
            INTL_SetCCCRetval(obj, MK_OUT_OF_MEMORY);
            return(NULL);
    }
    vis_bufsz = S_VNI_to_VISCII( vni_buf, vis_buf, vni_bufsz);
    INTL_SetCCCLen(obj, vis_bufsz);   /* length not counting null */
    return(vis_buf);
}
 
/**********************************************************************/
/*** Function:  xlat_viscii_2_vni()                                 ***/
/*** Purpose:   Converts a VISCII buffer to VNI buffer              ***/
/*** Arguments:   vis_buf:     Ptr to a buf of VISCII chars         ***/
/*** ---------    vis_bufsize: Size in bytes of vis_buf             ***/
/***              uncvtbuf:    We temparoraly ignore the            ***/
/***                           un-converted buffer now.             ***/
/*** Returns:   Returns NULL on failure, otherwise it returns a     ***/
/*** -------    pointer to a buffer converted characters.           ***/
/***            Caller must XP_FREE() this memory.                  ***/
/*** Implementer:    Vuong Nguyen (vlangsj@pluto.ftos.net)          ***/
/**********************************************************************/
PRIVATE unsigned char *xlat_viscii_2_vni (
                                          CCCDataObject obj,
                                          const unsigned char *vis_buf,
                                          int32 vis_bufsz)
{
    unsigned char *vni_buf = NULL;
    int32          vni_bufsz;
 
    /* Need to double the buffer, since VNI is (1 to 2)-bytes charset */
    vni_bufsz = (vis_bufsz * 2) + 1;
    if ((vni_buf = (unsigned char *)XP_ALLOC(vni_bufsz)) == (unsigned char *)NULL) {
            INTL_SetCCCRetval(obj, MK_OUT_OF_MEMORY);
            return(NULL);
    }
    vni_bufsz = S_VISCII_to_VNI( vis_buf, vni_buf, vis_bufsz);
    INTL_SetCCCLen(obj, vni_bufsz);   /* length not counting null */
    return(vni_buf);
}
 
/**********************************************************************/
/*** Function:  xlat_viscii_2_viqr()                                ***/
/*** Purpose:   Converts a VISCII buffer to VIQR buffer             ***/
/*** Arguments:   vis_buf:     Ptr to a buf of VISCII chars         ***/
/*** ---------    vis_bufsize: Size in bytes of vis_buf             ***/
/***              uncvtbuf:    We temparoraly ignore the            ***/
/***                           un-converted buffer now.             ***/
/*** Returns:   Returns NULL on failure, otherwise it returns a     ***/
/*** -------    pointer to a buffer converted characters.           ***/
/***            Caller must XP_FREE() this memory.                  ***/
/*** Implementer:    Vuong Nguyen (vlangsj@pluto.ftos.net)          ***/
/**********************************************************************/
PRIVATE unsigned char *xlat_viscii_2_viqr (
                                          CCCDataObject obj,
                                          const unsigned char *vis_buf,
                                          int32 vis_bufsz)
{
    unsigned char *viqr_buf = NULL;
    int32          viqr_bufsz;
 
    /* The worst case will be a VISCII string contains all */
    /* 2-diacritical marks (eg: A^'), so we must reserve a */
    /* output VIQR buffer that at least 3 times bigger     */
    viqr_bufsz = (vis_bufsz * 3) + 1;
    if ((viqr_buf = (unsigned char *)XP_ALLOC(viqr_bufsz)) == (unsigned char *)NULL) {
            INTL_SetCCCRetval(obj, MK_OUT_OF_MEMORY);
            return(NULL);
    }
    viqr_bufsz = S_VISCII_to_VIQR( vis_buf, viqr_buf, vis_bufsz);
    INTL_SetCCCLen(obj, viqr_bufsz);   /* length not counting null */
    return(viqr_buf);
}
 
/*####################################################################*/
/*### The following routines are called by Mozilla                 ###*/
/*####################################################################*/
 
/**********************************************************************/
/*** Function:  viet_viqr_2_any()                                   ***/
/*** Purpose:   Converts a VIQR buffer to the destination charset   ***/
/**********************************************************************/
MODULE_PRIVATE unsigned char *viet_viqr_2_any (
                                          CCCDataObject obj,
                                          const unsigned char *viqr_buf,
                                          int32 viqr_bufsz)
{
    unsigned char *out_buf;
 
    out_buf = xlat_viqr_2_viscii(obj,viqr_buf,viqr_bufsz);
    if (INTL_GetCCCToCSID(obj) != CS_VIET_VISCII)
        out_buf = xlat_vis_ucs2_X(obj, out_buf);
    return(out_buf);
}
 
/**********************************************************************/
/*** Function:  viet_vni_2_any()                                    ***/
/*** Purpose:   Converts a VNI buffer to the destination charset    ***/
/**********************************************************************/
MODULE_PRIVATE unsigned char *viet_vni_2_any (
                                          CCCDataObject obj,
                                          const unsigned char *vni_buf,
                                          int32 vni_bufsz)
{
    unsigned char *out_buf;
 
    out_buf = xlat_vni_2_viscii(obj,vni_buf,vni_bufsz);
    if (INTL_GetCCCToCSID(obj) != CS_VIET_VISCII)
        out_buf = xlat_vis_ucs2_X(obj, out_buf);
    return(out_buf);
}
 
/**********************************************************************/
/*** Function:  viet_any_2_viqr()                                   ***/
/*** Purpose:   Converts a from_charset buffer to a VIQR buffer     ***/
/**********************************************************************/
MODULE_PRIVATE unsigned char *viet_any_2_viqr (
                                          CCCDataObject obj,
                                          const unsigned char *in_buf,
                                          int32 in_bufsz)
{
    unsigned char *out_buf, *viqr_buf;
    uint32 use_tmp_mem, bufsz=0, fcsid;
 
    fcsid = INTL_GetCCCFromCSID(obj);
    use_tmp_mem = 1; /*** Assume we will use a temporary VIS buf ***/
    switch (fcsid) {
        case CS_VIET_VISCII:
                  /*** No intermediate conversion is needed ***/
                  out_buf = (unsigned char *) in_buf;
                  bufsz = in_bufsz;
                  use_tmp_mem = 0;
                  break;
        case CS_VIET_VNI:
                  /*** This is multi-bytes charset, cannot convert ***/
                  /*** via UCS2, so we use our internal conversion ***/
                  out_buf = xlat_vni_2_viscii(obj,in_buf,in_bufsz);
                  break;
        default:  /*** A via-UCS2 conversion is needed        ***/
                  out_buf = xlat_X_ucs2_vis(obj, in_buf, in_bufsz);
    }
    if (out_buf) {
        if (use_tmp_mem) bufsz = INTL_GetCCCLen(obj);
        viqr_buf = xlat_viscii_2_viqr(obj, out_buf, bufsz);
        if (use_tmp_mem) {     /*** VIQR allocates memory itself so  ***/
            XP_FREE(out_buf);  /*** we need to free the temp VIS buf ***/
        }
        out_buf = viqr_buf;
    }
    return(out_buf);
}
 
/**********************************************************************/
/*** Function:  viet_any_2_vni()                                    ***/
/*** Purpose:   Converts a from_charset buffer to a VNI buffer      ***/
/**********************************************************************/
MODULE_PRIVATE unsigned char *viet_any_2_vni (
                                          CCCDataObject obj,
                                          const unsigned char *in_buf,
                                          int32 in_bufsz)
{
    unsigned char *out_buf, *vni_buf;
    uint32 use_tmp_mem, bufsz=0, fcsid;
 
    fcsid = INTL_GetCCCFromCSID(obj);
    use_tmp_mem = 1; /*** Assume we will use a temporary VIS buf ***/
    switch (fcsid) {
        case CS_VIET_VISCII:
                  /*** No intermediate conversion is needed ***/
                  out_buf = (unsigned char *) in_buf;
                  bufsz = in_bufsz;
                  use_tmp_mem = 0;
                  break;
        case CS_VIET_VIQR:
                  /*** This is multi-bytes charset, cannot convert ***/
                  /*** via UCS2, so we use our internal conversion ***/
                  out_buf = xlat_viqr_2_viscii(obj,in_buf,in_bufsz);
                  break;
        default:  /*** A via-UCS2 conversion is needed        ***/
                  out_buf = xlat_X_ucs2_vis(obj, in_buf, in_bufsz);
    }
    if (out_buf) {
        if (use_tmp_mem) bufsz = INTL_GetCCCLen(obj);
        vni_buf = xlat_viscii_2_vni(obj, out_buf, bufsz);
        if (use_tmp_mem) {     /*** VNI allocates memory itself so   ***/
            XP_FREE(out_buf);  /*** we need to free the temp VIS buf ***/
        }
        out_buf = vni_buf;
    }
    return(out_buf);
}
