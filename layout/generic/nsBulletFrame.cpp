/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

/* rendering object for list-item bullets */

#include "nsCOMPtr.h"
#include "nsBulletFrame.h"
#include "nsGkAtoms.h"
#include "nsHTMLParts.h"
#include "nsHTMLContainerFrame.h"
#include "nsIFontMetrics.h"
#include "nsGenericHTMLElement.h"
#include "nsPresContext.h"
#include "nsIPresShell.h"
#include "nsIDocument.h"
#include "nsIRenderingContext.h"
#include "nsILoadGroup.h"
#include "nsIURL.h"
#include "nsNetUtil.h"
#include "prprf.h"
#ifdef IBMBIDI
#include "nsBidiPresUtils.h"
#endif // IBMBIDI
#include "nsDisplayList.h"

#include "imgILoader.h"
#include "imgIContainer.h"
#include "nsStubImageDecoderObserver.h"

#include "nsIServiceManager.h"
#include "nsIComponentManager.h"
#include "nsContentUtils.h"

class nsBulletListener : public nsStubImageDecoderObserver
{
public:
  nsBulletListener();
  virtual ~nsBulletListener();

  NS_DECL_ISUPPORTS
  // imgIDecoderObserver (override nsStubImageDecoderObserver)
  NS_IMETHOD OnStartContainer(imgIRequest *aRequest, imgIContainer *aImage);
  NS_IMETHOD OnDataAvailable(imgIRequest *aRequest, gfxIImageFrame *aFrame,
                             const nsRect *aRect);
  NS_IMETHOD OnStopDecode(imgIRequest *aRequest, nsresult status,
                          const PRUnichar *statusArg);
  // imgIContainerObserver (override nsStubImageDecoderObserver)
  NS_IMETHOD FrameChanged(imgIContainer *aContainer, gfxIImageFrame *newframe,
                          nsRect * dirtyRect);

  void SetFrame(nsBulletFrame *frame) { mFrame = frame; }

private:
  nsBulletFrame *mFrame;
};


nsBulletFrame::~nsBulletFrame()
{
}

void
nsBulletFrame::Destroy()
{
  // Stop image loading first
  if (mImageRequest) {
    mImageRequest->Cancel(NS_ERROR_FAILURE);
    mImageRequest = nsnull;
  }

  if (mListener)
    NS_REINTERPRET_CAST(nsBulletListener*, mListener.get())->SetFrame(nsnull);

  // Let base class do the rest
  nsFrame::Destroy();
}

#ifdef NS_DEBUG
NS_IMETHODIMP
nsBulletFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("Bullet"), aResult);
}
#endif

nsIAtom*
nsBulletFrame::GetType() const
{
  return nsGkAtoms::bulletFrame;
}

NS_IMETHODIMP
nsBulletFrame::DidSetStyleContext()
{
  imgIRequest *newRequest = GetStyleList()->mListStyleImage;

  if (newRequest) {

    if (!mListener) {
      nsBulletListener *listener;
      NS_NEWXPCOM(listener, nsBulletListener);
      NS_ADDREF(listener);
      listener->SetFrame(this);
      listener->QueryInterface(NS_GET_IID(imgIDecoderObserver), getter_AddRefs(mListener));
      NS_ASSERTION(mListener, "queryinterface for the listener failed");
      NS_RELEASE(listener);
    }

    PRBool needNewRequest = PR_TRUE;

    if (mImageRequest) {
      // Reload the image, maybe...
      nsCOMPtr<nsIURI> oldURI;
      mImageRequest->GetURI(getter_AddRefs(oldURI));
      nsCOMPtr<nsIURI> newURI;
      newRequest->GetURI(getter_AddRefs(newURI));
      if (oldURI && newURI) {
        PRBool same;
        newURI->Equals(oldURI, &same);
        if (same) {
          needNewRequest = PR_FALSE;
        } else {
          mImageRequest->Cancel(NS_ERROR_FAILURE);
          mImageRequest = nsnull;
        }
      }
    }

    if (needNewRequest) {
      newRequest->Clone(mListener, getter_AddRefs(mImageRequest));
    }
  } else {
    // No image request on the new style context
    if (mImageRequest) {
      mImageRequest->Cancel(NS_ERROR_FAILURE);
      mImageRequest = nsnull;
    }
  }

  return NS_OK;
}

class nsDisplayBullet : public nsDisplayItem {
public:
  nsDisplayBullet(nsBulletFrame* aFrame) : nsDisplayItem(aFrame) {
    MOZ_COUNT_CTOR(nsDisplayBullet);
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayBullet() {
    MOZ_COUNT_DTOR(nsDisplayBullet);
  }
#endif

  virtual nsIFrame* HitTest(nsDisplayListBuilder* aBuilder, nsPoint aPt) { return mFrame; }
  virtual void Paint(nsDisplayListBuilder* aBuilder, nsIRenderingContext* aCtx,
     const nsRect& aDirtyRect);
  NS_DISPLAY_DECL_NAME("Bullet")
};

void nsDisplayBullet::Paint(nsDisplayListBuilder* aBuilder,
     nsIRenderingContext* aCtx, const nsRect& aDirtyRect)
{
  NS_STATIC_CAST(nsBulletFrame*, mFrame)->
    PaintBullet(*aCtx, aBuilder->ToReferenceFrame(mFrame), aDirtyRect);
}

NS_IMETHODIMP
nsBulletFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists)
{
  if (!IsVisibleForPainting(aBuilder))
    return NS_OK;

  DO_GLOBAL_REFLOW_COUNT_DSP("nsBulletFrame");
  
  return aLists.Content()->AppendNewToTop(new (aBuilder) nsDisplayBullet(this));
}

void
nsBulletFrame::PaintBullet(nsIRenderingContext& aRenderingContext, nsPoint aPt,
                           const nsRect& aDirtyRect)
{
  const nsStyleList* myList = GetStyleList();
  PRUint8 listStyleType = myList->mListStyleType;

  if (myList->mListStyleImage && mImageRequest) {
    PRUint32 status;
    mImageRequest->GetImageStatus(&status);
    if (status & imgIRequest::STATUS_LOAD_COMPLETE &&
        !(status & imgIRequest::STATUS_ERROR)) {
      nsCOMPtr<imgIContainer> imageCon;
      mImageRequest->GetImage(getter_AddRefs(imageCon));
      if (imageCon) {
        nsRect dest(mPadding.left, mPadding.top,
                    mRect.width - (mPadding.left + mPadding.right),
                    mRect.height - (mPadding.top + mPadding.bottom));
        nsLayoutUtils::DrawImage(&aRenderingContext, imageCon,
                                 dest + aPt, aDirtyRect);
        return;
      }
    }
  }

  const nsStyleFont* myFont = GetStyleFont();
  const nsStyleColor* myColor = GetStyleColor();

  nsCOMPtr<nsIFontMetrics> fm;
  aRenderingContext.SetColor(myColor->mColor);

#ifdef IBMBIDI
  nsCharType charType = eCharType_LeftToRight;
  PRUint8 level = 0;
  PRBool isBidiSystem = PR_FALSE;
  const nsStyleVisibility* vis = GetStyleVisibility();
  PRUint32 hints = 0;
#endif // IBMBIDI

  nsAutoString text;
  switch (listStyleType) {
  case NS_STYLE_LIST_STYLE_NONE:
    break;

  default:
  case NS_STYLE_LIST_STYLE_DISC:
    aRenderingContext.FillEllipse(mPadding.left + aPt.x, mPadding.top + aPt.y,
                                  mRect.width - (mPadding.left + mPadding.right),
                                  mRect.height - (mPadding.top + mPadding.bottom));
    break;

  case NS_STYLE_LIST_STYLE_CIRCLE:
    aRenderingContext.DrawEllipse(mPadding.left + aPt.x, mPadding.top + aPt.y,
                                  mRect.width - (mPadding.left + mPadding.right),
                                  mRect.height - (mPadding.top + mPadding.bottom));
    break;

  case NS_STYLE_LIST_STYLE_SQUARE:
    aRenderingContext.FillRect(mPadding.left + aPt.x, mPadding.top + aPt.y,
                               mRect.width - (mPadding.left + mPadding.right),
                               mRect.height - (mPadding.top + mPadding.bottom));
    break;

  case NS_STYLE_LIST_STYLE_DECIMAL:
  case NS_STYLE_LIST_STYLE_OLD_DECIMAL:
  case NS_STYLE_LIST_STYLE_DECIMAL_LEADING_ZERO:
#ifdef IBMBIDI
    GetListItemText(*myList, text);
    charType = eCharType_EuropeanNumber;
    break;

  case NS_STYLE_LIST_STYLE_MOZ_ARABIC_INDIC:
    if (GetListItemText(*myList, text))
      charType = eCharType_ArabicNumber;
    else
      charType = eCharType_EuropeanNumber;
    break;

  case NS_STYLE_LIST_STYLE_HEBREW:
    aRenderingContext.GetHints(hints);
    isBidiSystem = (hints & NS_RENDERING_HINT_BIDI_REORDERING);
    if (!isBidiSystem) {
      if (GetListItemText(*myList, text)) {
         charType = eCharType_RightToLeft;
        level = 1;
      } else {
        charType = eCharType_EuropeanNumber;
      }

      if (NS_STYLE_DIRECTION_RTL == vis->mDirection) {
        text.Cut(0, 1);
        text.AppendLiteral(".");
      }
      break;
    }
    // else fall through
#endif // IBMBIDI

  case NS_STYLE_LIST_STYLE_LOWER_ROMAN:
  case NS_STYLE_LIST_STYLE_UPPER_ROMAN:
  case NS_STYLE_LIST_STYLE_LOWER_ALPHA:
  case NS_STYLE_LIST_STYLE_UPPER_ALPHA:
  case NS_STYLE_LIST_STYLE_OLD_LOWER_ROMAN:
  case NS_STYLE_LIST_STYLE_OLD_UPPER_ROMAN:
  case NS_STYLE_LIST_STYLE_OLD_LOWER_ALPHA:
  case NS_STYLE_LIST_STYLE_OLD_UPPER_ALPHA:
  case NS_STYLE_LIST_STYLE_LOWER_GREEK:
#ifndef IBMBIDI
  case NS_STYLE_LIST_STYLE_HEBREW:
#endif
  case NS_STYLE_LIST_STYLE_ARMENIAN:
  case NS_STYLE_LIST_STYLE_GEORGIAN:
  case NS_STYLE_LIST_STYLE_CJK_IDEOGRAPHIC:
  case NS_STYLE_LIST_STYLE_HIRAGANA:
  case NS_STYLE_LIST_STYLE_KATAKANA:
  case NS_STYLE_LIST_STYLE_HIRAGANA_IROHA:
  case NS_STYLE_LIST_STYLE_KATAKANA_IROHA:
  case NS_STYLE_LIST_STYLE_MOZ_SIMP_CHINESE_INFORMAL: 
  case NS_STYLE_LIST_STYLE_MOZ_SIMP_CHINESE_FORMAL: 
  case NS_STYLE_LIST_STYLE_MOZ_TRAD_CHINESE_INFORMAL: 
  case NS_STYLE_LIST_STYLE_MOZ_TRAD_CHINESE_FORMAL: 
  case NS_STYLE_LIST_STYLE_MOZ_JAPANESE_INFORMAL: 
  case NS_STYLE_LIST_STYLE_MOZ_JAPANESE_FORMAL: 
  case NS_STYLE_LIST_STYLE_MOZ_CJK_HEAVENLY_STEM:
  case NS_STYLE_LIST_STYLE_MOZ_CJK_EARTHLY_BRANCH:
#ifndef IBMBIDI
  case NS_STYLE_LIST_STYLE_MOZ_ARABIC_INDIC:
#endif
  case NS_STYLE_LIST_STYLE_MOZ_PERSIAN:
  case NS_STYLE_LIST_STYLE_MOZ_URDU:
  case NS_STYLE_LIST_STYLE_MOZ_DEVANAGARI:
  case NS_STYLE_LIST_STYLE_MOZ_GURMUKHI:
  case NS_STYLE_LIST_STYLE_MOZ_GUJARATI:
  case NS_STYLE_LIST_STYLE_MOZ_ORIYA:
  case NS_STYLE_LIST_STYLE_MOZ_KANNADA:
  case NS_STYLE_LIST_STYLE_MOZ_MALAYALAM:
  case NS_STYLE_LIST_STYLE_MOZ_BENGALI:
  case NS_STYLE_LIST_STYLE_MOZ_TAMIL:
  case NS_STYLE_LIST_STYLE_MOZ_TELUGU:
  case NS_STYLE_LIST_STYLE_MOZ_THAI:
  case NS_STYLE_LIST_STYLE_MOZ_LAO:
  case NS_STYLE_LIST_STYLE_MOZ_MYANMAR:
  case NS_STYLE_LIST_STYLE_MOZ_KHMER:
  case NS_STYLE_LIST_STYLE_MOZ_HANGUL:
  case NS_STYLE_LIST_STYLE_MOZ_HANGUL_CONSONANT:
  case NS_STYLE_LIST_STYLE_MOZ_ETHIOPIC_HALEHAME:
  case NS_STYLE_LIST_STYLE_MOZ_ETHIOPIC_NUMERIC:
  case NS_STYLE_LIST_STYLE_MOZ_ETHIOPIC_HALEHAME_AM:
  case NS_STYLE_LIST_STYLE_MOZ_ETHIOPIC_HALEHAME_TI_ER:
  case NS_STYLE_LIST_STYLE_MOZ_ETHIOPIC_HALEHAME_TI_ET:
    fm = GetPresContext()->GetMetricsFor(myFont->mFont);
#ifdef IBMBIDI
    // If we can't render our numeral using the chars in the numbering
    // system, we'll be using "decimal"...
    PRBool usedChars =
#endif // IBMBIDI
    GetListItemText(*myList, text);
#ifdef IBMBIDI
    if (!usedChars)
      charType = eCharType_EuropeanNumber;
#endif
    aRenderingContext.SetFont(fm);
    nscoord ascent;
    fm->GetMaxAscent(ascent);
    aRenderingContext.SetTextRunRTL(PR_FALSE);
    aRenderingContext.DrawString(text, mPadding.left + aPt.x,
                                 mPadding.top + aPt.y + ascent);
    break;
  }
#ifdef IBMBIDI
  if (charType != eCharType_LeftToRight) {
    nsPresContext* presContext = GetPresContext();
    fm = presContext->GetMetricsFor(myFont->mFont);
    aRenderingContext.SetFont(fm);
    nscoord ascent;
    fm->GetMaxAscent(ascent);

    nsBidiPresUtils* bidiUtils = presContext->GetBidiUtils();
    if (bidiUtils) {
      const PRUnichar* buffer = text.get();
      PRInt32 textLength = text.Length();
      PRUint32 hints = 0;
      aRenderingContext.GetHints(hints);
      PRBool isNewTextRunSystem = (hints & NS_RENDERING_HINT_NEW_TEXT_RUNS) != 0;
      if (eCharType_RightToLeft == charType) {
        bidiUtils->FormatUnicodeText(presContext, (PRUnichar*)buffer, textLength,
                                     charType, level, PR_FALSE, isNewTextRunSystem);
      }
      else {
//Mohamed
        aRenderingContext.GetHints(hints);
        isBidiSystem = (hints & NS_RENDERING_HINT_ARABIC_SHAPING);
        bidiUtils->FormatUnicodeText(presContext, (PRUnichar*)buffer, textLength,
                                     charType, level, isBidiSystem, isNewTextRunSystem);//Mohamed
      }
    }
    // XXX is this right?
    aRenderingContext.SetTextRunRTL(level);
    aRenderingContext.DrawString(text, mPadding.left + aPt.x,
                                 mPadding.top + aPt.y + ascent);
  }
#endif // IBMBIDI
}

PRInt32
nsBulletFrame::SetListItemOrdinal(PRInt32 aNextOrdinal,
                                  PRBool* aChanged)
{
  // Assume that the ordinal comes from the caller
  PRInt32 oldOrdinal = mOrdinal;
  mOrdinal = aNextOrdinal;

  // Try to get value directly from the list-item, if it specifies a
  // value attribute. Note: we do this with our parent's content
  // because our parent is the list-item.
  nsIContent* parentContent = mParent->GetContent();
  if (parentContent) {
    nsGenericHTMLElement *hc =
      nsGenericHTMLElement::FromContent(parentContent);
    if (hc) {
      const nsAttrValue* attr = hc->GetParsedAttr(nsGkAtoms::value);
      if (attr && attr->Type() == nsAttrValue::eInteger) {
        // Use ordinal specified by the value attribute
        mOrdinal = attr->GetIntegerValue();
      }
    }
  }

  *aChanged = oldOrdinal != mOrdinal;

  return mOrdinal + 1;
}


// XXX change roman/alpha to use unsigned math so that maxint and
// maxnegint will work

/**
 * For all functions below, a return value of PR_TRUE means that we
 * could represent mOrder in the desired numbering system.  PR_FALSE
 * means we had to fall back to decimal
 */
static PRBool DecimalToText(PRInt32 ordinal, nsString& result)
{
   char cbuf[40];
   PR_snprintf(cbuf, sizeof(cbuf), "%ld", ordinal);
   result.AppendASCII(cbuf);
   return PR_TRUE;
}
static PRBool DecimalLeadingZeroToText(PRInt32 ordinal, nsString& result)
{
   char cbuf[40];
   PR_snprintf(cbuf, sizeof(cbuf), "%02ld", ordinal);
   result.AppendASCII(cbuf);
   return PR_TRUE;
}
static PRBool OtherDecimalToText(PRInt32 ordinal, PRUnichar zeroChar, nsString& result)
{
   PRUnichar diff = zeroChar - PRUnichar('0');
   DecimalToText(ordinal, result);
   PRUnichar* p = result.BeginWriting();
   if (ordinal < 0) {
     // skip the leading '-'
     ++p;
   }     
   for(; nsnull != *p ; p++) 
      *p += diff;
   return PR_TRUE;
}
static PRBool TamilToText(PRInt32 ordinal,  nsString& result)
{
   PRUnichar diff = 0x0BE6 - PRUnichar('0');
   DecimalToText(ordinal, result); 
   if (ordinal < 1 || ordinal > 9999) {
     // Can't do those in this system.
     return PR_FALSE;
   }
   PRUnichar* p = result.BeginWriting();
   for(; nsnull != *p ; p++) 
      if(*p != PRUnichar('0'))
         *p += diff;
   return PR_TRUE;
}


static const char gLowerRomanCharsA[] = "ixcm";
static const char gUpperRomanCharsA[] = "IXCM";
static const char gLowerRomanCharsB[] = "vld";
static const char gUpperRomanCharsB[] = "VLD";

static PRBool RomanToText(PRInt32 ordinal, nsString& result, const char* achars, const char* bchars)
{
  if (ordinal < 1 || ordinal > 3999) {
    DecimalToText(ordinal, result);
    return PR_FALSE;
  }
  nsAutoString addOn, decStr;
  decStr.AppendInt(ordinal, 10);
  PRIntn len = decStr.Length();
  const PRUnichar* dp = decStr.get();
  const PRUnichar* end = dp + len;
  PRIntn romanPos = len;
  PRIntn n;

  for (; dp < end; dp++) {
    romanPos--;
    addOn.SetLength(0);
    switch(*dp) {
      case '3':  addOn.Append(PRUnichar(achars[romanPos]));
      case '2':  addOn.Append(PRUnichar(achars[romanPos]));
      case '1':  addOn.Append(PRUnichar(achars[romanPos]));
        break;
      case '4':
        addOn.Append(PRUnichar(achars[romanPos]));
        // FALLTHROUGH
      case '5': case '6':
      case '7': case  '8':
        addOn.Append(PRUnichar(bchars[romanPos]));
        for(n=0;n<(*dp-'5');n++) {
          addOn.Append(PRUnichar(achars[romanPos]));
        }
        break;
      case '9':
        addOn.Append(PRUnichar(achars[romanPos]));
        addOn.Append(PRUnichar(achars[romanPos+1]));
        break;
      default:
        break;
    }
    result.Append(addOn);
  }
  return PR_TRUE;
}

#define ALPHA_SIZE 26
static const PRUnichar gLowerAlphaChars[ALPHA_SIZE]  = 
{
0x0061, 0x0062, 0x0063, 0x0064, 0x0065, // A   B   C   D   E
0x0066, 0x0067, 0x0068, 0x0069, 0x006A, // F   G   H   I   J
0x006B, 0x006C, 0x006D, 0x006E, 0x006F, // K   L   M   N   O
0x0070, 0x0071, 0x0072, 0x0073, 0x0074, // P   Q   R   S   T
0x0075, 0x0076, 0x0077, 0x0078, 0x0079, // U   V   W   X   Y
0x007A                                  // Z
};

static const PRUnichar gUpperAlphaChars[ALPHA_SIZE]  = 
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
static const PRUnichar gKatakanaChars[KATAKANA_CHARS_SIZE] =
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
static const PRUnichar gHiraganaChars[HIRAGANA_CHARS_SIZE] =
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
static const PRUnichar gHiraganaIrohaChars[HIRAGANA_IROHA_CHARS_SIZE] =
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
static const PRUnichar gKatakanaIrohaChars[KATAKANA_IROHA_CHARS_SIZE] =
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
static const PRUnichar gLowerGreekChars[LOWER_GREEK_CHARS_SIZE] =
{
0x03B1, 0x03B2, 0x03B3, 0x03B4, 0x03B5, // alpha  beta  gamma  delta  epsilon
0x03B6, 0x03B7, 0x03B8, 0x03B9, 0x03BA, // zeta   eta   theta  iota   kappa   
0x03BB, 0x03BC, 0x03BD, 0x03BE, 0x03BF, // lamda  mu    nu     xi     omicron 
0x03C0, 0x03C1, 0x03C3, 0x03C4, 0x03C5, // pi     rho   sigma  tau    upsilon 
0x03C6, 0x03C7, 0x03C8, 0x03C9          // phi    chi   psi    omega    
};

#define CJK_HEAVENLY_STEM_CHARS_SIZE 10 
static const PRUnichar gCJKHeavenlyStemChars[CJK_HEAVENLY_STEM_CHARS_SIZE] =
{
0x7532, 0x4e59, 0x4e19, 0x4e01, 0x620a,
0x5df1, 0x5e9a, 0x8f9b, 0x58ec, 0x7678
};
#define CJK_EARTHLY_BRANCH_CHARS_SIZE 12 
static const PRUnichar gCJKEarthlyBranchChars[CJK_EARTHLY_BRANCH_CHARS_SIZE] =
{
0x5b50, 0x4e11, 0x5bc5, 0x536f, 0x8fb0, 0x5df3,
0x5348, 0x672a, 0x7533, 0x9149, 0x620c, 0x4ea5
};
#define HANGUL_CHARS_SIZE 14 
static const PRUnichar gHangulChars[HANGUL_CHARS_SIZE] =
{
0xac00, 0xb098, 0xb2e4, 0xb77c, 0xb9c8, 0xbc14,
0xc0ac, 0xc544, 0xc790, 0xcc28, 0xce74, 0xd0c0,
0xd30c, 0xd558
};
#define HANGUL_CONSONANT_CHARS_SIZE 14 
static const PRUnichar gHangulConsonantChars[HANGUL_CONSONANT_CHARS_SIZE] =
{                                      
0x3131, 0x3134, 0x3137, 0x3139, 0x3141, 0x3142,
0x3145, 0x3147, 0x3148, 0x314a, 0x314b, 0x314c,
0x314d, 0x314e
};

// Ge'ez set of Ethiopic ordered list. There are other locale-dependent sets.
// For the time being, let's implement two Ge'ez sets only
// per Momoi san's suggestion in bug 102252. 
// For details, refer to http://www.ethiopic.org/Collation/OrderedLists.html.
#define ETHIOPIC_HALEHAME_CHARS_SIZE 26
static const PRUnichar gEthiopicHalehameChars[ETHIOPIC_HALEHAME_CHARS_SIZE] =
{                                      
0x1200, 0x1208, 0x1210, 0x1218, 0x1220, 0x1228,
0x1230, 0x1240, 0x1260, 0x1270, 0x1280, 0x1290,
0x12a0, 0x12a8, 0x12c8, 0x12d0, 0x12d8, 0x12e8,
0x12f0, 0x1308, 0x1320, 0x1330, 0x1338, 0x1340,
0x1348, 0x1350
};
#define ETHIOPIC_HALEHAME_AM_CHARS_SIZE 33
static const PRUnichar gEthiopicHalehameAmChars[ETHIOPIC_HALEHAME_AM_CHARS_SIZE] =
{                                      
0x1200, 0x1208, 0x1210, 0x1218, 0x1220, 0x1228,
0x1230, 0x1238, 0x1240, 0x1260, 0x1270, 0x1278,
0x1280, 0x1290, 0x1298, 0x12a0, 0x12a8, 0x12b8,
0x12c8, 0x12d0, 0x12d8, 0x12e0, 0x12e8, 0x12f0,
0x1300, 0x1308, 0x1320, 0x1328, 0x1330, 0x1338,
0x1340, 0x1348, 0x1350
};
#define ETHIOPIC_HALEHAME_TI_ER_CHARS_SIZE 31
static const PRUnichar gEthiopicHalehameTiErChars[ETHIOPIC_HALEHAME_TI_ER_CHARS_SIZE] =
{                                      
0x1200, 0x1208, 0x1210, 0x1218, 0x1228, 0x1230,
0x1238, 0x1240, 0x1250, 0x1260, 0x1270, 0x1278,
0x1290, 0x1298, 0x12a0, 0x12a8, 0x12b8, 0x12c8,
0x12d0, 0x12d8, 0x12e0, 0x12e8, 0x12f0, 0x1300,
0x1308, 0x1320, 0x1328, 0x1330, 0x1338, 0x1348,
0x1350
};
#define ETHIOPIC_HALEHAME_TI_ET_CHARS_SIZE 34
static const PRUnichar gEthiopicHalehameTiEtChars[ETHIOPIC_HALEHAME_TI_ET_CHARS_SIZE] =
{                                      
0x1200, 0x1208, 0x1210, 0x1218, 0x1220, 0x1228,
0x1230, 0x1238, 0x1240, 0x1250, 0x1260, 0x1270,
0x1278, 0x1280, 0x1290, 0x1298, 0x12a0, 0x12a8,
0x12b8, 0x12c8, 0x12d0, 0x12d8, 0x12e0, 0x12e8,
0x12f0, 0x1300, 0x1308, 0x1320, 0x1328, 0x1330,
0x1338, 0x1340, 0x1348, 0x1350
};


// We know cjk-ideographic need 31 characters to display 99,999,999,999,999,999
// georgian needs 6 at most
// armenian needs 12 at most
// hebrew may need more...

#define NUM_BUF_SIZE 34 

static PRBool CharListToText(PRInt32 ordinal, nsString& result, const PRUnichar* chars, PRInt32 aBase)
{
  PRUnichar buf[NUM_BUF_SIZE];
  PRInt32 idx = NUM_BUF_SIZE;
  if (ordinal < 1) {
    DecimalToText(ordinal, result);
    return PR_FALSE;
  }
  do {
    ordinal--; // a == 0
    PRInt32 cur = ordinal % aBase;
    buf[--idx] = chars[cur];
    ordinal /= aBase ;
  } while ( ordinal > 0);
  result.Append(buf+idx,NUM_BUF_SIZE-idx);
  return PR_TRUE;
}


static const PRUnichar gCJKIdeographicDigit1[10] =
{
  0x96f6, 0x4e00, 0x4e8c, 0x4e09, 0x56db,  // 0 - 4
  0x4e94, 0x516d, 0x4e03, 0x516b, 0x4e5d   // 5 - 9
};
static const PRUnichar gCJKIdeographicDigit2[10] =
{
  0x96f6, 0x58f9, 0x8cb3, 0x53c3, 0x8086,  // 0 - 4
  0x4f0d, 0x9678, 0x67d2, 0x634c, 0x7396   // 5 - 9
};
static const PRUnichar gCJKIdeographicDigit3[10] =
{
  0x96f6, 0x58f9, 0x8d30, 0x53c1, 0x8086,  // 0 - 4
  0x4f0d, 0x9646, 0x67d2, 0x634c, 0x7396   // 5 - 9
};
static const PRUnichar gCJKIdeographicUnit1[4] =
{
  0x000, 0x5341, 0x767e, 0x5343
};
static const PRUnichar gCJKIdeographicUnit2[4] =
{
  0x000, 0x62FE, 0x4F70, 0x4EDF
};
static const PRUnichar gCJKIdeographic10KUnit1[4] =
{
  0x000, 0x842c, 0x5104, 0x5146
};
static const PRUnichar gCJKIdeographic10KUnit2[4] =
{
  0x000, 0x4E07, 0x4ebf, 0x5146
};
static const PRUnichar gCJKIdeographic10KUnit3[4] =
{
  0x000, 0x4E07, 0x5104, 0x5146
};

static const PRBool CJKIdeographicToText(PRInt32 ordinal, nsString& result, 
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
  if (ordinal < 0) {
    DecimalToText(ordinal, result);
    return PR_FALSE;
  }
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
    ++ud;

  } while( ordinal > 0);
  result.Append(buf+idx,NUM_BUF_SIZE-idx);
// }
  return PR_TRUE;
}

#define HEBREW_THROSAND_SEP 0x0020
#define HEBREW_GERESH       0x05F3
#define HEBREW_GERSHAYIM    0x05F4
static const PRUnichar gHebrewDigit[22] = 
{
//   1       2       3       4       5       6       7       8       9
0x05D0, 0x05D1, 0x05D2, 0x05D3, 0x05D4, 0x05D5, 0x05D6, 0x05D7, 0x05D8,
//  10      20      30      40      50      60      70      80      90
0x05D9, 0x05DB, 0x05DC, 0x05DE, 0x05E0, 0x05E1, 0x05E2, 0x05E4, 0x05E6,
// 100     200     300     400
0x05E7, 0x05E8, 0x05E9, 0x05EA
};

static PRBool HebrewToText(PRInt32 ordinal, nsString& result)
{
  if (ordinal < 0) {
    DecimalToText(ordinal, result);
    return PR_FALSE;
  }
  if (ordinal == 0) {
    // This one is treated specially
#ifdef IBMBIDI
    static const PRUnichar hebrewZero[] = { 0x05D0, 0x05E4, 0x05E1 };
#else
    static const PRUnichar hebrewZero[] = { 0x05E1, 0x05E4, 0x05D0 };
#endif // IBMBIDI
    result.Append(hebrewZero);
    return PR_TRUE;
  }
  PRBool outputSep = PR_FALSE;
  PRUnichar buf[NUM_BUF_SIZE];
#ifdef IBMBIDI
  // Changes: 1) don't reverse the text; 2) don't insert geresh/gershayim.
  PRInt32 idx = 0;
#else
  PRInt32 idx = NUM_BUF_SIZE;
#endif // IBMBIDI
  PRUnichar digit;
  do {
    PRInt32 n3 = ordinal % 1000;
    if(outputSep)
#ifdef IBMBIDI
      buf[idx++] = HEBREW_THROSAND_SEP; // output thousand separator
#else
      buf[--idx] = HEBREW_THROSAND_SEP; // output thousand separator
#endif // IBMBIDI
    outputSep = ( n3 > 0); // request to output thousand separator next time.

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
#ifdef IBMBIDI
          buf[idx++] = digit;
#else
          buf[--idx] = digit;
#endif // IBMBIDI
          ++d;
        } else { 
          // if this is the last digit
#ifdef IBMBIDI
          buf[idx++] = digit;
#else
          if (d > 0)
          {
            buf[--idx] = HEBREW_GERSHAYIM; 
            buf[--idx] = digit;
          } else {
            buf[--idx] = digit;    
            buf[--idx] = HEBREW_GERESH;
          } // if
#endif // IBMBIDI
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
#ifdef IBMBIDI
        buf[idx++] = digit;
#else
        buf[--idx] = digit;
#endif // IBMBIDI
        ++d;
      } else {
        // if this is the last digit
#ifdef IBMBIDI
        buf[idx++] = digit;
#else
        if (d > 0)
        {
          buf[--idx] = HEBREW_GERSHAYIM;  
          buf[--idx] = digit;
        } else {
          buf[--idx] = digit;    
          buf[--idx] = HEBREW_GERESH;
        } // if
#endif // IBMBIDI
      } // if
    } // if
  
    // Process digit for 1 - 9 
    if ( n3 > 0)
    {
      digit = gHebrewDigit[n3-1];
      // must be the last digit
#ifdef IBMBIDI
      buf[idx++] = digit;
#else
      if (d > 0)
      {
        buf[--idx] = HEBREW_GERSHAYIM; 
        buf[--idx] = digit;
      } else {
        buf[--idx] = digit;    
        buf[--idx] = HEBREW_GERESH;
      } // if
#endif // IBMBIDI
    } // if
    ordinal /= 1000;
  } while (ordinal >= 1);
#ifdef IBMBIDI
  result.Append(buf, idx);
#else
  result.Append(buf+idx,NUM_BUF_SIZE-idx);
#endif // IBMBIDI
  return PR_TRUE;
}


static PRBool ArmenianToText(PRInt32 ordinal, nsString& result)
{
  // XXXbz this system goes out to a lot further than 9999... we should fix
  // that.  This algorithm seems broken in general.  There's this business of
  // "7000" being special and then there's the combining accent we're supposed
  // to be using...
  if (ordinal < 1 || ordinal > 9999) { // zero or reach the limit of Armenian numbering system
    DecimalToText(ordinal, result);
    return PR_FALSE;
  }

  PRUnichar buf[NUM_BUF_SIZE];
  PRInt32 idx = NUM_BUF_SIZE;
  PRInt32 d = 0;
  do {
    PRInt32 cur = ordinal % 10;
    if (cur > 0)
    {
      PRUnichar u = 0x0530 + (d * 9) + cur;
      buf[--idx] = u;
    }
    ++d;
    ordinal /= 10;
  } while (ordinal > 0);
  result.Append(buf + idx, NUM_BUF_SIZE - idx);
  return PR_TRUE;
}


static const PRUnichar gGeorgianValue [ 37 ] = { // 4 * 9 + 1 = 37
//      1       2       3       4       5       6       7       8       9
   0x10D0, 0x10D1, 0x10D2, 0x10D3, 0x10D4, 0x10D5, 0x10D6, 0x10F1, 0x10D7,
//     10      20      30      40      50      60      70      80      90
   0x10D8, 0x10D9, 0x10DA, 0x10DB, 0x10DC, 0x10F2, 0x10DD, 0x10DE, 0x10DF,
//    100     200     300     400     500     600     700     800     900
   0x10E0, 0x10E1, 0x10E2, 0x10F3, 0x10E4, 0x10E5, 0x10E6, 0x10E7, 0x10E8,
//   1000    2000    3000    4000    5000    6000    7000    8000    9000
   0x10E9, 0x10EA, 0x10EB, 0x10EC, 0x10ED, 0x10EE, 0x10F4, 0x10EF, 0x10F0,
//  10000
   0x10F5
};
static PRBool GeorgianToText(PRInt32 ordinal, nsString& result)
{
  if (ordinal < 1 || ordinal > 19999) { // zero or reach the limit of Georgian numbering system
    DecimalToText(ordinal, result);
    return PR_FALSE;
  }

  PRUnichar buf[NUM_BUF_SIZE];
  PRInt32 idx = NUM_BUF_SIZE;
  PRInt32 d = 0;
  do {
    PRInt32 cur = ordinal % 10;
    if (cur > 0)
    {
      PRUnichar u = gGeorgianValue[(d * 9 ) + ( cur - 1)];
      buf[--idx] = u;
    }
    ++d;
    ordinal /= 10;
  } while (ordinal > 0);
  result.Append(buf + idx, NUM_BUF_SIZE - idx);
  return PR_TRUE;
}

// Convert ordinal to Ethiopic numeric representation.
// The detail is available at http://www.ethiopic.org/Numerals/
// The algorithm used here is based on the pseudo-code put up there by
// Daniel Yacob <yacob@geez.org>.
// Another reference is Unicode 3.0 standard section 11.1.
#define ETHIOPIC_ONE             0x1369
#define ETHIOPIC_TEN             0x1372
#define ETHIOPIC_HUNDRED         0x137B
#define ETHIOPIC_TEN_THOUSAND    0x137C

static PRBool EthiopicToText(PRInt32 ordinal, nsString& result)
{
  nsAutoString asciiNumberString;      // decimal string representation of ordinal
  DecimalToText(ordinal, asciiNumberString);
  if (ordinal < 1) {
    result.Append(asciiNumberString);
    return PR_FALSE;
  }
  PRUint8 asciiStringLength = asciiNumberString.Length();

  // If number length is odd, add a leading "0"
  // the leading "0" preconditions the string to always have the
  // leading tens place populated, this avoids a check within the loop.
  // If we didn't add the leading "0", decrement asciiStringLength so
  // it will be equivalent to a zero-based index in both cases.
  if (asciiStringLength & 1) {
    asciiNumberString.Insert(NS_LITERAL_STRING("0"), 0);
  } else {
    asciiStringLength--;
  }

  // Iterate from the highest digits to lowest
  // indexFromLeft       indexes digits (0 = most significant)
  // groupIndexFromRight indexes pairs of digits (0 = least significant)
  for (PRUint8 indexFromLeft = 0, groupIndexFromRight = asciiStringLength >> 1;
       indexFromLeft <= asciiStringLength;
       indexFromLeft += 2, groupIndexFromRight--) {
    PRUint8 tensValue  = asciiNumberString.CharAt(indexFromLeft) & 0x0F;
    PRUint8 unitsValue = asciiNumberString.CharAt(indexFromLeft + 1) & 0x0F;
    PRUint8 groupValue = tensValue * 10 + unitsValue;

    PRBool oddGroup = (groupIndexFromRight & 1);

    // we want to clear ETHIOPIC_ONE when it is superfluous
    if (ordinal > 1 &&
        groupValue == 1 &&                  // one without a leading ten
        (oddGroup || indexFromLeft == 0)) { // preceding (100) or leading the sequence
      unitsValue = 0;
    }

    // put it all together...
    if (tensValue) {
      // map onto Ethiopic "tens":
      result.Append((PRUnichar) (tensValue +  ETHIOPIC_TEN - 1));
    }
    if (unitsValue) {
      //map onto Ethiopic "units":
      result.Append((PRUnichar) (unitsValue + ETHIOPIC_ONE - 1));
    }
    // Add a separator for all even groups except the last,
    // and for odd groups with non-zero value.
    if (oddGroup) {
      if (groupValue) {
        result.Append((PRUnichar) ETHIOPIC_HUNDRED);
      }
    } else {
      if (groupIndexFromRight) {
        result.Append((PRUnichar) ETHIOPIC_TEN_THOUSAND);
      }
    }
  }
  return PR_TRUE;
}


/* static */ PRBool
nsBulletFrame::AppendCounterText(PRInt32 aListStyleType,
                                 PRInt32 aOrdinal,
                                 nsString& result)
{
  PRBool success;
  
  switch (aListStyleType) {
    case NS_STYLE_LIST_STYLE_NONE: // used by counters code only
      break;

    case NS_STYLE_LIST_STYLE_DISC: // used by counters code only
      // XXX We really need to do this the same way we do list bullets.
      result.Append(PRUnichar(0x2022));
      break;

    case NS_STYLE_LIST_STYLE_CIRCLE: // used by counters code only
      // XXX We really need to do this the same way we do list bullets.
      result.Append(PRUnichar(0x25E6));
      break;

    case NS_STYLE_LIST_STYLE_SQUARE: // used by counters code only
      // XXX We really need to do this the same way we do list bullets.
      result.Append(PRUnichar(0x25FE));
      break;

    case NS_STYLE_LIST_STYLE_DECIMAL:
    case NS_STYLE_LIST_STYLE_OLD_DECIMAL:
    default: // CSS2 say "A users  agent that does not recognize a numbering system
      // should use 'decimal'
      success = DecimalToText(aOrdinal, result);
      break;

    case NS_STYLE_LIST_STYLE_DECIMAL_LEADING_ZERO:
      success = DecimalLeadingZeroToText(aOrdinal, result);
      break;

    case NS_STYLE_LIST_STYLE_LOWER_ROMAN:
    case NS_STYLE_LIST_STYLE_OLD_LOWER_ROMAN:
      success = RomanToText(aOrdinal, result,
                            gLowerRomanCharsA, gLowerRomanCharsB);
      break;
    case NS_STYLE_LIST_STYLE_UPPER_ROMAN:
    case NS_STYLE_LIST_STYLE_OLD_UPPER_ROMAN:
      success = RomanToText(aOrdinal, result,
                            gUpperRomanCharsA, gUpperRomanCharsB);
      break;

    case NS_STYLE_LIST_STYLE_LOWER_ALPHA:
    case NS_STYLE_LIST_STYLE_OLD_LOWER_ALPHA:
      success = CharListToText(aOrdinal, result, gLowerAlphaChars, ALPHA_SIZE);
      break;

    case NS_STYLE_LIST_STYLE_UPPER_ALPHA:
    case NS_STYLE_LIST_STYLE_OLD_UPPER_ALPHA:
      success = CharListToText(aOrdinal, result, gUpperAlphaChars, ALPHA_SIZE);
      break;

    case NS_STYLE_LIST_STYLE_KATAKANA:
      success = CharListToText(aOrdinal, result, gKatakanaChars,
                               KATAKANA_CHARS_SIZE);
      break;

    case NS_STYLE_LIST_STYLE_HIRAGANA:
      success = CharListToText(aOrdinal, result, gHiraganaChars,
                               HIRAGANA_CHARS_SIZE);
      break;
    
    case NS_STYLE_LIST_STYLE_KATAKANA_IROHA:
      success = CharListToText(aOrdinal, result, gKatakanaIrohaChars,
                               KATAKANA_IROHA_CHARS_SIZE);
      break;
 
    case NS_STYLE_LIST_STYLE_HIRAGANA_IROHA:
      success = CharListToText(aOrdinal, result, gHiraganaIrohaChars,
                               HIRAGANA_IROHA_CHARS_SIZE);
      break;

    case NS_STYLE_LIST_STYLE_LOWER_GREEK:
      success = CharListToText(aOrdinal, result, gLowerGreekChars ,
                               LOWER_GREEK_CHARS_SIZE);
      break;

    case NS_STYLE_LIST_STYLE_CJK_IDEOGRAPHIC: 
    case NS_STYLE_LIST_STYLE_MOZ_TRAD_CHINESE_INFORMAL: 
      success = CJKIdeographicToText(aOrdinal, result, gCJKIdeographicDigit1,
                                     gCJKIdeographicUnit1,
                                     gCJKIdeographic10KUnit1);
      break;

    case NS_STYLE_LIST_STYLE_MOZ_TRAD_CHINESE_FORMAL: 
      success = CJKIdeographicToText(aOrdinal, result, gCJKIdeographicDigit2,
                                     gCJKIdeographicUnit2,
                                     gCJKIdeographic10KUnit1);
      break;

    case NS_STYLE_LIST_STYLE_MOZ_SIMP_CHINESE_INFORMAL: 
      success = CJKIdeographicToText(aOrdinal, result, gCJKIdeographicDigit1,
                                     gCJKIdeographicUnit1,
                                     gCJKIdeographic10KUnit2);
      break;

    case NS_STYLE_LIST_STYLE_MOZ_SIMP_CHINESE_FORMAL: 
      success = CJKIdeographicToText(aOrdinal, result, gCJKIdeographicDigit3,
                                     gCJKIdeographicUnit2,
                                     gCJKIdeographic10KUnit2);
      break;

    case NS_STYLE_LIST_STYLE_MOZ_JAPANESE_INFORMAL: 
      success = CJKIdeographicToText(aOrdinal, result, gCJKIdeographicDigit1,
                                     gCJKIdeographicUnit1,
                                     gCJKIdeographic10KUnit3);
      break;

    case NS_STYLE_LIST_STYLE_MOZ_JAPANESE_FORMAL: 
      success = CJKIdeographicToText(aOrdinal, result, gCJKIdeographicDigit2,
                                     gCJKIdeographicUnit2,
                                     gCJKIdeographic10KUnit3);
      break;

    case NS_STYLE_LIST_STYLE_HEBREW: 
      success = HebrewToText(aOrdinal, result);
      break;

    case NS_STYLE_LIST_STYLE_ARMENIAN: 
      success = ArmenianToText(aOrdinal, result);
      break;

    case NS_STYLE_LIST_STYLE_GEORGIAN: 
      success = GeorgianToText(aOrdinal, result);
      break;
 
    case NS_STYLE_LIST_STYLE_MOZ_ARABIC_INDIC:
      success = OtherDecimalToText(aOrdinal, 0x0660, result);
      break;
 
    case NS_STYLE_LIST_STYLE_MOZ_PERSIAN:
    case NS_STYLE_LIST_STYLE_MOZ_URDU:
      success = OtherDecimalToText(aOrdinal, 0x06f0, result);
      break;
 
    case NS_STYLE_LIST_STYLE_MOZ_DEVANAGARI:
      success = OtherDecimalToText(aOrdinal, 0x0966, result);
      break;
 
    case NS_STYLE_LIST_STYLE_MOZ_GURMUKHI:
      success = OtherDecimalToText(aOrdinal, 0x0a66, result);
      break;
 
    case NS_STYLE_LIST_STYLE_MOZ_GUJARATI:
      success = OtherDecimalToText(aOrdinal, 0x0AE6, result);
      break;
 
    case NS_STYLE_LIST_STYLE_MOZ_ORIYA:
      success = OtherDecimalToText(aOrdinal, 0x0B66, result);
      break;
 
    case NS_STYLE_LIST_STYLE_MOZ_KANNADA:
      success = OtherDecimalToText(aOrdinal, 0x0CE6, result);
      break;
 
    case NS_STYLE_LIST_STYLE_MOZ_MALAYALAM:
      success = OtherDecimalToText(aOrdinal, 0x0D66, result);
      break;
 
    case NS_STYLE_LIST_STYLE_MOZ_THAI:
      success = OtherDecimalToText(aOrdinal, 0x0E50, result);
      break;
 
    case NS_STYLE_LIST_STYLE_MOZ_LAO:
      success = OtherDecimalToText(aOrdinal, 0x0ED0, result);
      break;
 
    case NS_STYLE_LIST_STYLE_MOZ_MYANMAR:
      success = OtherDecimalToText(aOrdinal, 0x1040, result);
      break;
 
    case NS_STYLE_LIST_STYLE_MOZ_KHMER:
      success = OtherDecimalToText(aOrdinal, 0x17E0, result);
      break;
 
    case NS_STYLE_LIST_STYLE_MOZ_BENGALI:
      success = OtherDecimalToText(aOrdinal, 0x09E6, result);
      break;
 
    case NS_STYLE_LIST_STYLE_MOZ_TELUGU:
      success = OtherDecimalToText(aOrdinal, 0x0C66, result);
      break;
 
    case NS_STYLE_LIST_STYLE_MOZ_TAMIL:
      success = TamilToText(aOrdinal, result);
      break;

    case NS_STYLE_LIST_STYLE_MOZ_CJK_HEAVENLY_STEM:
      success = CharListToText(aOrdinal, result, gCJKHeavenlyStemChars,
                               CJK_HEAVENLY_STEM_CHARS_SIZE);
      break;

    case NS_STYLE_LIST_STYLE_MOZ_CJK_EARTHLY_BRANCH:
      success = CharListToText(aOrdinal, result, gCJKEarthlyBranchChars,
                               CJK_EARTHLY_BRANCH_CHARS_SIZE);
      break;

    case NS_STYLE_LIST_STYLE_MOZ_HANGUL:
      success = CharListToText(aOrdinal, result, gHangulChars, HANGUL_CHARS_SIZE);
      break;

    case NS_STYLE_LIST_STYLE_MOZ_HANGUL_CONSONANT:
      success = CharListToText(aOrdinal, result, gHangulConsonantChars,
                               HANGUL_CONSONANT_CHARS_SIZE);
      break;

    case NS_STYLE_LIST_STYLE_MOZ_ETHIOPIC_HALEHAME:
      success = CharListToText(aOrdinal, result, gEthiopicHalehameChars,
                               ETHIOPIC_HALEHAME_CHARS_SIZE);
      break;

    case NS_STYLE_LIST_STYLE_MOZ_ETHIOPIC_NUMERIC:
      success = EthiopicToText(aOrdinal, result);
      break;

    case NS_STYLE_LIST_STYLE_MOZ_ETHIOPIC_HALEHAME_AM:
      success = CharListToText(aOrdinal, result, gEthiopicHalehameAmChars,
                               ETHIOPIC_HALEHAME_AM_CHARS_SIZE);
      break;

    case NS_STYLE_LIST_STYLE_MOZ_ETHIOPIC_HALEHAME_TI_ER:
      success = CharListToText(aOrdinal, result, gEthiopicHalehameTiErChars,
                               ETHIOPIC_HALEHAME_TI_ER_CHARS_SIZE);
      break;

    case NS_STYLE_LIST_STYLE_MOZ_ETHIOPIC_HALEHAME_TI_ET:
      success = CharListToText(aOrdinal, result, gEthiopicHalehameTiEtChars,
                               ETHIOPIC_HALEHAME_TI_ET_CHARS_SIZE);
      break;
  }
  return success;
}

PRBool
nsBulletFrame::GetListItemText(const nsStyleList& aListStyle,
                               nsString& result)
{
#ifdef IBMBIDI
  const nsStyleVisibility* vis = GetStyleVisibility();

  // XXX For some of these systems, "." is wrong!  This should really be
  // pushed down into the individual cases!
  if (NS_STYLE_DIRECTION_RTL == vis->mDirection) {
    result.AppendLiteral(".");
  }
#endif // IBMBIDI

  NS_ASSERTION(aListStyle.mListStyleType != NS_STYLE_LIST_STYLE_NONE &&
               aListStyle.mListStyleType != NS_STYLE_LIST_STYLE_DISC &&
               aListStyle.mListStyleType != NS_STYLE_LIST_STYLE_CIRCLE &&
               aListStyle.mListStyleType != NS_STYLE_LIST_STYLE_SQUARE,
               "we should be using specialized code for these types");
  PRBool success =
    AppendCounterText(aListStyle.mListStyleType, mOrdinal, result);

  // XXX For some of these systems, "." is wrong!  This should really be
  // pushed up into the cases...
#ifdef IBMBIDI
  if (NS_STYLE_DIRECTION_RTL != vis->mDirection)
#endif // IBMBIDI
  result.AppendLiteral(".");
  return success;
}

#define MIN_BULLET_SIZE 1


void
nsBulletFrame::GetDesiredSize(nsPresContext*  aCX,
                              nsIRenderingContext *aRenderingContext,
                              nsHTMLReflowMetrics& aMetrics)
{
  // Reset our padding.  If we need it, we'll set it below.
  mPadding.SizeTo(0, 0, 0, 0);
  
  const nsStyleList* myList = GetStyleList();
  nscoord ascent;

  if (myList->mListStyleImage && mImageRequest) {
    PRUint32 status;
    mImageRequest->GetImageStatus(&status);
    if (status & imgIRequest::STATUS_SIZE_AVAILABLE &&
        !(status & imgIRequest::STATUS_ERROR)) {
      // auto size the image
      mComputedSize.width = mIntrinsicSize.width;
      mComputedSize.height = mIntrinsicSize.height;

#if 0 // don't do scaled images in bullets
      if (mComputedSize == mIntrinsicSize) {
        mTransform.SetToIdentity();
      } else {
        if (mComputedSize.width != 0 && mComputedSize.height != 0) {
          mTransform.SetToScale(float(mIntrinsicSize.width) / float(mComputedSize.width),
                                float(mIntrinsicSize.height) / float(mComputedSize.height));
        }
      }
#endif

      aMetrics.width = mComputedSize.width;
      aMetrics.ascent = aMetrics.height = mComputedSize.height;

      return;
    }
  }

  // If we're getting our desired size and don't have an image, reset
  // mIntrinsicSize to (0,0).  Otherwise, if we used to have an image, it
  // changed, and the new one is coming in, but we're reflowing before it's
  // fully there, we'll end up with mIntrinsicSize not matching our size, but
  // won't trigger a reflow in OnStartContainer (because mIntrinsicSize will
  // match the image size).
  mIntrinsicSize.SizeTo(0, 0);

  const nsStyleFont* myFont = GetStyleFont();
  nsCOMPtr<nsIFontMetrics> fm = aCX->GetMetricsFor(myFont->mFont);
  nscoord bulletSize;

  nsAutoString text;
  switch (myList->mListStyleType) {
    case NS_STYLE_LIST_STYLE_NONE:
      aMetrics.width = 0;
      aMetrics.ascent = aMetrics.height = 0;
      break;

    case NS_STYLE_LIST_STYLE_DISC:
    case NS_STYLE_LIST_STYLE_CIRCLE:
    case NS_STYLE_LIST_STYLE_SQUARE:
      fm->GetMaxAscent(ascent);
      bulletSize = PR_MAX(nsPresContext::CSSPixelsToAppUnits(MIN_BULLET_SIZE),
                          NSToCoordRound(0.8f * (float(ascent) / 2.0f)));
      mPadding.bottom = NSToCoordRound(float(ascent) / 8.0f);
      aMetrics.width = mPadding.right + bulletSize;
      aMetrics.ascent = aMetrics.height = mPadding.bottom + bulletSize;
      break;

    default:
    case NS_STYLE_LIST_STYLE_DECIMAL_LEADING_ZERO:
    case NS_STYLE_LIST_STYLE_DECIMAL:
    case NS_STYLE_LIST_STYLE_OLD_DECIMAL:
    case NS_STYLE_LIST_STYLE_LOWER_ROMAN:
    case NS_STYLE_LIST_STYLE_UPPER_ROMAN:
    case NS_STYLE_LIST_STYLE_LOWER_ALPHA:
    case NS_STYLE_LIST_STYLE_UPPER_ALPHA:
    case NS_STYLE_LIST_STYLE_OLD_LOWER_ROMAN:
    case NS_STYLE_LIST_STYLE_OLD_UPPER_ROMAN:
    case NS_STYLE_LIST_STYLE_OLD_LOWER_ALPHA:
    case NS_STYLE_LIST_STYLE_OLD_UPPER_ALPHA:
    case NS_STYLE_LIST_STYLE_KATAKANA:
    case NS_STYLE_LIST_STYLE_HIRAGANA:
    case NS_STYLE_LIST_STYLE_KATAKANA_IROHA:
    case NS_STYLE_LIST_STYLE_HIRAGANA_IROHA:
    case NS_STYLE_LIST_STYLE_LOWER_GREEK:
    case NS_STYLE_LIST_STYLE_HEBREW: 
    case NS_STYLE_LIST_STYLE_ARMENIAN: 
    case NS_STYLE_LIST_STYLE_GEORGIAN: 
    case NS_STYLE_LIST_STYLE_CJK_IDEOGRAPHIC: 
    case NS_STYLE_LIST_STYLE_MOZ_SIMP_CHINESE_INFORMAL: 
    case NS_STYLE_LIST_STYLE_MOZ_SIMP_CHINESE_FORMAL: 
    case NS_STYLE_LIST_STYLE_MOZ_TRAD_CHINESE_INFORMAL: 
    case NS_STYLE_LIST_STYLE_MOZ_TRAD_CHINESE_FORMAL: 
    case NS_STYLE_LIST_STYLE_MOZ_JAPANESE_INFORMAL: 
    case NS_STYLE_LIST_STYLE_MOZ_JAPANESE_FORMAL: 
    case NS_STYLE_LIST_STYLE_MOZ_CJK_HEAVENLY_STEM:
    case NS_STYLE_LIST_STYLE_MOZ_CJK_EARTHLY_BRANCH:
    case NS_STYLE_LIST_STYLE_MOZ_ARABIC_INDIC:
    case NS_STYLE_LIST_STYLE_MOZ_PERSIAN:
    case NS_STYLE_LIST_STYLE_MOZ_URDU:
    case NS_STYLE_LIST_STYLE_MOZ_DEVANAGARI:
    case NS_STYLE_LIST_STYLE_MOZ_GURMUKHI:
    case NS_STYLE_LIST_STYLE_MOZ_GUJARATI:
    case NS_STYLE_LIST_STYLE_MOZ_ORIYA:
    case NS_STYLE_LIST_STYLE_MOZ_KANNADA:
    case NS_STYLE_LIST_STYLE_MOZ_MALAYALAM:
    case NS_STYLE_LIST_STYLE_MOZ_BENGALI:
    case NS_STYLE_LIST_STYLE_MOZ_TAMIL:
    case NS_STYLE_LIST_STYLE_MOZ_TELUGU:
    case NS_STYLE_LIST_STYLE_MOZ_THAI:
    case NS_STYLE_LIST_STYLE_MOZ_LAO:
    case NS_STYLE_LIST_STYLE_MOZ_MYANMAR:
    case NS_STYLE_LIST_STYLE_MOZ_KHMER:
    case NS_STYLE_LIST_STYLE_MOZ_HANGUL:
    case NS_STYLE_LIST_STYLE_MOZ_HANGUL_CONSONANT:
    case NS_STYLE_LIST_STYLE_MOZ_ETHIOPIC_HALEHAME:
    case NS_STYLE_LIST_STYLE_MOZ_ETHIOPIC_NUMERIC:
    case NS_STYLE_LIST_STYLE_MOZ_ETHIOPIC_HALEHAME_AM:
    case NS_STYLE_LIST_STYLE_MOZ_ETHIOPIC_HALEHAME_TI_ER:
    case NS_STYLE_LIST_STYLE_MOZ_ETHIOPIC_HALEHAME_TI_ET:
      GetListItemText(*myList, text);
      fm->GetHeight(aMetrics.height);
      aRenderingContext->SetFont(fm);
      aMetrics.width = nsLayoutUtils::GetStringWidth(this, aRenderingContext, text.get(), text.Length());
      aMetrics.width += mPadding.right;
      fm->GetMaxAscent(aMetrics.ascent);
      break;
  }
}

NS_IMETHODIMP
nsBulletFrame::Reflow(nsPresContext* aPresContext,
                      nsHTMLReflowMetrics& aMetrics,
                      const nsHTMLReflowState& aReflowState,
                      nsReflowStatus& aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsBulletFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aMetrics, aStatus);

  // Get the base size
  GetDesiredSize(aPresContext, aReflowState.rendContext, aMetrics);

  // Add in the border and padding; split the top/bottom between the
  // ascent and descent to make things look nice
  const nsMargin& borderPadding = aReflowState.mComputedBorderPadding;
  aMetrics.width += borderPadding.left + borderPadding.right;
  aMetrics.height += borderPadding.top + borderPadding.bottom;
  aMetrics.ascent += borderPadding.top;

  aStatus = NS_FRAME_COMPLETE;
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aMetrics);
  return NS_OK;
}

/* virtual */ nscoord
nsBulletFrame::GetMinWidth(nsIRenderingContext *aRenderingContext)
{
  nsHTMLReflowMetrics metrics;
  DISPLAY_MIN_WIDTH(this, metrics.width);
  GetDesiredSize(GetPresContext(), aRenderingContext, metrics);
  return metrics.width;
}

/* virtual */ nscoord
nsBulletFrame::GetPrefWidth(nsIRenderingContext *aRenderingContext)
{
  nsHTMLReflowMetrics metrics;
  DISPLAY_PREF_WIDTH(this, metrics.width);
  GetDesiredSize(GetPresContext(), aRenderingContext, metrics);
  return metrics.width;
}


NS_IMETHODIMP nsBulletFrame::OnStartContainer(imgIRequest *aRequest,
                                              imgIContainer *aImage)
{
  if (!aImage) return NS_ERROR_INVALID_ARG;
  if (!aRequest) return NS_ERROR_INVALID_ARG;

  PRUint32 status;
  aRequest->GetImageStatus(&status);
  if (status & imgIRequest::STATUS_ERROR) {
    return NS_OK;
  }
  
  nscoord w, h;
  aImage->GetWidth(&w);
  aImage->GetHeight(&h);

  nsPresContext* presContext = GetPresContext();

  nsSize newsize(nsPresContext::CSSPixelsToAppUnits(w),
                 nsPresContext::CSSPixelsToAppUnits(h));

  if (mIntrinsicSize != newsize) {
    mIntrinsicSize = newsize;

    // Now that the size is available (or an error occurred), trigger
    // a reflow of the bullet frame.
    nsIPresShell *shell = presContext->GetPresShell();
    if (shell) {
      NS_ASSERTION(mParent, "No parent to pass the reflow request up to.");
      if (mParent) {
        mState |= NS_FRAME_IS_DIRTY;
        shell->FrameNeedsReflow(this, nsIPresShell::eStyleChange);
      }
    }
  }

  // Handle animations
  aImage->SetAnimationMode(presContext->ImageAnimationMode());
  // Ensure the animation (if any) is started.
  aImage->StartAnimation();

  
  return NS_OK;
}

NS_IMETHODIMP nsBulletFrame::OnDataAvailable(imgIRequest *aRequest,
                                             gfxIImageFrame *aFrame,
                                             const nsRect *aRect)
{
  // The image has changed.
  // Invalidate the entire content area. Maybe it's not optimal but it's simple and
  // always correct, and I'll be a stunned mullet if it ever matters for performance
  Invalidate(nsRect(0, 0, mRect.width, mRect.height), PR_FALSE);

  return NS_OK;
}

NS_IMETHODIMP nsBulletFrame::OnStopDecode(imgIRequest *aRequest,
                                          nsresult aStatus,
                                          const PRUnichar *aStatusArg)
{
  // XXX should the bulletframe do anything if the image failed to load?
  //     it didn't in the old code...

#if 0
  if (NS_FAILED(aStatus)) {
    // We failed to load the image. Notify the pres shell
    if (NS_FAILED(aStatus) && (mImageRequest == aRequest || !mImageRequest)) {
      imageFailed = PR_TRUE;
    }
  }
#endif

  return NS_OK;
}

NS_IMETHODIMP nsBulletFrame::FrameChanged(imgIContainer *aContainer,
                                          gfxIImageFrame *aNewFrame,
                                          nsRect *aDirtyRect)
{
  // Invalidate the entire content area. Maybe it's not optimal but it's simple and
  // always correct.
  Invalidate(nsRect(0, 0, mRect.width, mRect.height), PR_FALSE);

  return NS_OK;
}

void
nsBulletFrame::GetLoadGroup(nsPresContext *aPresContext, nsILoadGroup **aLoadGroup)
{
  if (!aPresContext)
    return;

  NS_PRECONDITION(nsnull != aLoadGroup, "null OUT parameter pointer");

  nsIPresShell *shell = aPresContext->GetPresShell();

  if (!shell)
    return;

  nsIDocument *doc = shell->GetDocument();
  if (!doc)
    return;

  *aLoadGroup = doc->GetDocumentLoadGroup().get();  // already_AddRefed
}








NS_IMPL_ISUPPORTS2(nsBulletListener, imgIDecoderObserver, imgIContainerObserver)

nsBulletListener::nsBulletListener() :
  mFrame(nsnull)
{
}

nsBulletListener::~nsBulletListener()
{
}

NS_IMETHODIMP nsBulletListener::OnStartContainer(imgIRequest *aRequest,
                                                 imgIContainer *aImage)
{
  if (!mFrame)
    return NS_ERROR_FAILURE;

  return mFrame->OnStartContainer(aRequest, aImage);
}

NS_IMETHODIMP nsBulletListener::OnDataAvailable(imgIRequest *aRequest,
                                                gfxIImageFrame *aFrame,
                                                const nsRect *aRect)
{
  if (!mFrame)
    return NS_ERROR_FAILURE;

  return mFrame->OnDataAvailable(aRequest, aFrame, aRect);
}

NS_IMETHODIMP nsBulletListener::OnStopDecode(imgIRequest *aRequest,
                                             nsresult status,
                                             const PRUnichar *statusArg)
{
  if (!mFrame)
    return NS_ERROR_FAILURE;
  
  return mFrame->OnStopDecode(aRequest, status, statusArg);
}

NS_IMETHODIMP nsBulletListener::FrameChanged(imgIContainer *aContainer,
                                             gfxIImageFrame *newframe,
                                             nsRect * dirtyRect)
{
  if (!mFrame)
    return NS_ERROR_FAILURE;

  return mFrame->FrameChanged(aContainer, newframe, dirtyRect);
}
