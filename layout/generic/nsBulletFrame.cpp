/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */
#include "nsCOMPtr.h"
#include "nsBulletFrame.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLIIDs.h"
#include "nsHTMLParts.h"
#include "nsHTMLValue.h"
#include "nsHTMLContainerFrame.h"
#include "nsIFontMetrics.h"
#include "nsIHTMLContent.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsIDocument.h"
#include "nsIReflowCommand.h"
#include "nsIRenderingContext.h"
#include "nsIURL.h"
#include "prprf.h"

nsBulletFrame::nsBulletFrame()
{
}

nsBulletFrame::~nsBulletFrame()
{
}

NS_IMETHODIMP
nsBulletFrame::DeleteFrame(nsIPresContext& aPresContext)
{
  // Stop image loading first
  mImageLoader.StopAllLoadImages(&aPresContext);

  // Let base class do the rest
  return nsFrame::DeleteFrame(aPresContext);
}

NS_IMETHODIMP
nsBulletFrame::Init(nsIPresContext&  aPresContext,
                    nsIContent*      aContent,
                    nsIFrame*        aParent,
                    nsIStyleContext* aContext,
                    nsIFrame*        aPrevInFlow)
{
  nsresult  rv = nsFrame::Init(aPresContext, aContent, aParent,
                               aContext, aPrevInFlow);

  nsIURL* baseURL = nsnull;
  nsIHTMLContent* htmlContent;
  rv = mContent->QueryInterface(kIHTMLContentIID, (void**)&htmlContent);
  if (NS_SUCCEEDED(rv)) {
    htmlContent->GetBaseURL(baseURL);
    NS_RELEASE(htmlContent);
  }
  else {
    nsIDocument* doc;
    rv = mContent->GetDocument(doc);
    if (NS_SUCCEEDED(rv) && doc) {
      doc->GetBaseURL(baseURL);
      NS_RELEASE(doc);
    }
  }
  const nsStyleList* myList = (const nsStyleList*)
    mStyleContext->GetStyleData(eStyleStruct_List);
  mImageLoader.Init(this, UpdateBulletCB, this,
                    baseURL, myList->mListStyleImage);
  NS_IF_RELEASE(baseURL);

  return NS_OK;
}

NS_IMETHODIMP
nsBulletFrame::GetFrameName(nsString& aResult) const
{
  return MakeFrameName("Bullet", aResult);
}

NS_METHOD
nsBulletFrame::Paint(nsIPresContext&      aCX,
                     nsIRenderingContext& aRenderingContext,
                     const nsRect&        aDirtyRect,
                     nsFramePaintLayer    aWhichLayer)
{
  if (NS_FRAME_PAINT_LAYER_FOREGROUND != aWhichLayer) {
    return NS_OK;
  }

  const nsStyleDisplay* disp =
    (const nsStyleDisplay*)mStyleContext->GetStyleData(eStyleStruct_Display);

  if (disp->mVisible) {
    const nsStyleList* myList =
      (const nsStyleList*)mStyleContext->GetStyleData(eStyleStruct_List);
    PRUint8 listStyleType = myList->mListStyleType;

    if (myList->mListStyleImage.Length() > 0) {
      nsIImage* image = mImageLoader.GetImage();
      if (nsnull == image) {
        if (!mImageLoader.GetLoadImageFailed()) {
          // No image yet
          return NS_OK;
        }
      }
      else {
        if (!mImageLoader.GetLoadImageFailed()) {
          nsRect innerArea(mPadding.left, mPadding.top,
                           mRect.width - (mPadding.left + mPadding.right),
                           mRect.height - (mPadding.top + mPadding.bottom));
          aRenderingContext.DrawImage(image, innerArea);
          return NS_OK;
        }
        listStyleType = NS_STYLE_LIST_STYLE_DISC;
      }
    }

    const nsStyleFont* myFont =
      (const nsStyleFont*)mStyleContext->GetStyleData(eStyleStruct_Font);
    const nsStyleColor* myColor =
      (const nsStyleColor*)mStyleContext->GetStyleData(eStyleStruct_Color);

    nsCOMPtr<nsIFontMetrics> fm;
    aRenderingContext.SetColor(myColor->mColor);

    nsAutoString text;
    switch (listStyleType) {
    case NS_STYLE_LIST_STYLE_NONE:
      break;

    default:
    case NS_STYLE_LIST_STYLE_BASIC:
    case NS_STYLE_LIST_STYLE_DISC:
      aRenderingContext.FillEllipse(mPadding.left, mPadding.top,
                                    mRect.width - (mPadding.left + mPadding.right),
                                    mRect.height - (mPadding.top + mPadding.bottom));
      break;

    case NS_STYLE_LIST_STYLE_CIRCLE:
      aRenderingContext.DrawEllipse(mPadding.left, mPadding.top,
                                    mRect.width - (mPadding.left + mPadding.right),
                                    mRect.height - (mPadding.top + mPadding.bottom));
      break;

    case NS_STYLE_LIST_STYLE_SQUARE:
      aRenderingContext.FillRect(mPadding.left, mPadding.top,
                                 mRect.width - (mPadding.left + mPadding.right),
                                 mRect.height - (mPadding.top + mPadding.bottom));
      break;

    case NS_STYLE_LIST_STYLE_DECIMAL:
    case NS_STYLE_LIST_STYLE_DECIMAL_LEADING_ZERO:
    case NS_STYLE_LIST_STYLE_LOWER_ROMAN:
    case NS_STYLE_LIST_STYLE_UPPER_ROMAN:
    case NS_STYLE_LIST_STYLE_LOWER_ALPHA:
    case NS_STYLE_LIST_STYLE_UPPER_ALPHA:
    case NS_STYLE_LIST_STYLE_LOWER_GREEK:
    case NS_STYLE_LIST_STYLE_HEBREW:
    case NS_STYLE_LIST_STYLE_ARMENIAN:
    case NS_STYLE_LIST_STYLE_GEORGIAN:
    case NS_STYLE_LIST_STYLE_CJK_IDEOGRAPHIC:
    case NS_STYLE_LIST_STYLE_HIRAGANA:
    case NS_STYLE_LIST_STYLE_KATAKANA:
    case NS_STYLE_LIST_STYLE_HIRAGANA_IROHA:
    case NS_STYLE_LIST_STYLE_KATAKANA_IROHA:
      aCX.GetMetricsFor(myFont->mFont, getter_AddRefs(fm));
      GetListItemText(aCX, *myList, text);
      aRenderingContext.SetFont(fm);
      aRenderingContext.DrawString(text, mPadding.left, mPadding.top);
      break;
    }
  }
  return NS_OK;
}

PRInt32
nsBulletFrame::SetListItemOrdinal(PRInt32 aNextOrdinal)
{
  // Assume that the ordinal comes from the block reflow state
  mOrdinal = aNextOrdinal;

  // Try to get value directly from the list-item, if it specifies a
  // value attribute. Note: we do this with our parent's content
  // because our parent is the list-item.
  nsHTMLValue value;
  nsIContent* parentContent;
  mParent->GetContent(&parentContent);
  nsIHTMLContent* hc;
  if (NS_OK == parentContent->QueryInterface(kIHTMLContentIID, (void**) &hc)) {
    if (NS_CONTENT_ATTR_HAS_VALUE ==
        hc->GetHTMLAttribute(nsHTMLAtoms::value, value)) {
      if (eHTMLUnit_Integer == value.GetUnit()) {
        // Use ordinal specified by the value attribute
        mOrdinal = value.GetIntValue();
        if (mOrdinal <= 0) {
          mOrdinal = 1;
        }
      }
    }
    NS_RELEASE(hc);
  }
  NS_RELEASE(parentContent);

  return mOrdinal + 1;
}


// XXX change roman/alpha to use unsigned math so that maxint and
// maxnegint will work


static void DecimalToText(PRInt32 ordinal, nsString& result)
{
   char cbuf[40];
   PR_snprintf(cbuf, sizeof(cbuf), "%ld", ordinal);
   result.Append(cbuf);
}
static void DecimalLeadingZeroToText(PRInt32 ordinal, nsString& result)
{
   char cbuf[40];
   PR_snprintf(cbuf, sizeof(cbuf), "%02ld", ordinal);
   result.Append(cbuf);
}


static const char* gLowerRomanCharsA = "ixcm";
static const char* gUpperRomanCharsA = "IXCM";
static const char* gLowerRomanCharsB = "vld?";
static const char* gUpperRomanCharsB = "VLD?";

static void RomanToText(PRInt32 ordinal, nsString& result, const char* achars, const char* bchars)
{
  if (ordinal <= 0) {
    ordinal = 1;
  }
  nsAutoString addOn, decStr;
  decStr.Append(ordinal, 10);
  PRIntn len = decStr.Length();
  const PRUnichar* dp = decStr.GetUnicode();
  const PRUnichar* end = dp + len;
  PRIntn romanPos = len;
  PRIntn n;

  for (; dp < end; dp++) {
    romanPos--;
    addOn.SetLength(0);
    switch(*dp) {
      case '3':  addOn.Append(achars[romanPos]);
      case '2':  addOn.Append(achars[romanPos]);
      case '1':  addOn.Append(achars[romanPos]);
        break;
      case '4':
        addOn.Append(achars[romanPos]);
        // FALLTHROUGH
      case '5': case '6':
      case '7': case  '8':
        addOn.Append(bchars[romanPos]);
        for(n=0;n<(*dp-'5');n++) {
          addOn.Append(achars[romanPos]);
        }
        break;
      case '9':
        addOn.Append(achars[romanPos]);
        addOn.Append(achars[romanPos+1]);
        break;
      default:
        break;
    }
    result.Append(addOn);
  }
}

#define ALPHA_SIZE 26
static PRUnichar gLowerAlphaChars[ALPHA_SIZE]  = 
{
0x0061, 0x0062, 0x0063, 0x0064, 0x0065, // A   B   C   D   E
0x0066, 0x0067, 0x0068, 0x0069, 0x006A, // F   G   H   I   J
0x006B, 0x006C, 0x006D, 0x006E, 0x006F, // K   L   M   N   O
0x0070, 0x0071, 0x0072, 0x0073, 0x0074, // P   Q   R   S   T
0x0075, 0x0076, 0x0077, 0x0078, 0x0079, // U   V   W   X   Y
0x007A                                  // Z
};

static PRUnichar gUpperAlphaChars[ALPHA_SIZE]  = 
{
0x0041, 0x0042, 0x0043, 0x0044, 0x0045, // A   B   C   D   E
0x0046, 0x0047, 0x0048, 0x0049, 0x004A, // F   G   H   I   J
0x004B, 0x004C, 0x004D, 0x004E, 0x004F, // K   L   M   N   O
0x0050, 0x0051, 0x0052, 0x0053, 0x0054, // P   Q   R   S   T
0x0055, 0x0056, 0x0057, 0x0058, 0x0059, // U   V   W   X   Y
0x005A                                  // Z
};


#define KATAKANA_CHARS_SIZE 48
// Page 94 Writing Systems of The World
// after modification by momoi
static PRUnichar gKatakanaChars[KATAKANA_CHARS_SIZE] =
{
0x30A2, 0x30A4, 0x30A6, 0x30A8, 0x30AA, //  a    i   u    e    o
0x30AB, 0x30AD, 0x30AF, 0x30B1, 0x30B3, // ka   ki  ku   ke   ko
0x30B5, 0x30B7, 0x30B9, 0x30BB, 0x30BD, // sa  shi  su   se   so
0x30BF, 0x30C1, 0x30C4, 0x30C6, 0x30C8, // ta  chi tsu   te   to
0x30CA, 0x30CB, 0x30CC, 0x30CD, 0x30CE, // na   ni  nu   ne   no
0x30CF, 0x30D2, 0x30D5, 0x30D8, 0x30DB, // ha   hi  hu   he   ho
0x30DE, 0x30DF, 0x30E0, 0x30E1, 0x30E2, // ma   mi  mu   me   mo
0x30E4,         0x30E6,         0x30E8, // ya       yu        yo 
0x30E9, 0x30EA, 0x30EB, 0x30EC, 0x30ED, // ra   ri  ru   re   ro
0x30EF, 0x30F0,         0x30F1, 0x30F2, // wa (w)i     (w)e (w)o
0x30F3                                  //  n
};

#define HIRAGANA_CHARS_SIZE 48 
static PRUnichar gHiraganaChars[HIRAGANA_CHARS_SIZE] =
{
0x3042, 0x3044, 0x3046, 0x3048, 0x304A, //  a    i    u    e    o
0x304B, 0x304D, 0x304F, 0x3051, 0x3053, // ka   ki   ku   ke   ko
0x3055, 0x3057, 0x3059, 0x305B, 0x305D, // sa  shi   su   se   so
0x305F, 0x3061, 0x3064, 0x3066, 0x3068, // ta  chi  tsu   te   to
0x306A, 0x306B, 0x306C, 0x306D, 0x306E, // na   ni   nu   ne   no
0x306F, 0x3072, 0x3075, 0x3078, 0x307B, // ha   hi   hu   he   ho
0x307E, 0x307F, 0x3080, 0x3081, 0x3082, // ma   mi   mu   me   mo
0x3084,         0x3086,         0x3088, // ya        yu       yo 
0x3089, 0x308A, 0x308B, 0x308C, 0x308D, // ra   ri   ru   re   ro
0x308F, 0x3090,         0x3091, 0x3092, // wa (w)i      (w)e (w)o
0x3093                                  // n
};


#define HIRAGANA_IROHA_CHARS_SIZE 47
// Page 94 Writing Systems of The World
static PRUnichar gHiraganaIrohaChars[HIRAGANA_IROHA_CHARS_SIZE] =
{
0x3044, 0x308D, 0x306F, 0x306B, 0x307B, //  i   ro   ha   ni   ho
0x3078, 0x3068, 0x3061, 0x308A, 0x306C, // he   to  chi   ri   nu
0x308B, 0x3092, 0x308F, 0x304B, 0x3088, // ru (w)o   wa   ka   yo
0x305F, 0x308C, 0x305D, 0x3064, 0x306D, // ta   re   so  tsu   ne
0x306A, 0x3089, 0x3080, 0x3046, 0x3090, // na   ra   mu    u (w)i
0x306E, 0x304A, 0x304F, 0x3084, 0x307E, // no    o   ku   ya   ma
0x3051, 0x3075, 0x3053, 0x3048, 0x3066, // ke   hu   ko    e   te
0x3042, 0x3055, 0x304D, 0x3086, 0x3081, //  a   sa   ki   yu   me
0x307F, 0x3057, 0x3091, 0x3072, 0x3082, // mi  shi (w)e   hi   mo 
0x305B, 0x3059                          // se   su
};

#define KATAKANA_IROHA_CHARS_SIZE 47
static PRUnichar gKatakanaIrohaChars[KATAKANA_IROHA_CHARS_SIZE] =
{
0x30A4, 0x30ED, 0x30CF, 0x30CB, 0x30DB, //  i   ro   ha   ni   ho
0x30D8, 0x30C8, 0x30C1, 0x30EA, 0x30CC, // he   to  chi   ri   nu
0x30EB, 0x30F2, 0x30EF, 0x30AB, 0x30E8, // ru (w)o   wa   ka   yo
0x30BF, 0x30EC, 0x30BD, 0x30C4, 0x30CD, // ta   re   so  tsu   ne
0x30CA, 0x30E9, 0x30E0, 0x30A6, 0x30F0, // na   ra   mu    u (w)i
0x30CE, 0x30AA, 0x30AF, 0x30E4, 0x30DE, // no    o   ku   ya   ma
0x30B1, 0x30D5, 0x30B3, 0x30A8, 0x30C6, // ke   hu   ko    e   te
0x30A2, 0x30B5, 0x30AD, 0x30E6, 0x30E1, //  a   sa   ki   yu   me
0x30DF, 0x30B7, 0x30F1, 0x30D2, 0x30E2, // mi  shi (w)e   hi   mo 
0x30BB, 0x30B9                          // se   su
};

#define LOWER_GREEK_CHARS_SIZE 24
// Note: 0x03C2 GREEK FINAL SIGMA is not used in here....
static PRUnichar gLowerGreekChars[LOWER_GREEK_CHARS_SIZE] =
{
0x03B1, 0x03B2, 0x03B3, 0x03B4, 0x03B5, // alpha  beta  gamma  delta  epsilon
0x03B6, 0x03B7, 0x03B8, 0x03B9, 0x03BA, // zeta   eta   theta  iota   kappa   
0x03BB, 0x03BC, 0x03BD, 0x03BE, 0x03BF, // lamda  mu    nu     xi     omicron 
0x03C0, 0x03C1, 0x03C3, 0x03C4, 0x03C5, // pi     rho   sigma  tau    upsilon 
0x03C6, 0x03C7, 0x03C8, 0x03C9          // phi    chi   psi    omega    
};

// We know cjk-ideographic need 31 characters to display 99,999,999,999,999,999
// georgian and armenian need 6 at most
// hebrew may need more...

#define NUM_BUF_SIZE 34 

static void CharListToText(PRInt32 ordinal, nsString& result, const PRUnichar* chars, PRInt32 aBase)
{
  PRUnichar buf[NUM_BUF_SIZE];
  PRInt32 idx = NUM_BUF_SIZE;
  if (ordinal <= 0) {
    ordinal = 1;
  }
  do {
    ordinal--; // a == 0
    PRInt32 cur = ordinal % aBase;
    buf[--idx] = chars[cur];
    ordinal /= aBase ;
  } while ( ordinal > 0);
  result.Append(buf+idx,NUM_BUF_SIZE-idx);
}


static PRUnichar gCJKIdeographicDigit[10] =
{
  0x96f6, 0x4e00, 0x4e8c, 0x4e09, 0x56db,  // 0 - 4
  0x4e94, 0x516d, 0x4e03, 0x516b, 0x4e5d   // 5 - 9
};
static PRUnichar gCJKIdeographicUnit[4] =
{
  0x000, 0x5341, 0x767e, 0x5343
};
static PRUnichar gCJKIdeographic10KUnit[4] =
{
  0x000, 0x842c, 0x5104, 0x5146
};

static void CJKIdeographicToText(PRInt32 ordinal, nsString& result, 
                                 const PRUnichar* digits,
                                 const PRUnichar *unit, 
                                 const PRUnichar* unit10k)
{
// In theory, we need the following if condiction,
// However, the limit, 10 ^ 16, is greater than the max of PRUint32
// so we don't really need to test it here.
// if( ordinal > 9999999999999999)
// {
//    PR_snprintf(cbuf, sizeof(cbuf), "%ld", ordinal);
//    result.Append(cbuf);
// } 
// else 
// {
  PRUnichar c10kUnit = 0;
  PRUnichar cUnit = 0;
  PRUnichar cDigit = 0;
  PRUint32 ud = 0;
  PRUnichar buf[NUM_BUF_SIZE];
  PRInt32 idx = NUM_BUF_SIZE;
  PRBool bOutputZero = ( 0 == ordinal );
  do {
    if(0 == (ud % 4)) {
      c10kUnit = unit10k[ud/4];
    }
    PRInt32 cur = ordinal % 10;
    cDigit = digits[cur];
    if( 0 == cur)
    {
      cUnit = 0;
      if(bOutputZero) {
        bOutputZero = PR_FALSE;
        if(0 != cDigit)
          buf[--idx] = cDigit;
      }
    }
    else
    {
      bOutputZero = PR_TRUE;
      cUnit = unit[ud%4];

      if(0 != c10kUnit)
        buf[--idx] = c10kUnit;
      if(0 != cUnit)
        buf[--idx] = cUnit;
      if((0 != cDigit) && 
         ( (1 != cur) || (1 != (ud%4)) || ( ordinal > 10)) )
        buf[--idx] = cDigit;

      c10kUnit =  0;
    }
    ordinal /= 10;
    ud++;

  } while( ordinal > 0);
  result.Append(buf+idx,NUM_BUF_SIZE-idx);
// }

}

#define HEBREW_THROSAND_SEP 0x0020
#define HEBREW_GERESH       0x05F3
#define HEBREW_GERSHAYIM    0x05F4
static PRUnichar gHebrewDigit[22] = 
{
//   1       2       3       4       5       6       7       8       9
0x05D0, 0x05D1, 0x05D2, 0x05D3, 0x05D4, 0x05D5, 0x05D6, 0x05D7, 0x05D8,
//  10      20      30      40      50      60      70      80      90
0x05D9, 0x05DB, 0x05DC, 0x05DE, 0x05E0, 0x05E1, 0x05E2, 0x05E4, 0x05E6,
// 100     200     300     400
0x05E7, 0x05E8, 0x05E9, 0x05EA
};

static void HebrewToText(PRInt32 ordinal, nsString& result)
{
  PRBool outputSep = PR_FALSE;
  PRUnichar buf[NUM_BUF_SIZE];
  PRInt32 idx = NUM_BUF_SIZE;
  PRUnichar digit;
  do {
    PRInt32 n3 = ordinal % 1000;
    if(outputSep)
      buf[--idx] = HEBREW_THROSAND_SEP; // output thousand seperator
    outputSep = ( n3 > 0); // request to output thousand seperator next time.

    PRInt32 d = 0; // we need to keep track of digit got output per 3 digits,
    // so we can handle Gershayim and Gersh correctly

    // Process digit for 100 - 900
    for(PRInt32 n1 = 400; n1 > 0; )
    {
      if( n3 >= n1)
      {
        n3 -= n1;

        digit = gHebrewDigit[(n1/100)-1+18];
        if( n3 > 0)
        {
          buf[--idx] = digit;
          d++;
        } else { 
          // if this is the last digit
          if (d > 0)
          {
            buf[--idx] = HEBREW_GERSHAYIM; 
            buf[--idx] = digit;
          } else {
            buf[--idx] = digit;    
            buf[--idx] = HEBREW_GERESH;
          } // if
        } // if
      } else {
        n1 -= 100;
      } // if
    } // for

    // Process digit for 10 - 90
    PRInt32 n2;
    if( n3 >= 10 )
    {
      // Special process for 15 and 16
      if(( 15 == n3 ) || (16 == n3)) {
        // Special rule for religious reason...
        // 15 is represented by 9 and 6, not 10 and 5
        // 16 is represented by 9 and 7, not 10 and 6
        n2 = 9;
        digit = gHebrewDigit[ n2 - 1];    
      } else {
        n2 = n3 - (n3 % 10);
        digit = gHebrewDigit[(n2/10)-1+9];
      } // if

      n3 -= n2;

      if( n3  > 0) {
        buf[--idx] = digit;
        d++;
      } else {
        // if this is the last digit
        if (d > 0)
        {
          buf[--idx] = HEBREW_GERSHAYIM;  
          buf[--idx] = digit;
        } else {
          buf[--idx] = digit;    
          buf[--idx] = HEBREW_GERESH;
        } // if
      } // if
    } // if
  
    // Process digit for 1 - 9 
    if ( n3 > 0)
    {
      digit = gHebrewDigit[n3-1];
      // must be the last digit
      if (d > 0)
      {
        buf[--idx] = HEBREW_GERSHAYIM; 
        buf[--idx] = digit;
      } else {
        buf[--idx] = digit;    
        buf[--idx] = HEBREW_GERESH;
      } // if
    } // if
    ordinal /= 1000;
  } while (ordinal >= 1);
  result.Append(buf+idx,NUM_BUF_SIZE-idx);
}


static void ArmenianToText(PRInt32 ordinal, nsString& result)
{
  if((0 == ordinal) || (ordinal > 9999)) { // zero or reach the limit of Armenain numbering system
    DecimalToText(ordinal, result);
    return;
  } else {
    PRUnichar buf[NUM_BUF_SIZE];
    PRInt32 idx = NUM_BUF_SIZE;
    PRInt32 d = 0;
    do {
      PRInt32 cur = ordinal % 10;
      if( cur > 0)
      {
        PRUnichar u = 0x0530 + (d * 9) + cur;
        buf[--idx] = u;
      }
      d++;
      ordinal /= 10;
    } while ( ordinal > 0);
    result.Append(buf+idx,NUM_BUF_SIZE-idx);
  }
}


static PRUnichar gGeorgianValue [ 37 ] = { // 4 * 9 + 1 = 37
//      1       2       3       4       5       6       7       8       9
   0x10A0, 0x10A1, 0x10A2, 0x10A3, 0x10A4, 0x10A5, 0x10A6, 0x10C1, 0x10A7,
//     10      20      30      40      50      60      70      80      90
   0x10A8, 0x10A9, 0x10AA, 0x10AB, 0x10AC, 0x10C2, 0x10AD, 0x10AE, 0x10AF,
//    100     200     300     400     500     600     700     800     900
   0x10B0, 0x10B1, 0x10B2, 0x10B3, 0x10C3, 0x10B4, 0x10B5, 0x10B6, 0x10B7,
//   1000    2000    3000    4000    5000    6000    7000    8000    9000
   0x10B8, 0x10B9, 0x10BA, 0x10BB, 0x10BC, 0x10BD, 0x10BE, 0x10C4, 0x10C5,
//  10000
   0x10BF
};
static void GeorgianToText(PRInt32 ordinal, nsString& result)
{
  if((0 == ordinal) || (ordinal > 19999)) { // zero or reach the limit of Armenain numbering system
    DecimalToText(ordinal, result);
    return;
  } else {
    PRUnichar buf[NUM_BUF_SIZE];
    PRInt32 idx = NUM_BUF_SIZE;
    PRInt32 d = 0;
    do {
      PRInt32 cur = ordinal % 10;
      if( cur > 0)
      {
        PRUnichar u = gGeorgianValue[(d * 9 ) + ( cur - 1)];
        buf[--idx] = u;
      }
      d++;
      ordinal /= 10;
    } while ( ordinal > 0);
    result.Append(buf+idx,NUM_BUF_SIZE-idx);
  }
}

void
nsBulletFrame::GetListItemText(nsIPresContext& aCX,
                               const nsStyleList& aListStyle,
                               nsString& result)
{
  switch (aListStyle.mListStyleType) {
    case NS_STYLE_LIST_STYLE_DECIMAL:
    default: // CSS2 say "A users  agent that does not recognize a numbering system
      // should use 'decimal'
      DecimalToText(mOrdinal, result);
      break;

    case NS_STYLE_LIST_STYLE_DECIMAL_LEADING_ZERO:
      DecimalLeadingZeroToText(mOrdinal, result);
      break;

    case NS_STYLE_LIST_STYLE_LOWER_ROMAN:
      RomanToText(mOrdinal, result, gLowerRomanCharsA, gLowerRomanCharsB);
      break;
    case NS_STYLE_LIST_STYLE_UPPER_ROMAN:
      RomanToText(mOrdinal, result, gUpperRomanCharsA, gUpperRomanCharsB);
      break;

    case NS_STYLE_LIST_STYLE_LOWER_ALPHA:
      CharListToText(mOrdinal, result, gLowerAlphaChars, ALPHA_SIZE);
      break;

    case NS_STYLE_LIST_STYLE_UPPER_ALPHA:
      CharListToText(mOrdinal, result, gUpperAlphaChars, ALPHA_SIZE);
      break;

    case NS_STYLE_LIST_STYLE_KATAKANA:
      CharListToText(mOrdinal, result, gKatakanaChars, KATAKANA_CHARS_SIZE);
      break;

    case NS_STYLE_LIST_STYLE_HIRAGANA:
      CharListToText(mOrdinal, result, gHiraganaChars, HIRAGANA_CHARS_SIZE);
      break;
    
    case NS_STYLE_LIST_STYLE_KATAKANA_IROHA:
      CharListToText(mOrdinal, result, gKatakanaIrohaChars, KATAKANA_IROHA_CHARS_SIZE);
      break;
 
    case NS_STYLE_LIST_STYLE_HIRAGANA_IROHA:
      CharListToText(mOrdinal, result, gHiraganaIrohaChars, HIRAGANA_IROHA_CHARS_SIZE);
      break;

    case NS_STYLE_LIST_STYLE_LOWER_GREEK:
      CharListToText(mOrdinal, result, gLowerGreekChars , LOWER_GREEK_CHARS_SIZE);
      break;

    case NS_STYLE_LIST_STYLE_CJK_IDEOGRAPHIC: 
      // We may need to pass in different table for CJK-Ideographic-complex if it is
      // supported in CSS3 or we may pass in different table for simplified Chinese
      CJKIdeographicToText(mOrdinal, result, gCJKIdeographicDigit, gCJKIdeographicUnit, gCJKIdeographic10KUnit);
      break;

    case NS_STYLE_LIST_STYLE_HEBREW: 
      HebrewToText(mOrdinal, result);
      break;

    case NS_STYLE_LIST_STYLE_ARMENIAN: 
      ArmenianToText(mOrdinal, result);
      break;

    case NS_STYLE_LIST_STYLE_GEORGIAN: 
      GeorgianToText(mOrdinal, result);
      break;
 
  }
  result.Append(".");
}

#define MIN_BULLET_SIZE 5               // from laytext.c

nsresult
nsBulletFrame::UpdateBulletCB(nsIPresContext* aPresContext,
                              nsHTMLImageLoader* aLoader,
                              nsIFrame* aFrame,
                              void* aClosure,
                              PRUint32 aStatus)
{
  nsresult rv = NS_OK;
  if (NS_IMAGE_LOAD_STATUS_SIZE_AVAILABLE & aStatus) {
    // Now that the size is available, trigger a reflow of the bullet
    // frame.
    nsCOMPtr<nsIPresShell> shell;
    rv = aPresContext->GetShell(getter_AddRefs(shell));
    if (NS_SUCCEEDED(rv) && shell) {
      nsIReflowCommand* cmd;
      rv = NS_NewHTMLReflowCommand(&cmd, aFrame,
                                   nsIReflowCommand::ContentChanged);
      if (NS_OK == rv) {
        shell->EnterReflowLock();
        shell->AppendReflowCommand(cmd);
        NS_RELEASE(cmd);
        shell->ExitReflowLock();
      }
    }
  }
  return rv;
}

void
nsBulletFrame::GetDesiredSize(nsIPresContext*  aCX,
                              const nsHTMLReflowState& aReflowState,
                              nsHTMLReflowMetrics& aMetrics)
{
  const nsStyleList* myList =
    (const nsStyleList*)mStyleContext->GetStyleData(eStyleStruct_List);
  nscoord ascent;

  if (myList->mListStyleImage.Length() > 0) {
    mImageLoader.GetDesiredSize(aCX, &aReflowState, aMetrics);
    if (!mImageLoader.GetLoadImageFailed()) {
      nsHTMLContainerFrame::CreateViewForFrame(*aCX, this, mStyleContext,
                                               PR_FALSE);
      aMetrics.ascent = aMetrics.height;
      aMetrics.descent = 0;
      return;
    }
  }

  const nsStyleFont* myFont =
    (const nsStyleFont*)mStyleContext->GetStyleData(eStyleStruct_Font);
  nsCOMPtr<nsIFontMetrics> fm;
  aCX->GetMetricsFor(myFont->mFont, getter_AddRefs(fm));
  nscoord bulletSize;
  float p2t;
  float t2p;

  nsAutoString text;
  switch (myList->mListStyleType) {
  case NS_STYLE_LIST_STYLE_NONE:
    aMetrics.width = 0;
    aMetrics.height = 0;
    aMetrics.ascent = 0;
    aMetrics.descent = 0;
    break;

  case NS_STYLE_LIST_STYLE_DISC:
  case NS_STYLE_LIST_STYLE_CIRCLE:
  case NS_STYLE_LIST_STYLE_BASIC:
  case NS_STYLE_LIST_STYLE_SQUARE:
    aCX->GetTwipsToPixels(&t2p);
    fm->GetMaxAscent(ascent);
    bulletSize = NSTwipsToIntPixels(
      (nscoord)NSToIntRound(0.8f * (float(ascent) / 2.0f)), t2p);
    if (bulletSize < 1) {
      bulletSize = MIN_BULLET_SIZE;
    }
    aCX->GetPixelsToTwips(&p2t);
    bulletSize = NSIntPixelsToTwips(bulletSize, p2t);
    mPadding.bottom = ascent / 8;
    aMetrics.width = mPadding.right + bulletSize;
    aMetrics.height = mPadding.bottom + bulletSize;
    aMetrics.ascent = mPadding.bottom + bulletSize;
    aMetrics.descent = 0;
    break;

 default:
  case NS_STYLE_LIST_STYLE_DECIMAL_LEADING_ZERO:
  case NS_STYLE_LIST_STYLE_DECIMAL:
  case NS_STYLE_LIST_STYLE_LOWER_ROMAN:
  case NS_STYLE_LIST_STYLE_UPPER_ROMAN:
  case NS_STYLE_LIST_STYLE_LOWER_ALPHA:
  case NS_STYLE_LIST_STYLE_UPPER_ALPHA:
  case NS_STYLE_LIST_STYLE_KATAKANA:
  case NS_STYLE_LIST_STYLE_HIRAGANA:
  case NS_STYLE_LIST_STYLE_KATAKANA_IROHA:
  case NS_STYLE_LIST_STYLE_HIRAGANA_IROHA:
  case NS_STYLE_LIST_STYLE_LOWER_GREEK:
  case NS_STYLE_LIST_STYLE_HEBREW: 
  case NS_STYLE_LIST_STYLE_ARMENIAN: 
  case NS_STYLE_LIST_STYLE_GEORGIAN: 
  case NS_STYLE_LIST_STYLE_CJK_IDEOGRAPHIC: 
    GetListItemText(*aCX, *myList, text);
    fm->GetHeight(aMetrics.height);
    aReflowState.rendContext->SetFont(fm);
    aReflowState.rendContext->GetWidth(text, aMetrics.width);
    aMetrics.width += mPadding.right;
    fm->GetMaxAscent(aMetrics.ascent);
    fm->GetMaxDescent(aMetrics.descent);
    break;
  }
}

NS_IMETHODIMP
nsBulletFrame::Reflow(nsIPresContext& aPresContext,
                      nsHTMLReflowMetrics& aMetrics,
                      const nsHTMLReflowState& aReflowState,
                      nsReflowStatus& aStatus)
{
  // Get the base size
  GetDesiredSize(&aPresContext, aReflowState, aMetrics);

  // Add in the border and padding; split the top/bottom between the
  // ascent and descent to make things look nice
  const nsMargin& borderPadding = aReflowState.mComputedBorderPadding;
  aMetrics.width += borderPadding.left + borderPadding.right;
  aMetrics.height += borderPadding.top + borderPadding.bottom;
  aMetrics.ascent += borderPadding.top;
  aMetrics.descent += borderPadding.bottom;

  if (nsnull != aMetrics.maxElementSize) {
    aMetrics.maxElementSize->width = aMetrics.width;
    aMetrics.maxElementSize->height = aMetrics.height;
  }
  aStatus = NS_FRAME_COMPLETE;
  return NS_OK;
}
