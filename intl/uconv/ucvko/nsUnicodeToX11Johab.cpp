/*
 * @(#)CharToByteX11Johab.java  1.1 99/08/28
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is CharToByteX11Johab.java.
 *
 * The Initial Developer of the Original Code is
 * Deogtae Kim <dtkim@calab.kaist.ac.kr> (98/05/03) and Based on: Hanterm source code adapted by Jungshik Shin <jshin@pantheon.yale.edu>.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Deogtae Kim <dtkim@calab.kaist.ac.kr> (99/08/28)
 *   Jungshik Shin <jshin@pantheon.yale.edu>
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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
#include "nsUnicodeToX11Johab.h"
#include "nsUCvKODll.h"


typedef char byte;

// XPCOM stuff
NS_IMPL_ADDREF(nsUnicodeToX11Johab)
NS_IMPL_RELEASE(nsUnicodeToX11Johab)
nsresult nsUnicodeToX11Johab::QueryInterface(REFNSIID aIID,
                                          void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }

  *aInstancePtr = NULL;

  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

  if (aIID.Equals(NS_GET_IID(nsIUnicodeEncoder))) {
    *aInstancePtr = (void*) ((nsIUnicodeEncoder*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsICharRepresentable))) {
    *aInstancePtr = (void*) ((nsICharRepresentable*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*) ((nsISupports*)((nsIUnicodeEncoder*)this));
    NS_ADDREF_THIS();
    return NS_OK;
  }

  return NS_NOINTERFACE;
}


NS_IMETHODIMP nsUnicodeToX11Johab::SetOutputErrorBehavior(
      PRInt32 aBehavior,
      nsIUnicharEncoder * aEncoder, PRUnichar aChar)
{
   return NS_ERROR_NOT_IMPLEMENTED;
}

//   1	/*
//   2	 * @(#)CharToByteX11Johab.java  1.0 98/05/03
//   3	 *
//   4	 * Purposes:
//   5	 *   1. Enable displaying all 11,172 Modern hangul syllables with Hanterm
//   6	 *      johab fonts on Unix
//   7	 *   2. Enable displaying some of Unicode 2.0 ancient hangul syllables
//   8	 *      with Hanterm johab fonts on Unix
//   9	 *   3. Enable displaying all of Unicode 2.0 ancient hangul syllables with
//  10	 *      possible future extended Hanterm johab fonts on Unix
//  11	 *
//  12	 * Installation Instructions:
//  13	 * 1. Install Hanterm Johab fonts and a proper font property file to Unix syste
//  14	m.
//  15	 *    (Confer http://calab.kaist.ac.kr/~dtkim/java/ )
//  16	 * 2. Make a directory "jdk1.x.x/classes/"
//  17	 * 3. Compile this class into "jdk1.x.x/classes/"
//  18	 *
//  19	 * Author: Deogtae Kim <dtkim@calab.kaist.ac.kr>, 98/05/03
//  20	 *
//  21	 * Based on: Hanterm source code adapted by Jungshik Shin <jshin@pantheon.yale.
//  22	edu>
//  23	 */
//  24	
//  25	import sun.io.CharToByteConverter;
//  26	import sun.io.MalformedInputException;
//  27	import sun.io.UnknownCharacterException;
//  28	import sun.io.ConversionBufferFullException;
//  29	
//  30	public class CharToByteX11Johab extends CharToByteConverter
//  31	{
//  32	    int state = START;
//  33	
//  34	    public static final int START = 0;
//  35	    public static final int LEADING_CONSONANT = 1;
//  36	    public static final int VOWEL = 2;
//  37	
//  38	    int l = 0x5f; // leading consonant
//  39	    int v = 0;    // vowel
//  40	    int t = 0;    // trailing consonant

#define START 1
#define LEADING_CONSONANT 1
#define VOWEL 2

// constructor and destroctor

nsUnicodeToX11Johab::nsUnicodeToX11Johab()
{
   Reset();
   state = START;
   l = 0x5f;
   v = 0;
   t = 0;

}
nsUnicodeToX11Johab::~nsUnicodeToX11Johab()
{
}

//  41	
//  42	    /*
//  43	     * This method indicates the charset name for this font.
//  44	     */
//  45	    public String getCharacterEncoding()
//  46	    {
//  47	        return "X11Johab";
//  48	    }
//  49	
//  50	    /*
//  51	     * This method indicates the range this font covers.
//  52	     */
//  53	    public boolean canConvert(char ch)
//  54	    {
//  55	        if ( 0xac00 <= ch && ch <= 0xd7a3      // Modern hangul syllables
//  56	             || 0x1100 <= ch && ch <= 0x1112   // modern leading consonants (19
//  57	)
//  58	             || 0x1113 <= ch && ch <= 0x1159   // ancient leading consonants (7
//  59	1)
//  60	                && lconBase[ch-0x1100] != 0
//  61	             || ch == 0x115f                   // leading consonants filler
//  62	             || 0x1160 <= ch && ch <= 0x1175   // modern vowels (21)
//  63	             || 0x1176 <= ch && ch <= 0x11a2   // ancient vowels (45)
//  64	                && vowBase[ch-0x1160] != 0
//  65	             || 0x11a8 <= ch && ch <= 0x11c2   // modern trailing consonants (2
//  66	7)
//  67	             || 0x11c3 <= ch && ch <= 0x11f9   // ancient trailing consonants (
//  68	55)
//  69	                && tconBase[ch-0x11a7] != 0 )
//  70	            return true;
//  71	        return false;
//  72	    }

#define canConvert(ch) \
 (((0xac00 <=(ch))&&((ch)<= 0xd7a3))    /* Modern hangul syllables         */\
   || ((0x1100 <=(ch))&&((ch)<= 0x1112)) /* modern leading consonants (19)  */\
   || ((0x1113 <=(ch))&&((ch)<= 0x1159) /* ancient leading consonants (71) */\
       && (lconBase[ch-0x1100] != 0))                                        */\
   || ((ch) == 0x115f)                 /* leading consonants filler       */\
   || ((0x1160 <=(ch))&&((ch)<= 0x1175))  /* modern vowels (21)              */\
   || ((0x1176 <=(ch))&&((ch)<= 0x11a2) /* ancient vowels (45)             */\
       && (vowBase[(ch)-0x1160] != 0  ))                                        */\
   || ((0x11a8 <=(ch))&&((ch)<= 0x11c2))/* modern trailing consonants (27) */\
   || ((0x11c3 <=(ch))&&((ch)<= 0x11f9) /* ancient trailing consonants (55)*/\
       && (tconBase[(ch)-0x11a7] != 0 )))

//  73	
//  74	    /*
//  75	     * This method converts the unicode to this font index.
//  76	     * Note: ConversionBufferFullException is not handled
//  77	     *       since this class is only used for character display.
//  78	     */
//  79	    public int convert(char[] input, int inStart, int inEnd,
//  80	                       byte[] output, int outStart, int outEnd)
//  81	        throws MalformedInputException, UnknownCharacterException
//  82	    {
NS_IMETHODIMP nsUnicodeToX11Johab::Convert(
      const PRUnichar * input, PRInt32 * aSrcLength,
      char * output, PRInt32 * aDestLength)
{
//  83	        charOff = inStart;
//  84	        byteOff = outStart;
                charOff = byteOff = 0;
/*  85	*/
/*  86	*/      for (; charOff < *aSrcLength; charOff++)
/*  87	*/      {
/*  88	*/          PRUnichar ch = input[charOff];
/*  89	*/          if (0xac00 <= ch && ch <= 0xd7a3)
/*  90	*/          {
/*  91	*/              if ( state != START )
/*  92	*/                  composeHangul(output);
/*  93	*/              ch -= 0xac00;
/*  94	*/              l = (ch / 588);        // 588 = 21*28
/*  95	*/              v = ( ch / 28 ) % 21  + 1;
/*  96	*/              t = ch % 28;
/*  97	*/              composeHangul(output);
/*  98	*/          } else if (0x1100 <= ch && ch <= 0x115f)
/*  99	*/          {  // leading consonants (19 + 71 + 1)
/* 100	*/              if ( state != START )
/* 101	*/                  composeHangul(output);
/* 102	*/              l = ch - 0x1100;
/* 103	*/              state = LEADING_CONSONANT;
/* 104	*/          } else if (1160 <= ch && ch <= 0x11a2)
/* 105	*/          {  // vowels (1 + 21 + 45)
/* 106	*/              v = ch - 0x1160;
/* 107	*/              state = VOWEL;
/* 108	*/          } else if (0x11a8 <= ch && ch <= 0x11f9)
/* 109	*/          {  // modern trailing consonants (27)
/* 110	*/              t = ch - 0x11a7;
/* 111	*/              composeHangul(output);
// 112	            } else
// 113	            {
// 114	                throw new UnknownCharacterException();
/* 115	*/           }
/* 116	*/       }
/* 117	*/
/* 118	*/       if ( state != START )
/* 119	*/           composeHangul( output );
/* 120	*/
// 121	         return byteOff - outStart;
// 122	     }
                 *aDestLength = byteOff;
                 return NS_OK;
}
// 123	
// 124	    public int flush(byte output[], int i, int j)
// 125	        throws MalformedInputException
// 126	    {
NS_IMETHODIMP nsUnicodeToX11Johab::Finish(
      char * output, PRInt32 * aDestLength)
{
/* 127	*/      byteOff = 0;
/* 128	*/      PRInt32 len = 0;
/* 129	*/      if ( state != START )
/* 130	*/      {
/* 131	*/          composeHangul( output );
/* 132	*/          len = byteOff;
/* 133	*/      }
/* 134	*/      byteOff = charOff = 0;
// 135	        return len;
                *aDestLength = len;
// 136	    }
// 137	
   return NS_OK;
}

//================================================================
NS_IMETHODIMP nsUnicodeToX11Johab::Reset()
// 138	    public void reset()
/* 139 */  {
/* 140 */      byteOff = charOff = 0;
/* 141 */      state = START;
/* 142 */      l = 0x5f;
/* 143 */      v = t = 0;
               return NS_OK;
/* 144 */  }
//================================================================
// 145	
// 146	    public int getMaxBytesPerChar()
// 147	    {
// 148	        return 6;
// 149	    }

NS_IMETHODIMP nsUnicodeToX11Johab::GetMaxLength(
      const PRUnichar * aSrc, PRInt32 aSrcLength,
      PRInt32 * aDestLength)
{
   *aDestLength = (aSrcLength + 1) *  6;
   return NS_OK;
}
//================================================================

// 150	
// 151	    // The base font index for leading consonants
// 152	
// 153	    static final short[] lconBase = {
static const PRUint16 lconBase[] = {
/* 154 */       // modern leading consonants (19)
/* 155 */       1, 11, 21, 31, 41, 51,
/* 156 */       61, 71, 81, 91, 101, 111,
/* 157 */       121, 131, 141, 151, 161, 171,
/* 158 */       181, 
/* 159 */
/* 160 */       // ancient leading consonants (71 + reserved 5 + filler 1)
/* 161 */       0, 0, 0, 0, 0, 0,       // \u1113 ~ : 
/* 162 */       0, 0, 0, 0, 0, 201,     // \u1119 ~ :
/* 163 */       0, 221, 251, 0, 0, 0,   // \u111f ~ :
/* 164 */       0, 0, 281, 0, 0, 0,     // \u1125 ~ :
/* 165 */       191, 0, 211, 0, 231, 0, // \u112b ~ :
/* 166 */       0, 241, 0, 0, 0, 291,   // \u1131 ~ :
/* 167 */       0, 0, 0, 0, 0, 0,       // \u1137 ~ :
/* 168 */       0, 0, 0, 261, 0, 0,     // \u113d ~ :
/* 169 */       0, 0, 0, 0, 0, 0,       // \u1143 ~ :
/* 170 */       0, 0, 0, 271, 0, 0,     // \u1149 ~ :
/* 171 */       0, 0, 0, 0, 0, 0,       // \u114f ~ :
/* 172 */       0, 0, 0, 0, 301,        // \u1155 ~ :
/* 173 */       0, 0, 0, 0, 0,          // \u115a ~ : reserved
/* 174 */       0,                      // \u115f   : leading consonant filler
/* 175 */  };
//================================================================
// 176	
// 177	    // The base font index for vowels
// 178	
// 179     static final short[] vowBase = {
static const PRUint16 vowBase[] = {
/* 180 */      // modern vowels (filler + 21)
/* 181 */      0,311,314,317,320,323,   // (Fill), A, AE, YA, YAE, EO
/* 182 */      326,329,332,335,339,343, // E, YEO, YE, O, WA, WAE
/* 183 */      347,351,355,358,361,364, // OI, YO, U, WEO, WE, WI
/* 184 */      367,370,374,378,         // YU, EU, UI, I
/* 185 */ 
/* 186 */      // ancient vowels (45)
/* 187 */      0, 0, 0, 0, 0, 0,        // \u1176 ~ : A-O, A-U, YA-O, YA-YO, EO-O, EO- U
/* 189 */      0, 0, 0, 0, 0, 0,        // \u117c ~ : EO-EU, YEO-O, YEO-U, O-EO, O-E, O-YE
/* 191 */      0, 0, 381, 384, 0, 0,    // \u1182 ~ : O-O, O-U, YO-YA, YO-YAE, YO-YEO,  YO-O
/* 193 */      387, 0, 0, 0, 0, 0,      // \u1188 ~ : YO-I, U-A, U-AE, U-EO-EU, U-YE, U-U
/* 195 */      0, 0, 0, 390, 393, 0,    // \u118e ~ : YU-A, YU-EO, YU-E, YU-YEO, YU-YE , YU-U
/* 197 */      396, 0, 0, 0, 0, 0,      // \u1194 ~ : YU-I, EU-U, EU-EU, YI-U, I-A, I- YA
/* 199 */      0, 0, 0, 0, 399, 0,      // \u119a ~ : I-O, I-U, I-EU, I-ARAEA, ARAEA, 	ARAEA-EO
/* 201 */      0, 402, 0                // \u11a0 ~ : ARAEA-U, ARAEA-I,SSANGARAEA 
/* 202 */  };
//================================================================
// 203	
// 204	    // The base font index for trailing consonants
// 205	
// 206	    static final short[] tconBase = {
static const PRUint16 tconBase[] = {
// 207	        // modern trailing consonants (filler + 27)
/* 208	*/      0, 
/* 209	*/      405, 409, 413, 417, 421,
/* 210	*/      425, 429, 433, 437, 441,
/* 211	*/      445, 459, 453, 457, 461,
/* 212	*/      465, 469, 473, 477, 481,
/* 213	*/      485, 489, 493, 497, 501,
/* 214	*/      505, 509,
// 215	        
// 216	        // ancient trailing consonants (55)
/* 217	*/      0, 0, 0, 0, 0, 0,      // \u11c3 ~ :
/* 218	*/      0, 0, 0, 0, 0, 0,      // \u11c9 ~ :
/* 219	*/      0, 0, 0, 0, 0, 0,      // \u11cf ~ :
/* 220	*/      0, 0, 0, 0, 513, 517,  // \u11d5 ~ :
/* 221	*/      0, 0, 0, 0, 0, 0,      // \u11db ~ :
/* 222	*/      0, 0, 0, 0, 0, 0,      // \u11e1 ~ :
/* 223	*/      0, 0, 0, 0, 0, 0,      // \u11e7 ~ :
/* 224	*/      0, 0, 0, 525, 0, 0,    // \u11ed ~ :
/* 225	*/      0, 0, 0, 0, 0, 0,      // \u11f3 ~ :
/* 226	*/      521                    // \u11f9:
/* 227	*/  };
//================================================================
// 228	
// 229	    // The mapping from vowels to leading consonant type
// 230	    // in absence of trailing consonant
// 231	
// 232	    static final short[] lconMap1 = {
static const PRUint8 lconMap1[] = {
/* 233	*/      0,0,0,0,0,0,     // (Fill), A, AE, YA, YAE, EO
/* 234	*/      0,0,0,1,3,3,     // E, YEO, YE, O, WA, WAE
/* 235	*/      3,1,2,4,4,4,     // OI, YO, U, WEO, WE, WI
/* 236	*/      2,1,3,0,         // YU, EU, UI, I
// 237	
// 238	        // ancient vowels (45)
/* 239	*/      3, 4, 3, 3, 3, 4,   // \u1176 ~ : A-O, A-U, YA-O, YA-YO, EO-O, EO-U
/* 240	*/      4, 3, 4, 3, 3, 3,   // \u117c ~ : EO-EU, YEO-O, YEO-U, O-EO, O-E, O-YE
/* 241	*/      1, 1, 3, 3, 3, 1,   // \u1182 ~ : O-O, O-U, YO-YA, YO-YAE, YO-YEO, YO-O
/* 242	*/      3, 4, 4, 4, 4, 2,   // \u1188 ~ : YO-I, U-A, U-AE, U-EO-EU, U-YE, U-U
/* 243	*/      3, 3, 3, 3, 3, 2,   // \u118e ~ : YU-A, YU-EO, YU-E, YU-YEO, YU-YE, YU-U
/* 245	*/      4, 2, 2, 4, 0, 0,   // \u1194 ~ : YU-I, EU-U, EU-EU, YI-U, I-A, I-YA
/* 246	*/      3, 4, 3, 0, 1, 3,   // \u119a ~ : I-O, I-U, I-EU, I-ARAEA, ARAEA, ARAEA-EO
/* 248	*/      2, 3, 1             // \u11a0 ~ : ARAEA-U, ARAEA-I, SSANGARAEA
/* 249	*/  };
//================================================================
// 250	
// 251	    // The mapping from vowels to leading consonant type
// 252	    // in presence of trailing consonant
// 253	
static const PRUint8 lconMap2[] = {
// 254	    static final short[] lconMap2 = {
/* 255	*/      5,5,5,5,5,5,     // (Fill), A, AE, YA, YAE, EO
/* 256	*/      5,5,5,6,8,8,     // E, YEO, YE, O, WA, WAE
/* 257	*/      8,6,7,9,9,9,     // OI, YO, U, WEO, WE, WI
/* 258	*/      7,6,8,5,         // YU, EU, UI, I
// 259	
// 260	        // ancient vowels (45)
/* 261	*/      8, 9, 8, 8, 8, 9,   // \u1176 ~ : A-O, A-U, YA-O, YA-YO, EO-O, EO-U
/* 262	*/      9, 8, 9, 8, 8, 8,   // \u117c ~ : EO-EU, YEO-O, YEO-U, O-EO, O-E, O-YE
/* 263	*/      6, 6, 8, 8, 8, 6,   // \u1182 ~ : O-O, O-U, YO-YA, YO-YAE, YO-YEO, YO-O
/* 264	*/      8, 9, 9, 9, 9, 7,   // \u1188 ~ : YO-I, U-A, U-AE, U-EO-EU, U-YE, U-U
/* 265	*/      8, 8, 8, 8, 8, 7,   // \u118e ~ : YU-A, YU-EO, YU-E, YU-YEO, YU-YE, Y
/* 267	*/      9, 7, 7, 9, 5, 5,   // \u1194 ~ : YU-I, EU-U, EU-EU, YI-U, I-A, I-YA
/* 268	*/      8, 9, 8, 5, 6, 8,   // \u119a ~ : I-O, I-U, I-EU, I-ARAEA, ARAEA, ARAEA-EO
/* 270	*/      7, 8, 6             // \u11a0 ~ : ARAEA-U, ARAEA-I, SSANGARAEA
/* 271	*/  };
//================================================================
// 272	
// 273	    // vowel type ; 1 = o and its alikes, 0 = others            
// 274	    static final short[] vowType = {
static const PRUint8 vowType[] = {
/* 275	*/      0,0,0,0,0,0,
/* 276	*/      0,0,0,1,1,1,
/* 277	*/      1,1,0,0,0,0,
/* 278	*/      0,1,1,0,
// 279	
// 280	        // ancient vowels (45)
/* 281	*/      1, 0, 1, 1, 1, 0,   // \u1176 ~ : A-O, A-U, YA-O, YA-YO, EO-O, EO-U
/* 282	*/      0, 1, 0, 1, 1, 1,   // \u117c ~ : EO-EU, YEO-O, YEO-U, O-EO, O-E, O-YE
/* 283	*/      1, 1, 0, 0, 0, 0,   // \u1182 ~ : O-O, O-U, YO-YA, YO-YAE, YO-YEO, YO-O
/* 284	*/      0, 0, 0, 0, 0, 0,   // \u1188 ~ : YO-I, U-A, U-AE, U-EO-EU, U-YE, U-U
/* 285	*/      0, 0, 0, 0, 0, 0,   // \u118e ~ : YU-A, YU-EO, YU-E, YU-YEO, YU-YE, YU-U
/* 287	*/      0, 0, 0, 0, 0, 0,   // \u1194 ~ : YU-I, EU-U, EU-EU, YI-U, I-A, I-YA
/* 288	*/      0, 0, 0, 0, 0, 0,   // \u119a ~ : I-O, I-U, I-EU, I-ARAEA, ARAEA, ARAEA-EO
/* 290	*/      0, 0, 0             // \u11a0 ~ : ARAEA-U, ARAEA-I, SSANGARAEA
/* 291	*/  };
//================================================================
// 292	
// 293	    // The mapping from trailing consonants to vowel type
// 294	
// 295	    static final int[] tconType = {
static const PRUint8 tconType[] = {
/* 296	*/      0, 1, 1, 1, 2, 1,
/* 297	*/      1, 1, 1, 1, 1, 1,
/* 298	*/      1, 1, 1, 1, 1, 1,
/* 299	*/      1, 1, 1, 1, 1, 1,
/* 300	*/      1, 1, 1, 1,
// 301	
// 302	        // ancient trailing consonants (55)
/* 303	*/      1, 1, 1, 1, 1, 1,  // \u11c3 ~ :
/* 304	*/      1, 1, 1, 1, 1, 1,  // \u11c9 ~ :
/* 305	*/      1, 1, 1, 1, 1, 1,  // \u11cf ~ :
/* 306	*/      1, 1, 1, 1, 1, 1,  // \u11d5 ~ :
/* 307	*/      1, 1, 1, 1, 1, 1,  // \u11db ~ :
/* 308	*/      1, 1, 1, 1, 1, 1,  // \u11e1 ~ :
/* 309	*/      1, 1, 1, 1, 1, 1,  // \u11e7 ~ :
/* 310	*/      1, 1, 1, 1, 1, 1,  // \u11ed ~ :
/* 311	*/      1, 1, 1, 1, 1, 1,  // \u11f3 ~ :
/* 312	*/      1                  // \u11f9:
/* 313	*/  };
//================================================================
// 314	
// 315	    // The mapping from vowels to trailing consonant type
// 316	
// 317	    static final int[] tconMap = {
static const PRUint8 tconMap[] = {
/* 318	*/      0, 0, 2, 0, 2, 1,  // (Fill), A, AE, YA, YAE, EO
/* 319	*/      2, 1, 2, 3, 0, 0,  // E, YEO, YE, O, WA, WAE
/* 320	*/      0, 3, 3, 1, 1, 1,  // OI, YO, U, WEO, WE, WI
/* 321	*/      3, 3, 0, 1,        // YU, EU, UI, I
// 322	
// 323	        // ancient vowels (45)
/* 324	*/      3, 3, 3, 3, 3, 3,   // \u1176 ~ : A-O, A-U, YA-O, YA-YO, EO-O, EO-U
/* 325	*/      3, 3, 3, 1, 0, 0,   // \u117c ~ : EO-EU, YEO-O, YEO-U, O-EO, O-E, O-YE
/* 326	*/      3, 3, 3, 1, 0, 3,   // \u1182 ~ : O-O, O-U, YO-YA, YO-YAE, YO-YEO, YO-O
/* 327	*/      0, 0, 0, 0, 0, 3,   // \u1188 ~ : YO-I, U-A, U-AE, U-EO-EU, U-YE, U-U
/* 328	*/      0, 1, 1, 1, 1, 3,   // \u118e ~ : YU-A, YU-EO, YU-E, YU-YEO, YU-YE, YU- U
/* 330	*/      1, 3, 3, 3, 2, 2,   // \u1194 ~ : YU-I, EU-U, EU-EU, YI-U, I-A, I-YA
/* 331	*/      3, 3, 3, 1, 3, 0,   // \u119a ~ : I-O, I-U, I-EU, I-ARAEA, ARAEA, ARAEA -EO
/* 333	*/      3, 2, 3             // \u11a0 ~ : ARAEA-U, ARAEA-I, SSANGARAEA
/* 334	*/  };
//================================================================
// 335	
// 336	    void composeHangul(byte[] output)
void nsUnicodeToX11Johab::composeHangul(char* output)
/* 337	*/  {
// 338	        int ind;
	        PRUint16 ind;
/* 339  */ 
/* 340	*/      if ( lconBase[l] != 0 )
/* 341	*/      {   // non-filler and supported by Hanterm Johab fonts
/* 342	*/          ind = lconBase[l] + ( t > 0 ? lconMap2[v] : lconMap1[v] );
/* 343	*/          output[byteOff++] = (byte) (ind / 256);
/* 344	*/          output[byteOff++] = (byte) (ind % 256);
/* 345	*/      }
/* 346  */
/* 347	*/      if ( vowBase[v] != 0 )
/* 348	*/      {   // non-filler and supported by Hanterm Johab fonts
/* 349	*/          ind = vowBase[v];
/* 350	*/          if ( vowType[v] == 1)
/* 351	*/          {   //'o' and alikes 
/* 352	*/              // GIYEOK and KIEUK got special treatment
/* 353	*/              ind += ( (l == 0 || l == 15) ? 0 : 1)
/* 354	*/                     + (t > 0 ?  2 : 0 );
/* 355	*/          }
/* 356	*/          else
/* 357	*/          { 
/* 358	*/              ind += tconType[t];
/* 359	*/          }
/* 360  */
/* 361	*/          output[byteOff++] = (byte) (ind / 256);
/* 362	*/          output[byteOff++] = (byte) (ind % 256);
/* 363	*/      }
/* 364	*/
/* 365	*/      if ( tconBase[t] != 0 )  
/* 366	*/      {   // non-filler and supported by Hanterm Johab fonts
/* 367	*/          ind = tconBase[t] + tconMap[v];
/* 368	*/          output[byteOff++] = (byte) (ind / 256);
/* 369	*/          output[byteOff++] = (byte) (ind % 256);
/* 370	*/      } else  if (vowBase[v] == 0) 
/* 371	*/      {   // give one syllable display width since current display width is 0.
/* 372	*/          output[byteOff++] = (byte) 0;
/* 373	*/          output[byteOff++] = (byte) 0;
/* 374	*/      }
/* 375	*/
/* 376	*/      state = START;
/* 377	*/      l = 0x5f;
/* 378	*/      v = t = 0;
/* 379  */  }
// 380   }
// 381	

NS_IMETHODIMP nsUnicodeToX11Johab::FillInfo(PRUint32* aInfo)
{
   // ac00-d7a3
   PRUint32 b = 0xac00 >> 5;
   PRUint32 e = 0xd7a3 >> 5;
   aInfo[ e ] |= (0xFFFFFFFFL >> (31 - ((0xd7a3) & 0x1f)));
   for( ; b < e ; b++)
      aInfo[b] |= 0xFFFFFFFFL;

   PRUnichar i;

   // 1100-1112
   for(i=0x1100;i<=0x1112;i++)
      SET_REPRESENTABLE(aInfo, i);
   // 1113-1159
   for(i=0x1113;i<=0x1159;i++)
      if(lconBase[i-0x1100]!=0)
         SET_REPRESENTABLE(aInfo, i);
   // 115f
   SET_REPRESENTABLE(aInfo, 0x115f);
   // 1160-1175
   for(i=0x1160;i<=0x1175;i++)
      SET_REPRESENTABLE(aInfo, i);
   // 1176-11a2
   for(i=0x1176;i<=0x11a2;i++)
      if(vowBase[i-0x1160]!=0)
         SET_REPRESENTABLE(aInfo, i);
   // 11a8-11c2
   for(i=0x11a8;i<=0x11c2;i++)
      SET_REPRESENTABLE(aInfo, i);
   // 11c3-11f9
   for(i=0x11c3;i<=0x11f9;i++)
      if(tconBase[i-0x11a7]!=0)
         SET_REPRESENTABLE(aInfo, i);
   return NS_OK;
}
