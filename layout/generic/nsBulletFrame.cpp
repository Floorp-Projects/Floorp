/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
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
#include "nsILoadGroup.h"
#include "nsIURL.h"
#include "nsNetUtil.h"
#include "nsLayoutAtoms.h"
#include "prprf.h"
#ifdef IBMBIDI
#include "nsBidiPresUtils.h"
#endif // IBMBIDI

#include "imgILoader.h"
#include "imgIContainer.h"
#include "imgIDecoderObserver.h"

#include "nsIServiceManager.h"
#include "nsIComponentManager.h"


class nsBulletListener : public imgIDecoderObserver
{
public:
  nsBulletListener();
  virtual ~nsBulletListener();

  NS_DECL_ISUPPORTS
  NS_DECL_IMGIDECODEROBSERVER
  NS_DECL_IMGICONTAINEROBSERVER

  void SetFrame(nsBulletFrame *frame) { mFrame = frame; }

private:
  nsBulletFrame *mFrame;
};






nsBulletFrame::nsBulletFrame()
{
}

nsBulletFrame::~nsBulletFrame()
{
}

NS_IMETHODIMP
nsBulletFrame::Destroy(nsIPresContext* aPresContext)
{
  // Stop image loading first
  if (mImageRequest) {
    mImageRequest->Cancel(NS_ERROR_FAILURE);
    mImageRequest = nsnull;
  }

  if (mListener)
    NS_REINTERPRET_CAST(nsBulletListener*, mListener.get())->SetFrame(nsnull);

  // Let base class do the rest
  return nsFrame::Destroy(aPresContext);
}

NS_IMETHODIMP
nsBulletFrame::Init(nsIPresContext*  aPresContext,
                    nsIContent*      aContent,
                    nsIFrame*        aParent,
                    nsIStyleContext* aContext,
                    nsIFrame*        aPrevInFlow)
{
  nsresult  rv = nsFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);

  const nsStyleList* myList = (const nsStyleList*)mStyleContext->GetStyleData(eStyleStruct_List);

  if (!myList->mListStyleImage.IsEmpty()) {
    nsCOMPtr<imgILoader> il(do_GetService("@mozilla.org/image/loader;1", &rv));
    if (NS_FAILED(rv))
      return rv;

    nsCOMPtr<nsILoadGroup> loadGroup;
    GetLoadGroup(aPresContext, getter_AddRefs(loadGroup));

    nsCOMPtr<nsIURI> baseURI;
    GetBaseURI(getter_AddRefs(baseURI));

    nsCOMPtr<nsIURI> imgURI;
    NS_NewURI(getter_AddRefs(imgURI), myList->mListStyleImage, baseURI);

    if (!mListener) {
      nsBulletListener *listener;
      NS_NEWXPCOM(listener, nsBulletListener);
      NS_ADDREF(listener);
      listener->SetFrame(this);
      listener->QueryInterface(NS_GET_IID(imgIDecoderObserver), getter_AddRefs(mListener));
      NS_ASSERTION(mListener, "queryinterface for the listener failed");
      NS_RELEASE(listener);
    }

    il->LoadImage(imgURI, loadGroup, mListener, aPresContext, nsIRequest::LOAD_NORMAL, nsnull, nsnull, getter_AddRefs(mImageRequest));
  }

  return NS_OK;
}

#ifdef NS_DEBUG
NS_IMETHODIMP
nsBulletFrame::GetFrameName(nsString& aResult) const
{
  return MakeFrameName("Bullet", aResult);
}
#endif

NS_IMETHODIMP
nsBulletFrame::GetFrameType(nsIAtom** aType) const
{
  NS_PRECONDITION(nsnull != aType, "null OUT parameter pointer");
  *aType = nsLayoutAtoms::bulletFrame; 
  NS_ADDREF(*aType);
  return NS_OK;
}

#include "nsIDOMNode.h"
NS_IMETHODIMP
nsBulletFrame::Paint(nsIPresContext*      aPresContext,
                     nsIRenderingContext& aRenderingContext,
                     const nsRect&        aDirtyRect,
                     nsFramePaintLayer    aWhichLayer,
                     PRUint32             aFlags)
{
  if (NS_FRAME_PAINT_LAYER_FOREGROUND != aWhichLayer) {
    return NS_OK;
  }

  PRBool isVisible;
  if (NS_SUCCEEDED(IsVisibleForPainting(aPresContext, aRenderingContext, PR_TRUE, &isVisible)) && isVisible) {
    const nsStyleList* myList = (const nsStyleList*)mStyleContext->GetStyleData(eStyleStruct_List);
    PRUint8 listStyleType = myList->mListStyleType;

    if (myList->mListStyleImage.Length() > 0 && mImageRequest) {
      PRUint32 status;
      mImageRequest->GetImageStatus(&status);
      if (status & imgIRequest::STATUS_LOAD_COMPLETE) {
        nsCOMPtr<imgIContainer> imageCon;
        mImageRequest->GetImage(getter_AddRefs(imageCon));
        if (imageCon) {
          nsRect innerArea(mPadding.left, mPadding.top,
                           mRect.width - (mPadding.left + mPadding.right),
                           mRect.height - (mPadding.top + mPadding.bottom));
          nsPoint p(innerArea.x, innerArea.y);
          innerArea.x = innerArea.y = 0;
          aRenderingContext.DrawImage(imageCon, &innerArea, &p);
          return NS_OK;
        }
      }
    }

    const nsStyleFont* myFont = (const nsStyleFont*)mStyleContext->GetStyleData(eStyleStruct_Font);
    const nsStyleColor* myColor = (const nsStyleColor*)mStyleContext->GetStyleData(eStyleStruct_Color);

    nsCOMPtr<nsIFontMetrics> fm;
    aRenderingContext.SetColor(myColor->mColor);

#ifdef IBMBIDI
    nsCharType charType = eCharType_LeftToRight;
    PRUint8 level = 0;
    PRBool isBidiSystem = PR_FALSE;
    const nsStyleVisibility* vis = (const nsStyleVisibility*)mStyleContext->GetStyleData(eStyleStruct_Visibility);
    PRUint32 hints = 0;
#endif // IBMBIDI

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
    case NS_STYLE_LIST_STYLE_OLD_DECIMAL:
    case NS_STYLE_LIST_STYLE_DECIMAL_LEADING_ZERO:
#ifdef IBMBIDI
      GetListItemText(aPresContext, *myList, text);
      charType = eCharType_EuropeanNumber;
      break;

    case NS_STYLE_LIST_STYLE_MOZ_ARABIC_INDIC:
      GetListItemText(aPresContext, *myList, text);
      charType = eCharType_ArabicNumber;
      break;

    case NS_STYLE_LIST_STYLE_HEBREW:
      aRenderingContext.GetHints(hints);
      isBidiSystem = (hints & NS_RENDERING_HINT_BIDI_REORDERING);
      if (!isBidiSystem) {
        charType = eCharType_RightToLeft;
        level = 1;
        GetListItemText(aPresContext, *myList, text);

        if (NS_STYLE_DIRECTION_RTL == vis->mDirection) {
          nsStr::Delete(text, 0, 1);
          text.AppendWithConversion(".");
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
      aPresContext->GetMetricsFor(myFont->mFont, getter_AddRefs(fm));
      GetListItemText(aPresContext, *myList, text);
      aRenderingContext.SetFont(fm);
      nscoord ascent;
      fm->GetMaxAscent(ascent);
      aRenderingContext.DrawString(text, mPadding.left, mPadding.top + ascent);
      break;
    }
#ifdef IBMBIDI
    if (charType != eCharType_LeftToRight) {
      aPresContext->GetMetricsFor(myFont->mFont, getter_AddRefs(fm));
      aRenderingContext.SetFont(fm);
      nscoord ascent;
      fm->GetMaxAscent(ascent);

      nsBidiPresUtils* bidiUtils;
      aPresContext->GetBidiUtils(&bidiUtils);
      if (bidiUtils) {
        const PRUnichar* buffer = text.get();
        PRInt32 textLength = text.Length();
        if (eCharType_RightToLeft == charType) {
          bidiUtils->FormatUnicodeText(aPresContext, (PRUnichar*)buffer, textLength,
                                       charType, level, PR_FALSE);
        }
        else {
//Mohamed
          aRenderingContext.GetHints(hints);
          isBidiSystem = (hints & NS_RENDERING_HINT_ARABIC_SHAPING);
          bidiUtils->FormatUnicodeText(aPresContext, (PRUnichar*)buffer, textLength,
                                       charType, level, isBidiSystem);//Mohamed
        }
      }
      aRenderingContext.DrawString(text, mPadding.left, mPadding.top + ascent);
    }   
#endif // IBMBIDI
  }
  DO_GLOBAL_REFLOW_COUNT_DSP("nsBulletFrame", &aRenderingContext);
  return NS_OK;
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
  nsHTMLValue value;
  nsCOMPtr<nsIContent> parentContent;
  mParent->GetContent(getter_AddRefs(parentContent));
  if (parentContent) {
    nsCOMPtr<nsIHTMLContent> hc = do_QueryInterface(parentContent);
    if (hc) {
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
    }
  }

  *aChanged = oldOrdinal != mOrdinal;

  return mOrdinal + 1;
}


// XXX change roman/alpha to use unsigned math so that maxint and
// maxnegint will work


static void DecimalToText(PRInt32 ordinal, nsString& result)
{
   char cbuf[40];
   PR_snprintf(cbuf, sizeof(cbuf), "%ld", ordinal);
   result.AppendWithConversion(cbuf);
}
static void DecimalLeadingZeroToText(PRInt32 ordinal, nsString& result)
{
   char cbuf[40];
   PR_snprintf(cbuf, sizeof(cbuf), "%02ld", ordinal);
   result.AppendWithConversion(cbuf);
}
static void OtherDecimalToText(PRInt32 ordinal, PRUnichar zeroChar, nsString& result)
{
   PRUnichar diff = zeroChar - PRUnichar('0');
   DecimalToText(ordinal, result); 
   PRUnichar* p = (PRUnichar*)result.get();
   for(; nsnull != *p ; p++) 
      *p += diff;
}
static void TamilToText(PRInt32 ordinal,  nsString& result)
{
   PRUnichar diff = 0x0BE6 - PRUnichar('0');
   DecimalToText(ordinal, result); 
   PRUnichar* p = (PRUnichar*)result.get();
   for(; nsnull != *p ; p++) 
      if(*p != PRUnichar('0'))
         *p += diff;
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
      case '3':  addOn.AppendWithConversion(achars[romanPos]);
      case '2':  addOn.AppendWithConversion(achars[romanPos]);
      case '1':  addOn.AppendWithConversion(achars[romanPos]);
        break;
      case '4':
        addOn.AppendWithConversion(achars[romanPos]);
        // FALLTHROUGH
      case '5': case '6':
      case '7': case  '8':
        addOn.AppendWithConversion(bchars[romanPos]);
        for(n=0;n<(*dp-'5');n++) {
          addOn.AppendWithConversion(achars[romanPos]);
        }
        break;
      case '9':
        addOn.AppendWithConversion(achars[romanPos]);
        addOn.AppendWithConversion(achars[romanPos+1]);
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

#define CJK_HEAVENLY_STEM_CHARS_SIZE 10 
static PRUnichar gCJKHeavenlyStemChars[CJK_HEAVENLY_STEM_CHARS_SIZE] =
{
0x7532, 0x4e59, 0x4e19, 0x4e01, 0x620a,
0x5df1, 0x5e9a, 0x8f9b, 0x58ec, 0x7678
};
#define CJK_EARTHLY_BRANCH_CHARS_SIZE 12 
static PRUnichar gCJKEarthlyBranchChars[CJK_EARTHLY_BRANCH_CHARS_SIZE] =
{
0x5b50, 0x4e11, 0x5bc5, 0x536f, 0x8fb0, 0x5df3,
0x5348, 0x672a, 0x7533, 0x9149, 0x620c, 0x4ea5
};
#define HANGUL_CHARS_SIZE 14 
static PRUnichar gHangulChars[HANGUL_CHARS_SIZE] =
{
0xac00, 0xb098, 0xb2e4, 0xb77c, 0xb9c8, 0xbc14,
0xc0ac, 0xc544, 0xc790, 0xcc28, 0xce74, 0xd0c0,
0xd30c, 0xd558
};
#define HANGUL_CONSONANT_CHARS_SIZE 14 
static PRUnichar gHangulConsonantChars[HANGUL_CONSONANT_CHARS_SIZE] =
{                                      
0x3131, 0x3134, 0x3137, 0x3139, 0x3141, 0x3142,
0x3145, 0x3147, 0x3148, 0x314a, 0x314b, 0x314c,
0x314d, 0x314e
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


static PRUnichar gCJKIdeographicDigit1[10] =
{
  0x96f6, 0x4e00, 0x4e8c, 0x4e09, 0x56db,  // 0 - 4
  0x4e94, 0x516d, 0x4e03, 0x516b, 0x4e5d   // 5 - 9
};
static PRUnichar gCJKIdeographicDigit2[10] =
{
  0x96f6, 0x58f9, 0x8cb3, 0x53c3, 0x8086,  // 0 - 4
  0x4f0d, 0x9678, 0x67d2, 0x634c, 0x7396   // 5 - 9
};
static PRUnichar gCJKIdeographicDigit3[10] =
{
  0x96f6, 0x58f9, 0x8d30, 0x53c1, 0x8086,  // 0 - 4
  0x4f0d, 0x9646, 0x67d2, 0x634c, 0x7396   // 5 - 9
};
static PRUnichar gCJKIdeographicUnit1[4] =
{
  0x000, 0x5341, 0x767e, 0x5343
};
static PRUnichar gCJKIdeographicUnit2[4] =
{
  0x000, 0x62FE, 0x4F70, 0x4EDF
};
static PRUnichar gCJKIdeographic10KUnit1[4] =
{
  0x000, 0x842c, 0x5104, 0x5146
};
static PRUnichar gCJKIdeographic10KUnit2[4] =
{
  0x000, 0x4E07, 0x4ebf, 0x5146
};
static PRUnichar gCJKIdeographic10KUnit3[4] =
{
  0x000, 0x4E07, 0x5104, 0x5146
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
      buf[idx++] = HEBREW_THROSAND_SEP; // output thousand seperator
#else
      buf[--idx] = HEBREW_THROSAND_SEP; // output thousand seperator
#endif // IBMBIDI
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
#ifdef IBMBIDI
          buf[idx++] = digit;
#else
          buf[--idx] = digit;
#endif // IBMBIDI
          d++;
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
        d++;
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
nsBulletFrame::GetListItemText(nsIPresContext* aCX,
                               const nsStyleList& aListStyle,
                               nsString& result)
{
#ifdef IBMBIDI
  const nsStyleVisibility* vis;
  GetStyleData(eStyleStruct_Visibility, (const nsStyleStruct*&)vis);

  if (NS_STYLE_DIRECTION_RTL == vis->mDirection) {
    result.AppendWithConversion(".");
  }
#endif // IBMBIDI

  switch (aListStyle.mListStyleType) {
    case NS_STYLE_LIST_STYLE_DECIMAL:
    case NS_STYLE_LIST_STYLE_OLD_DECIMAL:
    default: // CSS2 say "A users  agent that does not recognize a numbering system
      // should use 'decimal'
      DecimalToText(mOrdinal, result);
      break;

    case NS_STYLE_LIST_STYLE_DECIMAL_LEADING_ZERO:
      DecimalLeadingZeroToText(mOrdinal, result);
      break;

    case NS_STYLE_LIST_STYLE_LOWER_ROMAN:
    case NS_STYLE_LIST_STYLE_OLD_LOWER_ROMAN:
      RomanToText(mOrdinal, result, gLowerRomanCharsA, gLowerRomanCharsB);
      break;
    case NS_STYLE_LIST_STYLE_UPPER_ROMAN:
    case NS_STYLE_LIST_STYLE_OLD_UPPER_ROMAN:
      RomanToText(mOrdinal, result, gUpperRomanCharsA, gUpperRomanCharsB);
      break;

    case NS_STYLE_LIST_STYLE_LOWER_ALPHA:
    case NS_STYLE_LIST_STYLE_OLD_LOWER_ALPHA:
      CharListToText(mOrdinal, result, gLowerAlphaChars, ALPHA_SIZE);
      break;

    case NS_STYLE_LIST_STYLE_UPPER_ALPHA:
    case NS_STYLE_LIST_STYLE_OLD_UPPER_ALPHA:
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
    case NS_STYLE_LIST_STYLE_MOZ_TRAD_CHINESE_INFORMAL: 
      CJKIdeographicToText(mOrdinal, result, gCJKIdeographicDigit1, gCJKIdeographicUnit1, gCJKIdeographic10KUnit1);
      break;

    case NS_STYLE_LIST_STYLE_MOZ_TRAD_CHINESE_FORMAL: 
      CJKIdeographicToText(mOrdinal, result, gCJKIdeographicDigit2, gCJKIdeographicUnit2, gCJKIdeographic10KUnit1);
      break;

    case NS_STYLE_LIST_STYLE_MOZ_SIMP_CHINESE_INFORMAL: 
      CJKIdeographicToText(mOrdinal, result, gCJKIdeographicDigit1, gCJKIdeographicUnit1, gCJKIdeographic10KUnit2);
      break;

    case NS_STYLE_LIST_STYLE_MOZ_SIMP_CHINESE_FORMAL: 
      CJKIdeographicToText(mOrdinal, result, gCJKIdeographicDigit3, gCJKIdeographicUnit2, gCJKIdeographic10KUnit2);
      break;

    case NS_STYLE_LIST_STYLE_MOZ_JAPANESE_INFORMAL: 
      CJKIdeographicToText(mOrdinal, result, gCJKIdeographicDigit1, gCJKIdeographicUnit1, gCJKIdeographic10KUnit3);
      break;

    case NS_STYLE_LIST_STYLE_MOZ_JAPANESE_FORMAL: 
      CJKIdeographicToText(mOrdinal, result, gCJKIdeographicDigit2, gCJKIdeographicUnit2, gCJKIdeographic10KUnit3);
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
 
    case NS_STYLE_LIST_STYLE_MOZ_ARABIC_INDIC:
      OtherDecimalToText(mOrdinal, 0x0660, result);
      break;
 
    case NS_STYLE_LIST_STYLE_MOZ_PERSIAN:
    case NS_STYLE_LIST_STYLE_MOZ_URDU:
      OtherDecimalToText(mOrdinal, 0x06f0, result);
      break;
 
    case NS_STYLE_LIST_STYLE_MOZ_DEVANAGARI:
      OtherDecimalToText(mOrdinal, 0x0966, result);
      break;
 
    case NS_STYLE_LIST_STYLE_MOZ_GURMUKHI:
      OtherDecimalToText(mOrdinal, 0x0a66, result);
      break;
 
    case NS_STYLE_LIST_STYLE_MOZ_GUJARATI:
      OtherDecimalToText(mOrdinal, 0x0AE6, result);
      break;
 
    case NS_STYLE_LIST_STYLE_MOZ_ORIYA:
      OtherDecimalToText(mOrdinal, 0x0B66, result);
      break;
 
    case NS_STYLE_LIST_STYLE_MOZ_KANNADA:
      OtherDecimalToText(mOrdinal, 0x0CE6, result);
      break;
 
    case NS_STYLE_LIST_STYLE_MOZ_MALAYALAM:
      OtherDecimalToText(mOrdinal, 0x0D66, result);
      break;
 
    case NS_STYLE_LIST_STYLE_MOZ_THAI:
      OtherDecimalToText(mOrdinal, 0x0E50, result);
      break;
 
    case NS_STYLE_LIST_STYLE_MOZ_LAO:
      OtherDecimalToText(mOrdinal, 0x0ED0, result);
      break;
 
    case NS_STYLE_LIST_STYLE_MOZ_MYANMAR:
      OtherDecimalToText(mOrdinal, 0x1040, result);
      break;
 
    case NS_STYLE_LIST_STYLE_MOZ_KHMER:
      OtherDecimalToText(mOrdinal, 0x17E0, result);
      break;
 
    case NS_STYLE_LIST_STYLE_MOZ_BENGALI:
      OtherDecimalToText(mOrdinal, 0x09E6, result);
      break;
 
    case NS_STYLE_LIST_STYLE_MOZ_TELUGU:
      OtherDecimalToText(mOrdinal, 0x0C66, result);
      break;
 
    case NS_STYLE_LIST_STYLE_MOZ_TAMIL:
      TamilToText(mOrdinal, result);
      break;

    case NS_STYLE_LIST_STYLE_MOZ_CJK_HEAVENLY_STEM:
      CharListToText(mOrdinal, result, gCJKHeavenlyStemChars, CJK_HEAVENLY_STEM_CHARS_SIZE);
      break;

    case NS_STYLE_LIST_STYLE_MOZ_CJK_EARTHLY_BRANCH:
      CharListToText(mOrdinal, result, gCJKEarthlyBranchChars, CJK_EARTHLY_BRANCH_CHARS_SIZE);
      break;

    case NS_STYLE_LIST_STYLE_MOZ_HANGUL:
      CharListToText(mOrdinal, result, gHangulChars, HANGUL_CHARS_SIZE);
      break;

    case NS_STYLE_LIST_STYLE_MOZ_HANGUL_CONSONANT:
      CharListToText(mOrdinal, result, gHangulConsonantChars, HANGUL_CONSONANT_CHARS_SIZE);
      break;
  }
#ifdef IBMBIDI
  if (NS_STYLE_DIRECTION_RTL != vis->mDirection)
#endif // IBMBIDI
  result.AppendWithConversion(".");
}

#define MIN_BULLET_SIZE 5               // from laytext.c


#define MINMAX(_value,_min,_max) \
    ((_value) < (_min)           \
     ? (_min)                    \
     : ((_value) > (_max)        \
        ? (_max)                 \
        : (_value)))


void
nsBulletFrame::GetDesiredSize(nsIPresContext*  aCX,
                              const nsHTMLReflowState& aReflowState,
                              nsHTMLReflowMetrics& aMetrics)
{
  const nsStyleList* myList = (const nsStyleList*)mStyleContext->GetStyleData(eStyleStruct_List);
  nscoord ascent;

  if (!myList->mListStyleImage.IsEmpty()) {
    nscoord widthConstraint = NS_INTRINSICSIZE;
    nscoord heightConstraint = NS_INTRINSICSIZE;
    PRBool fixedContentWidth = PR_FALSE;
    PRBool fixedContentHeight = PR_FALSE;

    nscoord minWidth, maxWidth, minHeight, maxHeight;

    // Determine whether the image has fixed content width
    widthConstraint = aReflowState.mComputedWidth;
    minWidth = aReflowState.mComputedMinWidth;
    maxWidth = aReflowState.mComputedMaxWidth;
    if (widthConstraint != NS_INTRINSICSIZE) {
      fixedContentWidth = PR_TRUE;
    }

    // Determine whether the image has fixed content height
    heightConstraint = aReflowState.mComputedHeight;
    minHeight = aReflowState.mComputedMinHeight;
    maxHeight = aReflowState.mComputedMaxHeight;
    if (heightConstraint != NS_UNCONSTRAINEDSIZE) {
      fixedContentHeight = PR_TRUE;
    }

    PRBool haveComputedSize = PR_FALSE;
    PRBool needIntrinsicImageSize = PR_FALSE;

    nscoord newWidth=0, newHeight=0;
    if (fixedContentWidth) {
      newWidth = MINMAX(widthConstraint, minWidth, maxWidth);
      if (fixedContentHeight) {
        newHeight = MINMAX(heightConstraint, minHeight, maxHeight);
        haveComputedSize = PR_TRUE;
      } else {
        // We have a width, and an auto height. Compute height from
        // width once we have the intrinsic image size.
        if (mIntrinsicSize.height != 0) {
          newHeight = (mIntrinsicSize.height * newWidth) / mIntrinsicSize.width;
          haveComputedSize = PR_TRUE;
        } else {
          newHeight = 0;
          needIntrinsicImageSize = PR_TRUE;
        }
      }
    } else if (fixedContentHeight) {
      // We have a height, and an auto width. Compute width from height
      // once we have the intrinsic image size.
      newHeight = MINMAX(heightConstraint, minHeight, maxHeight);
      if (mIntrinsicSize.width != 0) {
        newWidth = (mIntrinsicSize.width * newHeight) / mIntrinsicSize.height;
        haveComputedSize = PR_TRUE;
      } else {
        newWidth = 0;
        needIntrinsicImageSize = PR_TRUE;
      }
    } else {
      // auto size the image
      if (mIntrinsicSize.width == 0 && mIntrinsicSize.height == 0)
        needIntrinsicImageSize = PR_TRUE;
      else
        haveComputedSize = PR_TRUE;

      newWidth = mIntrinsicSize.width;
      newHeight = mIntrinsicSize.height;
    }

    mComputedSize.width = newWidth;
    mComputedSize.height = newHeight;

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
    aMetrics.height = mComputedSize.height;

#if 0
    mImageLoader.GetDesiredSize(aCX, &aReflowState, aMetrics);
    if (!mImageLoader.GetLoadImageFailed()) {
      nsHTMLContainerFrame::CreateViewForFrame(aCX, this, mStyleContext, nsnull,
                                               PR_FALSE);
      aMetrics.ascent = aMetrics.height;
      aMetrics.descent = 0;
      return;
    }

#endif

    aMetrics.ascent = aMetrics.height;
    aMetrics.descent = 0;

    return;

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
      GetListItemText(aCX, *myList, text);
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
nsBulletFrame::Reflow(nsIPresContext* aPresContext,
                      nsHTMLReflowMetrics& aMetrics,
                      const nsHTMLReflowState& aReflowState,
                      nsReflowStatus& aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsBulletFrame", aReflowState.reason);
  DISPLAY_REFLOW(this, aReflowState, aMetrics, aStatus);
  if (eReflowReason_Incremental == aReflowState.reason) {
    nsIReflowCommand::ReflowType type;
    aReflowState.reflowCommand->GetType(type);

    /* if the style changed, see if we need to load a new url */
    if (nsIReflowCommand::StyleChanged == type) {
      nsCOMPtr<nsIURI> baseURI;
      GetBaseURI(getter_AddRefs(baseURI));

      const nsStyleList* myList = (const nsStyleList*)mStyleContext->GetStyleData(eStyleStruct_List);

      if (!myList->mListStyleImage.IsEmpty()) {

        if (!mListener) {
          nsBulletListener *listener;
          NS_NEWXPCOM(listener, nsBulletListener);
          NS_ADDREF(listener);
          listener->SetFrame(this);
          listener->QueryInterface(NS_GET_IID(imgIDecoderObserver), getter_AddRefs(mListener));
          NS_ASSERTION(mListener, "queryinterface for the listener failed");
          NS_RELEASE(listener);
        }


        nsCOMPtr<nsIURI> newURI;
        NS_NewURI(getter_AddRefs(newURI), myList->mListStyleImage, baseURI);

        PRBool needNewRequest = PR_TRUE;

        if (mImageRequest) {
          // Reload the image, maybe...
          nsCOMPtr<nsIURI> oldURI;
          mImageRequest->GetURI(getter_AddRefs(oldURI));
          if (oldURI) {
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
          nsresult rv;
          nsCOMPtr<imgILoader> il(do_GetService("@mozilla.org/image/loader;1", &rv));
          if (NS_FAILED(rv))
            return rv;

          nsCOMPtr<nsILoadGroup> loadGroup;
          GetLoadGroup(aPresContext, getter_AddRefs(loadGroup));

          il->LoadImage(newURI, loadGroup, mListener, aPresContext, nsIRequest::LOAD_NORMAL, nsnull, nsnull, getter_AddRefs(mImageRequest));
        }
      }
    }
  }

  // Get the base size
  GetDesiredSize(aPresContext, aReflowState, aMetrics);

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


NS_IMETHODIMP nsBulletFrame::OnStartContainer(imgIRequest *aRequest, nsIPresContext *aPresContext, imgIContainer *aImage)
{
  if (!aImage) return NS_ERROR_INVALID_ARG;

  nscoord w, h;
  aImage->GetWidth(&w);
  aImage->GetHeight(&h);

  float p2t;
  aPresContext->GetPixelsToTwips(&p2t);

  nsSize newsize(NSIntPixelsToTwips(w, p2t), NSIntPixelsToTwips(h, p2t));

  if (mIntrinsicSize != newsize) {
    mIntrinsicSize = newsize;

    // Now that the size is available (or an error occurred), trigger
    // a reflow of the bullet frame.
    nsCOMPtr<nsIPresShell> shell;
    nsresult rv = aPresContext->GetShell(getter_AddRefs(shell));
    if (NS_SUCCEEDED(rv) && shell) {
      NS_ASSERTION(mParent, "No parent to pass the reflow request up to.");
      if (mParent) {
        // Reflow the first child of the parent not the bullet frame.
        // The bullet frame is not in a line list so marking it dirty
        // has no effect. The reflowing of the bullet frame is done 
        // indirectly.
        nsIFrame* frame = nsnull;
        mParent->FirstChild(aPresContext, nsnull, &frame);
        NS_ASSERTION(frame, "No frame to mark dirty for bullet frame.");
        if (frame) {
          nsFrameState state;
          frame->GetFrameState(&state);
          state |= NS_FRAME_IS_DIRTY;
          frame->SetFrameState(state);
          mParent->ReflowDirtyChild(shell, frame);
        }
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP nsBulletFrame::OnDataAvailable(imgIRequest *aRequest, nsIPresContext *aPresContext, gfxIImageFrame *aFrame, const nsRect *aRect)
{
  if (!aRect) return NS_ERROR_NULL_POINTER;

  nsRect r(*aRect);

  /* XXX Why do we subtract 1 here?  The rect is (for example): (0, 0, 600, 1)..
         Why do we have to make y -1?
   */

  // The y coordinate of aRect is passed as a scanline where the first scanline is given
  // a value of 1. We need to convert this to the nsFrames coordinate space by subtracting
  // 1.
  r.y -= 1;

  float p2t;
  aPresContext->GetPixelsToTwips(&p2t);
  r.x = NSIntPixelsToTwips(r.x, p2t);
  r.y = NSIntPixelsToTwips(r.y, p2t);
  r.width = NSIntPixelsToTwips(r.width, p2t);
  r.height = NSIntPixelsToTwips(r.height, p2t);

  Invalidate(aPresContext, r, PR_FALSE);

  return NS_OK;
}

NS_IMETHODIMP nsBulletFrame::OnStopDecode(imgIRequest *aRequest, nsIPresContext *aPresContext, nsresult aStatus, const PRUnichar *aStatusArg)
{
  // XXX should the bulletframe do anything if the image failed to load?
  //     it didn't in the old code...
#if 0
  nsCOMPtr<nsIPresShell> presShell;
  aPresContext->GetShell(getter_AddRefs(presShell));


  if (NS_FAILED(aStatus)) {
    // We failed to load the image. Notify the pres shell
    if (NS_FAILED(aStatus) && (mImageRequest == aRequest || !mImageRequest)) {
      imageFailed = PR_TRUE;
    }
  }
#endif

  return NS_OK;
}

NS_IMETHODIMP nsBulletFrame::FrameChanged(imgIContainer *aContainer, nsIPresContext *aPresContext, gfxIImageFrame *aNewFrame, nsRect *aDirtyRect)
{
  nsRect r(*aDirtyRect);

  float p2t;
  aPresContext->GetPixelsToTwips(&p2t);
  r.x = NSIntPixelsToTwips(r.x, p2t);
  r.y = NSIntPixelsToTwips(r.y, p2t);
  r.width = NSIntPixelsToTwips(r.width, p2t);
  r.height = NSIntPixelsToTwips(r.height, p2t);

  Invalidate(aPresContext, r, PR_FALSE);

  return NS_OK;
}



void
nsBulletFrame::GetBaseURI(nsIURI **aURI)
{
  NS_PRECONDITION(nsnull != aURI, "null OUT parameter pointer");

  nsresult rv;
  nsCOMPtr<nsIURI> baseURI;
  nsCOMPtr<nsIHTMLContent> htmlContent(do_QueryInterface(mContent, &rv));
  if (NS_SUCCEEDED(rv)) {
    htmlContent->GetBaseURL(*getter_AddRefs(baseURI));
  }
  else {
    nsCOMPtr<nsIDocument> doc;
    mContent->GetDocument(*getter_AddRefs(doc));
    if (doc) {
      doc->GetBaseURL(*getter_AddRefs(baseURI));
    }
  }
  *aURI = baseURI;
  NS_IF_ADDREF(*aURI);
}

void
nsBulletFrame::GetLoadGroup(nsIPresContext *aPresContext, nsILoadGroup **aLoadGroup)
{
  if (!aPresContext)
    return;

  NS_PRECONDITION(nsnull != aLoadGroup, "null OUT parameter pointer");

  nsCOMPtr<nsIPresShell> shell;
  aPresContext->GetShell(getter_AddRefs(shell));

  if (!shell)
    return;

  nsCOMPtr<nsIDocument> doc;
  shell->GetDocument(getter_AddRefs(doc));
  if (!doc)
    return;

  doc->GetDocumentLoadGroup(aLoadGroup);
}








NS_IMPL_ISUPPORTS2(nsBulletListener, imgIDecoderObserver, imgIContainerObserver)

nsBulletListener::nsBulletListener() :
  mFrame(nsnull)
{
  NS_INIT_ISUPPORTS();
}

nsBulletListener::~nsBulletListener()
{
}

NS_IMETHODIMP nsBulletListener::OnStartDecode(imgIRequest *aRequest, nsISupports *aContext)
{
  return NS_OK;
}

NS_IMETHODIMP nsBulletListener::OnStartContainer(imgIRequest *aRequest, nsISupports *aContext, imgIContainer *aImage)
{
  if (!mFrame)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIPresContext> pc(do_QueryInterface(aContext));

  NS_ASSERTION(pc, "not a pres context!");

  return mFrame->OnStartContainer(aRequest, pc, aImage);
}

NS_IMETHODIMP nsBulletListener::OnStartFrame(imgIRequest *aRequest, nsISupports *aContext, gfxIImageFrame *aFrame)
{
  return NS_OK;
}

NS_IMETHODIMP nsBulletListener::OnDataAvailable(imgIRequest *aRequest, nsISupports *aContext, gfxIImageFrame *aFrame, const nsRect *aRect)
{
  if (!mFrame)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIPresContext> pc(do_QueryInterface(aContext));

  NS_ASSERTION(pc, "not a pres context!");

  return mFrame->OnDataAvailable(aRequest, pc, aFrame, aRect);
}

NS_IMETHODIMP nsBulletListener::OnStopFrame(imgIRequest *aRequest, nsISupports *aContext, gfxIImageFrame *aFrame)
{
  return NS_OK;
}

NS_IMETHODIMP nsBulletListener::OnStopContainer(imgIRequest *aRequest, nsISupports *aContext, imgIContainer *aImage)
{
  return NS_OK;
}

NS_IMETHODIMP nsBulletListener::OnStopDecode(imgIRequest *aRequest, nsISupports *aContext, nsresult status, const PRUnichar *statusArg)
{
  if (!mFrame)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIPresContext> pc(do_QueryInterface(aContext));

  NS_ASSERTION(pc, "not a pres context!");

  return mFrame->OnStopDecode(aRequest, pc, status, statusArg);
}

NS_IMETHODIMP nsBulletListener::FrameChanged(imgIContainer *aContainer, nsISupports *aContext, gfxIImageFrame *newframe, nsRect * dirtyRect)
{
  if (!mFrame)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIPresContext> pc(do_QueryInterface(aContext));

  NS_ASSERTION(pc, "not a pres context!");

  return mFrame->FrameChanged(aContainer, pc, newframe, dirtyRect);
}
