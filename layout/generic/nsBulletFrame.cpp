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
  // Release image loader first so that its refcnt can go to zero
  mImageLoader.DestroyLoader();
  return nsFrame::DeleteFrame(aPresContext);
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

static const char* gLowerRomanCharsA = "ixcm";
static const char* gUpperRomanCharsA = "IXCM";
static const char* gLowerRomanCharsB = "vld?";
static const char* gUpperRomanCharsB = "VLD?";
static const char* gLowerAlphaChars  = "abcdefghijklmnopqrstuvwxyz";
static const char* gUpperAlphaChars  = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";


#define KATAKANA_CHARS_SIZE 77
// Page 94 Writing Systems of The World
static PRUnichar gKatakanaChars[KATAKANA_CHARS_SIZE] =
{
0x30A2, 0x30A4, 0x30A6, 0x30A8, 0x30AA, //  a   i   u   e   o
0x30AB, 0x30AD, 0x30AF, 0x30B1, 0x30B3, // ka  ki  ku  ke  ko
0x30B5, 0x30B7, 0x30B9, 0x30BB, 0x30BD, // sa shi  su  se  so
0x30BF, 0x30C1, 0x30C4, 0x30C6, 0x30C8, // ta chi tsu  te  to
0x30CA, 0x30CB, 0x30CC, 0x30CD, 0x30CE, // na  ni  nu  ne  no
0x30CF, 0x30D2, 0x30D5, 0x30D8, 0x30DB, // ha  hi  hu  he  ho
0x30DE, 0x30DF, 0x30E0, 0x30E1, 0x30E2, // ma  mi  mu  me  mo
0x30E4, 0x30A4, 0x30E6, 0x30A8, 0x30E8, // ya   i  yu   e  yo 
0x30E9, 0x30EA, 0x30EB, 0x30EC, 0x30ED, // ra  ri  ru  re  ro
0x30EF, 0x30F0, 0x30A6, 0x30F1, 0x30F2, // wa   i   u   e   o
0x30AC, 0x30AE, 0x30B0, 0x30B2, 0x30B4, // ga  gi  gu  ge  go
0x30B6, 0x30B8, 0x30BA, 0x30BC, 0x30BE, // za  ji  zu  ze  zo
0x30C0, 0x30C2, 0x30C5, 0x30C7, 0x30C9, // da  ji  zu  de  do
0x30D0, 0x30D3, 0x30D6, 0x30D9, 0x30DC, // ba  bi  bu  be  bo
0x30D1, 0x30D4, 0x30D7, 0x30DA, 0x30DD, // pa  pi  pu  pe  po
0x30F4, 0x30F3                          // vu   n
};

#define HIRAGANA_CHARS_SIZE 77
static PRUnichar gHiraganaChars[HIRAGANA_CHARS_SIZE] =
{
0x3042, 0x3044, 0x3046, 0x3048, 0x304A, //  a   i   u   e   o
0x304B, 0x304D, 0x304F, 0x3051, 0x3053, // ka  ki  ku  ke  ko
0x3055, 0x3057, 0x3059, 0x305B, 0x305D, // sa shi  su  se  so
0x305F, 0x3061, 0x3064, 0x3066, 0x3068, // ta chi tsu  te  to
0x306A, 0x306B, 0x306C, 0x306D, 0x306E, // na  ni  nu  ne  no
0x306F, 0x3072, 0x3075, 0x3078, 0x307B, // ha  hi  hu  he  ho
0x307E, 0x307F, 0x3080, 0x3081, 0x3082, // ma  mi  mu  me  mo
0x3084, 0x3044, 0x3086, 0x3048, 0x3088, // ya   i  yu   e  yo 
0x3089, 0x308A, 0x308B, 0x308C, 0x308D, // ra  ri  ru  re  ro
0x308F, 0x3090, 0x3046, 0x3091, 0x3092, // wa   i   u   e   o
0x304C, 0x304E, 0x3050, 0x3052, 0x3054, // ga  gi  gu  ge  go
0x3056, 0x3058, 0x305A, 0x305C, 0x305E, // za  ji  zu  ze  zo
0x3060, 0x3062, 0x3065, 0x3067, 0x3069, // da  ji  zu  de  do
0x3070, 0x3073, 0x3076, 0x3079, 0x306C, // ba  bi  bu  be  bo
0x3071, 0x3074, 0x3077, 0x307A, 0x307D, // pa  pi  pu  pe  po
0x3094, 0x3093                          // vu   n
};


#define HIRAGANA_IROHA_CHARS_SIZE 47
// Page 94 Writing Systems of The World
static PRUnichar gHiraganaIrohaChars[HIRAGANA_IROHA_CHARS_SIZE] =
{
0x3044, 0x308D, 0x306F, 0x306B, 0x307B, //  i  ro  ha  ni  ho
0x3078, 0x3068, 0x3061, 0x308A, 0x306C, // he  to chi  ri  nu
0x308B, 0x3092, 0x308F, 0x304B, 0x3088, // ru   o  wa  ka  yo
0x305F, 0x308C, 0x305D, 0x3064, 0x306D, // ta  re  so tsu  ne
0x306A, 0x3089, 0x3080, 0x3046, 0x3090, // na  ra  mu   u   i
0x306E, 0x304A, 0x304F, 0x3084, 0x307E, // no   o  ku  ya  ma
0x3051, 0x3075, 0x3053, 0x3048, 0x3066, // ke  hu  ko   e  te
0x3042, 0x3055, 0x304D, 0x3086, 0x3081, //  a  sa  ki  yu  me
0x307F, 0x3057, 0x3091, 0x3072, 0x3082, // mi shi   e  hi  mo 
0x305B, 0x3059                          // se su
};

#define KATAKANA_IROHA_CHARS_SIZE 47
static PRUnichar gKatakanaIrohaChars[KATAKANA_IROHA_CHARS_SIZE] =
{
0x30A4, 0x30ED, 0x30CF, 0x30CB, 0x30DB, //  i  ro  ha  ni  ho
0x30D8, 0x30C8, 0x30C1, 0x30EA, 0x30CC, // he  to chi  ri  nu
0x30EB, 0x30F2, 0x30EF, 0x30AB, 0x3088, // ru   o  wa  ka  yo
0x30BF, 0x30EC, 0x30BD, 0x30C4, 0x30CD, // ta  re  so tsu  ne
0x30CA, 0x30E9, 0x30E0, 0x30A6, 0x30F0, // na  ra  mu   u   i
0x30CE, 0x30AA, 0x30AF, 0x30E4, 0x30DE, // no   o  ku  ya  ma
0x30B1, 0x30D5, 0x30B3, 0x30A8, 0x30C6, // ke  hu  ko   e  te
0x30A2, 0x30B5, 0x30AD, 0x30E6, 0x30E1, //  a  sa  ki  yu  me
0x30DF, 0x30B7, 0x30F1, 0x30D2, 0x30E2, // mi shi   e  hi  mo 
0x30BB, 0x30B9                          // se su
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

// XXX change roman/alpha to use unsigned math so that maxint and
// maxnegint will work
void
nsBulletFrame::GetListItemText(nsIPresContext& aCX,
                               const nsStyleList& aListStyle,
                               nsString& result)
{
  PRInt32 ordinal = mOrdinal;
  char cbuf[40];
  switch (aListStyle.mListStyleType) {
	case NS_STYLE_LIST_STYLE_HEBREW: // XXX Change me i18n 
	case NS_STYLE_LIST_STYLE_ARMENIAN: // XXX Change me i18n 
	case NS_STYLE_LIST_STYLE_GEORGIAN: // XXX Change me i18n 
	case NS_STYLE_LIST_STYLE_CJK_IDEOGRAPHIC: // XXX Change me i18n 

    case NS_STYLE_LIST_STYLE_DECIMAL:
 	default: // CSS2 say "A users  agent that does not recognize a numbering system
		     // should use 'decimal'
     PR_snprintf(cbuf, sizeof(cbuf), "%ld", ordinal);
      result.Append(cbuf);
      break;

    case NS_STYLE_LIST_STYLE_LOWER_ROMAN:
    case NS_STYLE_LIST_STYLE_UPPER_ROMAN:
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

      const char* achars;
      const char* bchars;
      if (aListStyle.mListStyleType == NS_STYLE_LIST_STYLE_LOWER_ROMAN) {
        achars = gLowerRomanCharsA;
        bchars = gLowerRomanCharsB;
      }
      else {
        achars = gUpperRomanCharsA;
        bchars = gUpperRomanCharsB;
      }
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
    break;

    case NS_STYLE_LIST_STYLE_LOWER_ALPHA:
    case NS_STYLE_LIST_STYLE_UPPER_ALPHA:
    {
      PRInt32 anOffset = -1;
      PRInt32 aBase = 26;
      PRInt32 ndex=0;
      PRInt32 root=1;
      PRInt32 next=aBase;
      PRInt32 expn=1;
      const char* chars =
        (aListStyle.mListStyleType == NS_STYLE_LIST_STYLE_LOWER_ALPHA)
        ? gLowerAlphaChars : gUpperAlphaChars;

      // must be positive here...
      if (ordinal <= 0) {
        ordinal = 1;
      }
      ordinal--;          // a == 0

      // scale up in baseN; exceed current value.
      while (next<=ordinal) {
        root=next;
        next*=aBase;
        expn++;
      }
      while (0!=(expn--)) {
        ndex = ((root<=ordinal) && (0!=root)) ? (ordinal/root): 0;
        ordinal %= root;
        if (root>1)
          result.Append(chars[ndex+anOffset]);
        else
          result.Append(chars[ndex]);
        root /= aBase;
      }
    }
    break;
    case NS_STYLE_LIST_STYLE_KATAKANA:
    case NS_STYLE_LIST_STYLE_HIRAGANA:
    case NS_STYLE_LIST_STYLE_KATAKANA_IROHA:
    case NS_STYLE_LIST_STYLE_HIRAGANA_IROHA:
    case NS_STYLE_LIST_STYLE_LOWER_GREEK:
    {
      PRInt32 anOffset = -1;
      PRInt32 aBase;
      PRInt32 ndex=0;
      PRInt32 root=1;
	  PRInt32 expn=1;
      const PRUnichar* chars;
	  switch(aListStyle.mListStyleType)
	  {
		case NS_STYLE_LIST_STYLE_KATAKANA:
			chars = gKatakanaChars;
			aBase = KATAKANA_CHARS_SIZE;
			break;
		case NS_STYLE_LIST_STYLE_HIRAGANA:
			chars = gHiraganaChars;
			aBase = HIRAGANA_CHARS_SIZE;
			break;
		case NS_STYLE_LIST_STYLE_KATAKANA_IROHA:
			chars = gKatakanaIrohaChars;
			aBase = KATAKANA_IROHA_CHARS_SIZE;
			break;
		case NS_STYLE_LIST_STYLE_HIRAGANA_IROHA:
			chars = gHiraganaIrohaChars;
			aBase = HIRAGANA_IROHA_CHARS_SIZE;
			break;
		default:
		case NS_STYLE_LIST_STYLE_LOWER_GREEK:
			chars = gLowerGreekChars;
			aBase = LOWER_GREEK_CHARS_SIZE;
			break;
	  }

      PRInt32 next=aBase;
       // must be positive here...
      if (ordinal <= 0) {
        ordinal = 1;
      }
      ordinal--;          // a == 0

      // scale up in baseN; exceed current value.
      while (next<=ordinal) {
        root=next;
        next*=aBase;
        expn++;
      }
      while (0!=(expn--)) {
        ndex = ((root<=ordinal) && (0!=root)) ? (ordinal/root): 0;
        ordinal %= root;
        if (root>1)
          result.Append(chars[ndex+anOffset]);
        else
          result.Append(chars[ndex]);
        root /= aBase;
      }
    }
	break;

  }
  result.Append(".");
}

#define MIN_BULLET_SIZE 5               // from laytext.c

static nsresult
UpdateBulletCB(nsIPresContext& aPresContext, nsIFrame* aFrame, PRIntn aStatus)
{
  nsresult rv = NS_OK;
  if (NS_IMAGE_LOAD_STATUS_SIZE_AVAILABLE & aStatus) {
    // Now that the size is available, trigger a reflow of the bullet
    // frame.
    nsCOMPtr<nsIPresShell> shell;
    rv = aPresContext.GetShell(getter_AddRefs(shell));
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
    mImageLoader.SetURLSpec(myList->mListStyleImage);
    nsIURL* baseURL = nsnull;
    nsIHTMLContent* htmlContent;
    if (NS_SUCCEEDED(mContent->QueryInterface(kIHTMLContentIID, (void**)&htmlContent))) {
      htmlContent->GetBaseURL(baseURL);
      NS_RELEASE(htmlContent);
    }
    else {
      nsIDocument* doc;
      if (NS_SUCCEEDED(mContent->GetDocument(doc))) {
        doc->GetBaseURL(baseURL);
        NS_RELEASE(doc);
      }
    }
    mImageLoader.SetBaseURL(baseURL);
    NS_IF_RELEASE(baseURL);
    mImageLoader.GetDesiredSize(aCX, aReflowState, this, UpdateBulletCB,
                                aMetrics);
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
