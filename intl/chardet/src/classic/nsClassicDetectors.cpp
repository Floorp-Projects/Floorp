/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
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
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */


#include "nsIFactory.h"
#include "nsISupports.h"
#include "nsClassicCharDetDll.h"
#include "pratom.h"

#include "nsICharsetDetector.h"
#include "nsICharsetDetectionObserver.h"
#include "nsIStringCharsetDetector.h"
#include "nsClassicDetectors.h"

//==========================================================

					/* values for EUC shift chars	*/
#define SS2		0x8E		/* Single Shift 2		*/
#define SS3		0x8F		/* Single Shift 3		*/
#define IsRoman(c)			((c) < 0x80)
#define IsSJIS2ndByte(c)	(((c) > 0x3F) && ((c) < 0xFD))
#define IsLoSJIS2ndByte(c)	(((c) > 0x3F) && ((c) < 0xA1))
#define IsHiSJIS2ndByte(c)	(((c) > 0xA0) && ((c) < 0xFD))
#define IsEUCJPKana(b1)		(((b1) > 0xA0) && ((b1) < 0xE0))
#define IsEUCJPKanji(b1or2)	(((b1or2) > 0xA0) && ((b1or2) < 0xFF))

#define	YES		1
#define NO		0
#define	MAYBE	-1

static int
isSJIS(const unsigned char *cp, PRInt32 len)
{
	while (len) {
		if (IsRoman(*cp)) {
			cp++, len--;
		} else if (*cp == 0x80) {		/* illegal SJIS 1st byte			*/
			return NO;
		} else if ((*cp < 0xA0)) {		/* byte 1 of 2byte SJIS 1st range	*/
			if (len > 1) {
				if (IsSJIS2ndByte(cp[1])) {
					if ((*cp != 0x8E && *cp != 0x8F) || (*(cp+1) <= 0xA0))
						return YES;
					cp += 2, len -= 2;	/* valid 2 byte SJIS				*/
				} else {
					return NO;			/* invalid SJIS	2nd byte			*/
				}
			} else
				break;						/* buffer ended w/1of2 byte SJIS */
		} else if (*cp == 0xA0) {			/* illegal EUCJP byte		*/
#if ALLOW_NBSP
			cp++, len--; /* allow nbsp */
#endif
		} else if (*cp < 0xE0) {		/* SJIS half-width kana				*/
			cp++, len--;
		} else if (*cp < 0xF0) {		/* byte 1 of 2byte SJIS	 2nd range	*/
			if (len > 1) {
				if (IsSJIS2ndByte(cp[1])) {
					cp += 2, len -= 2;	/* valid 2 byte SJIS				*/
				} else {
					return NO;			/* invalid SJIS						*/
				}
			} else
				break;					/* buffer ended w/1of2 byte SJIS	*/
		} else {
			return NO;					/* invalid SJIS 1st byte			*/
		}
	}
	return MAYBE;						/* No illegal SJIS values found		*/
}

static int
isEUCJP(const unsigned char *cp, PRInt32 len)
{
	while (len) {
		if (IsRoman(*cp)) {			/* Roman						*/
			cp++, len--;
		} else if (*cp == SS2) {		/* EUCJP JIS201 half-width kana */
			if (len > 1) {
				if (IsEUCJPKana(cp[1]))
					cp += 2, len -= 2;		/* valid half-width kana */
				else
					return NO;				/* invalid 2of3 byte EUC */ 
			} else
				break;						/* buffer ended w/1of2 byte EUC	*/
		} else if (*cp == SS3) {			/* EUCJP JIS212					*/
			 if (len > 1) {
			 	if (IsEUCJPKanji(cp[1])) {
			 		if (len > 2) {
				 		if (IsEUCJPKanji(cp[2]))
							cp += 2, len -= 2;	/* valid 3 byte EUCJP		*/
						else
							return NO;		/* invalid 3of3 byte EUCJP	*/
					} else
						break;				/* buffer ended w/2of3 byte EUCJP */
				} else
					return NO;				/* invalid 2of3 byte EUCJP	*/
			} else
				break;						/* buffer ended w/1of3 byte EUCJP */
		} else if (*cp == 0xA0) {			/* illegal EUCJP byte		*/
#if ALLOW_NBSP
			cp++, len--; /* allow nbsp */
#else
			return NO;
#endif
		} else if (*cp < 0xF0) {		/* EUCJP JIS208 (overlaps SJIS)		*/
			if (len > 1) {
			 	if (IsEUCJPKanji(cp[1]))
					cp += 2, len -= 2;		/* valid 2 byte EUCJP		*/
				else
					return NO;				/* invalid 2of2 byte EUCJP	*/
			} else
				break;						/* buffer ended w/1of2 byte EUCJP */
		} else if (*cp < 0xFF) {		/* EUCJP JIS208 only:			*/
			if (len > 1) {
			 	if (IsEUCJPKanji(cp[1]))
					return YES;			/* valid 2 byte EUCJP, invalid SJIS	*/
				else
					return NO;				/* invalid 2of2 byte EUCJP	*/
			} else
				break;						/* buffer ended w/1of2 byte EUCJP */
		} else {
			return NO;					/* invalid EUCJP 1st byte: 0xFF	*/
		}
	}
	return MAYBE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

static nsresult JA_AutoCharsetDetectBuffer(const char* aBuffer, const PRInt32 aLen, char* aCharset)
{
  PRBool hasEsc = PR_FALSE;
  PRBool asciiOnly = PR_TRUE;

  PL_strcpy(aCharset, "ISO-8859-1");

  // check 8 bit or ESC
  for (int i = 0; i < aLen; i++) {
    if ((unsigned char) aBuffer[i] > 127 || aBuffer[i] == 0x1B) {
      if (aBuffer[i] == 0x1B) {
        hasEsc = PR_TRUE;
        break;
      }
      else {
        asciiOnly = PR_FALSE;
      }
    }
  }

  if (hasEsc) {
    PL_strcpy(aCharset, "ISO-2022-JP");
  }
  else if (!asciiOnly) {
    // use old japanese auto detect code
    int euc, sjis;
    euc = isEUCJP((unsigned char *) aBuffer, aLen);
    sjis = isSJIS((unsigned char *) aBuffer, aLen);
    if (YES == euc) {
      PL_strcpy(aCharset, "EUC-JP");
    }
    else if (YES == sjis) {
      PL_strcpy(aCharset, "Shift_JIS");
    }
    else if (MAYBE == euc && NO == sjis) {
      PL_strcpy(aCharset, "EUC-JP");
    }
    else if (MAYBE == sjis && NO == euc) {
      PL_strcpy(aCharset, "Shift_JIS");
    }
    else if (MAYBE == euc && MAYBE == sjis) {
      PL_strcpy(aCharset, "EUC-JP");
    }
  }

  return NS_OK;
}

//==========================================================
NS_IMPL_ISUPPORTS1(nsClassicDetector, nsICharsetDetector)

//----------------------------------------------------------
nsClassicDetector::nsClassicDetector(const char* language)
{
  NS_INIT_REFCNT();
  mObserver = nsnull;
  PL_strcpy(mLanguage, language);
}
//----------------------------------------------------------
nsClassicDetector::~nsClassicDetector()
{
}
//----------------------------------------------------------
NS_IMETHODIMP nsClassicDetector::Init(
  nsICharsetDetectionObserver* aObserver)
{
  NS_ASSERTION(mObserver == nsnull , "Init twice");
  if(nsnull == aObserver)
     return NS_ERROR_ILLEGAL_VALUE;

  mObserver = aObserver;

  return NS_OK;
}
//----------------------------------------------------------
NS_IMETHODIMP nsClassicDetector::DoIt(
  const char* aBuf, PRUint32 aLen, PRBool* oDontFeedMe)
{
  NS_ASSERTION(mObserver != nsnull , "have not init yet");

  if((nsnull == aBuf) || (nsnull == oDontFeedMe))
     return NS_ERROR_ILLEGAL_VALUE;

  if (!PL_strcasecmp("ja", mLanguage) &&
      NS_SUCCEEDED(JA_AutoCharsetDetectBuffer(aBuf, (PRInt32) aLen, mCharset))) {
    mObserver->Notify(mCharset, eBestAnswer);
  }
  else {
    mObserver->Notify("", eNoAnswerMatch);
  }

  *oDontFeedMe = PR_TRUE;

  return NS_OK;
}
//----------------------------------------------------------
NS_IMETHODIMP nsClassicDetector::Done()
{
  NS_ASSERTION(mObserver != nsnull , "have not init yet");
  return NS_OK;
}

//==========================================================

NS_IMPL_ISUPPORTS1(nsClassicStringDetector, nsIStringCharsetDetector)

//----------------------------------------------------------
nsClassicStringDetector::nsClassicStringDetector(const char* language)
{
  NS_INIT_REFCNT();
  PL_strcpy(mLanguage, language);
}
//----------------------------------------------------------
nsClassicStringDetector::~nsClassicStringDetector()
{
}

//----------------------------------------------------------
NS_IMETHODIMP nsClassicStringDetector::DoIt(const char* aBuf, PRUint32 aLen, 
                                            const char** oCharset, 
                                            nsDetectionConfident &oConfident)
{
  oConfident = eNoAnswerMatch;
  *oCharset = "";

  if (!PL_strcasecmp("ja", mLanguage) &&
      NS_SUCCEEDED(JA_AutoCharsetDetectBuffer(aBuf, (PRInt32) aLen, mCharset))) {
    *oCharset = mCharset;
    oConfident = eBestAnswer;
  }

  return NS_OK;
}

