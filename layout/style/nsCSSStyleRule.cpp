/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#include "nsICSSStyleRule.h"
#include "nsICSSDeclaration.h"
#include "nsIStyleContext.h"
#include "nsIPresContext.h"
#include "nsIArena.h"
#include "nsIAtom.h"
#include "nsCRT.h"
#include "nsString.h"
#include "nsStyleConsts.h"
#include "nsHTMLAtoms.h"
#include "nsUnitConversion.h"

//#define DEBUG_REFS

static NS_DEFINE_IID(kIStyleRuleIID, NS_ISTYLE_RULE_IID);
static NS_DEFINE_IID(kICSSDeclarationIID, NS_ICSS_DECLARATION_IID);
static NS_DEFINE_IID(kICSSStyleRuleIID, NS_ICSS_STYLE_RULE_IID);

static NS_DEFINE_IID(kStyleColorSID, NS_STYLECOLOR_SID);
static NS_DEFINE_IID(kStyleDisplaySID, NS_STYLEDISPLAY_SID);
static NS_DEFINE_IID(kStyleFontSID, NS_STYLEFONT_SID);
static NS_DEFINE_IID(kStyleListSID, NS_STYLELIST_SID);
static NS_DEFINE_IID(kStylePositionSID, NS_STYLEPOSITION_SID);
static NS_DEFINE_IID(kStyleSpacingSID, NS_STYLESPACING_SID);
static NS_DEFINE_IID(kStyleTextSID, NS_STYLETEXT_SID);

static NS_DEFINE_IID(kCSSFontSID, NS_CSS_FONT_SID);
static NS_DEFINE_IID(kCSSColorSID, NS_CSS_COLOR_SID);
static NS_DEFINE_IID(kCSSTextSID, NS_CSS_TEXT_SID);
static NS_DEFINE_IID(kCSSMarginSID, NS_CSS_MARGIN_SID);
static NS_DEFINE_IID(kCSSPositionSID, NS_CSS_POSITION_SID);
static NS_DEFINE_IID(kCSSListSID, NS_CSS_LIST_SID);
static NS_DEFINE_IID(kCSSDisplaySID, NS_CSS_DISPLAY_SID);


// -- nsCSSSelector -------------------------------

nsCSSSelector::nsCSSSelector()
  : mTag(nsnull), mID(nsnull), mClass(nsnull), mPseudoClass(nsnull),
    mNext(nsnull)
{
}

nsCSSSelector::nsCSSSelector(nsIAtom* aTag, nsIAtom* aID, nsIAtom* aClass, nsIAtom* aPseudoClass)
  : mTag(aTag), mID(aID), mClass(aClass), mPseudoClass(aPseudoClass),
    mNext(nsnull)
{
  NS_IF_ADDREF(mTag);
  NS_IF_ADDREF(mID);
  NS_IF_ADDREF(mClass);
  NS_IF_ADDREF(mPseudoClass);
}

nsCSSSelector::nsCSSSelector(const nsCSSSelector& aCopy) 
  : mTag(aCopy.mTag), mID(aCopy.mID), mClass(aCopy.mClass), mPseudoClass(aCopy.mPseudoClass),
    mNext(nsnull)
{ // implmented to support extension to CSS2 (when we have to copy the array)
  NS_IF_ADDREF(mTag);
  NS_IF_ADDREF(mID);
  NS_IF_ADDREF(mClass);
  NS_IF_ADDREF(mPseudoClass);
}

nsCSSSelector::~nsCSSSelector()  
{  
  NS_IF_RELEASE(mTag);
  NS_IF_RELEASE(mID);
  NS_IF_RELEASE(mClass);
  NS_IF_RELEASE(mPseudoClass);
}

nsCSSSelector& nsCSSSelector::operator=(const nsCSSSelector& aCopy)
{
  NS_IF_RELEASE(mTag);
  NS_IF_RELEASE(mID);
  NS_IF_RELEASE(mClass);
  NS_IF_RELEASE(mPseudoClass);
  mTag          = aCopy.mTag;
  mID           = aCopy.mID;
  mClass        = aCopy.mClass;
  mPseudoClass  = aCopy.mPseudoClass;
  NS_IF_ADDREF(mTag);
  NS_IF_ADDREF(mID);
  NS_IF_ADDREF(mClass);
  NS_IF_ADDREF(mPseudoClass);
  return *this;
}

PRBool nsCSSSelector::Equals(const nsCSSSelector* aOther) const
{
  if (nsnull != aOther) {
    return (PRBool)((aOther->mTag == mTag) && (aOther->mID == mID) && 
                    (aOther->mClass == mClass) && (aOther->mPseudoClass == mPseudoClass));
  }
  return PR_FALSE;
}


void nsCSSSelector::Set(const nsString& aTag, const nsString& aID, 
                        const nsString& aClass, const nsString& aPseudoClass)
{
  nsAutoString  buffer;
  NS_IF_RELEASE(mTag);
  NS_IF_RELEASE(mID);
  NS_IF_RELEASE(mClass);
  NS_IF_RELEASE(mPseudoClass);
  if (0 < aTag.Length()) {
    aTag.ToUpperCase(buffer);    // XXX is this correct? what about class?
    mTag = NS_NewAtom(buffer);
  }
  if (0 < aID.Length()) {
    mID = NS_NewAtom(aID);
  }
  if (0 < aClass.Length()) {
    mClass = NS_NewAtom(aClass);
  }
  if (0 < aPseudoClass.Length()) {
    aPseudoClass.ToUpperCase(buffer);
    mPseudoClass = NS_NewAtom(buffer);
    if (nsnull == mTag) {
      mTag = nsHTMLAtoms::a;
      NS_ADDREF(mTag);
    }
  }
}

// -- nsCSSStyleRule -------------------------------

class CSSStyleRuleImpl : public nsICSSStyleRule {
public:
  void* operator new(size_t size);
  void* operator new(size_t size, nsIArena* aArena);
  void operator delete(void* ptr);

  CSSStyleRuleImpl(const nsCSSSelector& aSelector);

  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);
  NS_IMETHOD_(nsrefcnt) AddRef();
  NS_IMETHOD_(nsrefcnt) Release();

  virtual PRBool Equals(const nsIStyleRule* aRule) const;
  virtual PRUint32 HashValue(void) const;

  virtual nsCSSSelector* FirstSelector(void);
  virtual void AddSelector(const nsCSSSelector& aSelector);
  virtual void DeleteSelector(nsCSSSelector* aSelector);

  virtual nsICSSDeclaration* GetDeclaration(void) const;
  virtual void SetDeclaration(nsICSSDeclaration* aDeclaration);

  virtual PRInt32 GetWeight(void) const;
  virtual void SetWeight(PRInt32 aWeight);

  virtual nscoord CalcLength(const nsCSSValue& aValue, nsStyleFont* aFont, 
                             nsIPresContext* aPresContext);
  virtual PRBool SetCoord(const nsCSSValue& aValue, nsStyleCoord& aCoord, 
                          PRInt32 aMask, nsStyleFont* aFont, 
                          nsIPresContext* aPresContext);
  virtual void MapStyleInto(nsIStyleContext* aContext, nsIPresContext* aPresContext);

  virtual void List(FILE* out = stdout, PRInt32 aIndent = 0) const;

private: 
  // These are not supported and are not implemented! 
  CSSStyleRuleImpl(const CSSStyleRuleImpl& aCopy); 
  CSSStyleRuleImpl& operator=(const CSSStyleRuleImpl& aCopy); 

protected:
  virtual ~CSSStyleRuleImpl();

protected:
  PRUint32 mInHeap : 1;
  PRUint32 mRefCnt : 31;

  nsCSSSelector   mSelector;
  nsICSSDeclaration* mDeclaration;
  PRInt32         mWeight;
#ifdef DEBUG_REFS
  PRInt32 mInstance;
#endif
};


void* CSSStyleRuleImpl::operator new(size_t size)
{
  CSSStyleRuleImpl* rv = (CSSStyleRuleImpl*) ::operator new(size);
#ifdef NS_DEBUG
  if (nsnull != rv) {
    nsCRT::memset(rv, 0xEE, size);
  }
#endif
  rv->mInHeap = 1;
  return (void*) rv;
}

void* CSSStyleRuleImpl::operator new(size_t size, nsIArena* aArena)
{
  CSSStyleRuleImpl* rv = (CSSStyleRuleImpl*) aArena->Alloc(PRInt32(size));
#ifdef NS_DEBUG
  if (nsnull != rv) {
    nsCRT::memset(rv, 0xEE, size);
  }
#endif
  rv->mInHeap = 0;
  return (void*) rv;
}

void CSSStyleRuleImpl::operator delete(void* ptr)
{
  CSSStyleRuleImpl* rule = (CSSStyleRuleImpl*) ptr;
  if (nsnull != rule) {
    if (rule->mInHeap) {
      ::delete ptr;
    }
  }
}


#ifdef DEBUG_REFS
static PRInt32 gInstanceCount;
static const PRInt32 kInstrument = 1075;
#endif

CSSStyleRuleImpl::CSSStyleRuleImpl(const nsCSSSelector& aSelector)
  : mSelector(aSelector), mDeclaration(nsnull), mWeight(0)
{
  NS_INIT_REFCNT();
#ifdef DEBUG_REFS
  mInstance = gInstanceCount++;
  fprintf(stdout, "%d of %d + CSSStyleRule\n", mInstance, gInstanceCount);
#endif
}

CSSStyleRuleImpl::~CSSStyleRuleImpl()
{
  nsCSSSelector*  next = mSelector.mNext;

  while (nsnull != next) {
    nsCSSSelector*  selector = next;
    next = selector->mNext;
    delete selector;
  }
  NS_IF_RELEASE(mDeclaration);
#ifdef DEBUG_REFS
  --gInstanceCount;
  fprintf(stdout, "%d of %d - CSSStyleRule\n", mInstance, gInstanceCount);
#endif
}

#ifdef DEBUG_REFS
nsrefcnt CSSStyleRuleImpl::AddRef(void)                                
{                                    
  if (mInstance == kInstrument) {
    fprintf(stdout, "%d AddRef CSSStyleRule\n", mRefCnt + 1);
  }
  return ++mRefCnt;                                          
}

nsrefcnt CSSStyleRuleImpl::Release(void)                         
{                                                      
  if (mInstance == kInstrument) {
    fprintf(stdout, "%d Release CSSStyleRule\n", mRefCnt - 1);
  }
  if (--mRefCnt == 0) {                                
    delete this;                                       
    return 0;                                          
  }                                                    
  return mRefCnt;                                      
}
#else
NS_IMPL_ADDREF(CSSStyleRuleImpl)
NS_IMPL_RELEASE(CSSStyleRuleImpl)
#endif

nsresult CSSStyleRuleImpl::QueryInterface(const nsIID& aIID,
                                            void** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null pointer");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
  if (aIID.Equals(kICSSStyleRuleIID)) {
    *aInstancePtrResult = (void*) ((nsICSSStyleRule*)this);
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kIStyleRuleIID)) {
    *aInstancePtrResult = (void*) ((nsIStyleRule*)this);
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtrResult = (void*) ((nsISupports*)this);
    AddRef();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}


PRBool CSSStyleRuleImpl::Equals(const nsIStyleRule* aRule) const
{
  nsICSSStyleRule* iCSSRule;

  if (this == aRule) {
    return PR_TRUE;
  }

  if ((nsnull != aRule) && 
      (NS_OK == ((nsIStyleRule*)aRule)->QueryInterface(kICSSStyleRuleIID, (void**) &iCSSRule))) {

    CSSStyleRuleImpl* rule = (CSSStyleRuleImpl*)iCSSRule;
    const nsCSSSelector* local = &mSelector;
    const nsCSSSelector* other = &(rule->mSelector);
    PRBool  result = PR_TRUE;

    while ((PR_TRUE == result) && (nsnull != local) && (nsnull != other)) {
      if (! local->Equals(other)) {
        result = PR_FALSE;
      }
      local = local->mNext;
      other = other->mNext;
    }
    if ((nsnull != local) || (nsnull != other)) { // more were left
      result = PR_FALSE;
    }
    if ((rule->mDeclaration != mDeclaration) || 
        (rule->mWeight != mWeight)) {
      result = PR_FALSE;
    }
    NS_RELEASE(iCSSRule);
    return result;
  }
  return PR_FALSE;
}

PRUint32 CSSStyleRuleImpl::HashValue(void) const
{
  return (PRUint32)this;
}

nsCSSSelector* CSSStyleRuleImpl::FirstSelector(void)
{
  return &mSelector;
}

void CSSStyleRuleImpl::AddSelector(const nsCSSSelector& aSelector)
{
  if ((nsnull != aSelector.mTag) || (nsnull != aSelector.mID) ||
      (nsnull != aSelector.mClass) || (nsnull != aSelector.mPseudoClass)) { // skip empty selectors
    nsCSSSelector*  selector = new nsCSSSelector(aSelector);
    nsCSSSelector*  last = &mSelector;

    while (nsnull != last->mNext) {
      last = last->mNext;
    }
    last->mNext = selector;
  }
}


void CSSStyleRuleImpl::DeleteSelector(nsCSSSelector* aSelector)
{
  if (nsnull != aSelector) {
    if (&mSelector == aSelector) {  // handle first selector
      mSelector = *aSelector; // assign value
      mSelector.mNext = aSelector->mNext;
      delete aSelector;
    }
    else {
      nsCSSSelector*  selector = &mSelector;

      while (nsnull != selector->mNext) {
        if (aSelector == selector->mNext) {
          selector->mNext = aSelector->mNext;
          delete aSelector;
          return;
        }
        selector = selector->mNext;
      }
    }
  }
}

nsICSSDeclaration* CSSStyleRuleImpl::GetDeclaration(void) const
{
  NS_IF_ADDREF(mDeclaration);
  return mDeclaration;
}

void CSSStyleRuleImpl::SetDeclaration(nsICSSDeclaration* aDeclaration)
{
  NS_IF_RELEASE(mDeclaration);
  mDeclaration = aDeclaration;
  NS_IF_ADDREF(mDeclaration);
}

PRInt32 CSSStyleRuleImpl::GetWeight(void) const
{
  return mWeight;
}

void CSSStyleRuleImpl::SetWeight(PRInt32 aWeight)
{
  mWeight = aWeight;
}

nscoord CSSStyleRuleImpl::CalcLength(const nsCSSValue& aValue,
                                     nsStyleFont* aFont, 
                                     nsIPresContext* aPresContext)
{
  NS_ASSERTION(aValue.IsLengthUnit(), "not a length unit");
  if (aValue.IsFixedLengthUnit()) {
    return aValue.GetLengthTwips();
  }
  nsCSSUnit unit = aValue.GetUnit();
  switch (unit) {
    case eCSSUnit_EM:
      return aFont->mFont.size;
    case eCSSUnit_EN:
      return (aFont->mFont.size / 2);
    case eCSSUnit_XHeight:
      NS_NOTYETIMPLEMENTED("x height unit");
      return ((aFont->mFont.size / 3) * 2); // XXX HACK!
    case eCSSUnit_CapHeight:
      NS_NOTYETIMPLEMENTED("cap height unit");
      return ((aFont->mFont.size / 3) * 2); // XXX HACK!

    case eCSSUnit_Pixel:
      return (nscoord)(aPresContext->GetPixelsToTwips() * aValue.GetFloatValue());
  }
  return 0;
}


#define SETCOORD_NORMAL       0x01
#define SETCOORD_AUTO         0x02
#define SETCOORD_INHERIT      0x04
#define SETCOORD_PERCENT      0x08
#define SETCOORD_FACTOR       0x10
#define SETCOORD_LENGTH       0x20
#define SETCOORD_INTEGER      0x40
#define SETCOORD_ENUMERATED   0x80

#define SETCOORD_LP     (SETCOORD_LENGTH | SETCOORD_PERCENT)
#define SETCOORD_LH     (SETCOORD_LENGTH | SETCOORD_INHERIT)
#define SETCOORD_AH     (SETCOORD_AUTO | SETCOORD_INHERIT)
#define SETCOORD_LPH    (SETCOORD_LP | SETCOORD_INHERIT)
#define SETCOORD_LPFHN  (SETCOORD_LPH | SETCOORD_FACTOR | SETCOORD_NORMAL)
#define SETCOORD_LPAH   (SETCOORD_LP | SETCOORD_AH)
#define SETCOORD_LPEH   (SETCOORD_LP | SETCOORD_ENUMERATED | SETCOORD_INHERIT)
#define SETCOORD_IAH    (SETCOORD_INTEGER | SETCOORD_AH)

PRBool CSSStyleRuleImpl::SetCoord(const nsCSSValue& aValue, nsStyleCoord& aCoord, 
                                  PRInt32 aMask, nsStyleFont* aFont, 
                                  nsIPresContext* aPresContext)
{
  PRBool  result = PR_TRUE;
  if (aValue.GetUnit() == eCSSUnit_Null) {
    result = PR_FALSE;
  }
  else if (((aMask & SETCOORD_LENGTH) != 0) && 
           aValue.IsLengthUnit()) {
    aCoord.SetCoordValue(CalcLength(aValue, aFont, aPresContext));
  } 
  else if (((aMask & SETCOORD_PERCENT) != 0) && 
           (aValue.GetUnit() == eCSSUnit_Percent)) {
    aCoord.SetPercentValue(aValue.GetPercentValue());
  } 
  else if (((aMask & SETCOORD_INTEGER) != 0) && 
           (aValue.GetUnit() == eCSSUnit_Integer)) {
    aCoord.SetIntValue(aValue.GetIntValue(), eStyleUnit_Integer);
  } 
  else if (((aMask & SETCOORD_ENUMERATED) != 0) && 
           (aValue.GetUnit() == eCSSUnit_Enumerated)) {
    aCoord.SetIntValue(aValue.GetIntValue(), eStyleUnit_Enumerated);
  } 
  else if (((aMask & SETCOORD_AUTO) != 0) && 
           (aValue.GetUnit() == eCSSUnit_Auto)) {
    aCoord.SetAutoValue();
  } 
  else if (((aMask & SETCOORD_INHERIT) != 0) && 
           (aValue.GetUnit() == eCSSUnit_Inherit)) {
    aCoord.SetInheritValue();
  }
  else if (((aMask & SETCOORD_NORMAL) != 0) && 
           (aValue.GetUnit() == eCSSUnit_Normal)) {
    aCoord.SetNormalValue();
  }
  else if (((aMask & SETCOORD_FACTOR) != 0) && 
           (aValue.GetUnit() == eCSSUnit_Number)) {
    aCoord.SetFactorValue(aValue.GetFloatValue());
  }
  else {
    result = PR_FALSE;  // didn't set anything
  }
  return result;
}

void CSSStyleRuleImpl::MapStyleInto(nsIStyleContext* aContext, nsIPresContext* aPresContext)
{
  if (nsnull != mDeclaration) {
    nsStyleFont*  font = (nsStyleFont*)aContext->GetData(kStyleFontSID);

    nsCSSFont*  ourFont;
    if (NS_OK == mDeclaration->GetData(kCSSFontSID, (nsCSSStruct**)&ourFont)) {
      if (nsnull != ourFont) {
        nsStyleFont* parentFont = font;
        nsIStyleContext* parentContext = aContext->GetParent();
        if (nsnull != parentContext) {
          parentFont = (nsStyleFont*)parentContext->GetData(kStyleFontSID);
        }

        // font-family: string list
        if (ourFont->mFamily.GetUnit() == eCSSUnit_String) {
          nsAutoString familyList;
          ourFont->mFamily.GetStringValue(familyList);
          // XXX meeds font support to determine usable fonts
          // parse up the CSS string & remove the quotes
          // XXX only does first until we can tell what are installed fonts
          nsAutoString family;
          PRInt32 index = familyList.Find(PRUnichar(','));
          if (-1 < index) {
            familyList.Left(family, index);
          }
          else {
            family.Append(familyList);
          }
          family.StripChars("\"");
          family.StripWhitespace();

          font->mFont.name = family;
        }

        // font-style: enum
        if (ourFont->mStyle.GetUnit() == eCSSUnit_Enumerated) {
          font->mFont.style = ourFont->mStyle.GetIntValue();
        }

        // font-variant: enum
        if (ourFont->mVariant.GetUnit() == eCSSUnit_Enumerated) {
          font->mFont.variant = ourFont->mVariant.GetIntValue();
        }

        // font-weight: abs, enum
        if (ourFont->mWeight.GetUnit() == eCSSUnit_Integer) {
          font->mFont.style = ourFont->mWeight.GetIntValue();
        }
        else if (ourFont->mWeight.GetUnit() == eCSSUnit_Enumerated) {
          PRInt32 value = ourFont->mWeight.GetIntValue();
          switch (value) {
            case NS_STYLE_FONT_WEIGHT_NORMAL:
            case NS_STYLE_FONT_WEIGHT_BOLD:
              font->mFont.weight = value;
              break;
            case NS_STYLE_FONT_WEIGHT_BOLDER:
            case NS_STYLE_FONT_WEIGHT_LIGHTER:
              font->mFont.weight = (parentFont->mFont.weight + value);
              break;
          }
        }

        // font-size: enum, length, percent
        if (ourFont->mSize.GetUnit() == eCSSUnit_Enumerated) {
          static float kFontScale[7] = {
            0.5f,                 // xx-small
            0.666667f,            // x-small
            0.833333f,            // small
            1.0f,                 // medium
            1.5f,                 // large
            1.5f * 1.5f,          // x-large
            1.5f * 1.5f * 1.5f,   // xx-large
          };
          PRInt32 value = ourFont->mSize.GetIntValue();

          const nsFont& normal = aPresContext->GetDefaultFont();  // use normal font or body font??
          if ((NS_STYLE_FONT_SIZE_XXSMALL <= value) && 
              (value <= NS_STYLE_FONT_SIZE_XXLARGE)) {
            font->mFont.size = (nscoord)((float)normal.size * kFontScale[value]);
          }
          else if (NS_STYLE_FONT_SIZE_LARGER == value) {
            PRInt32 index;
            for (index = NS_STYLE_FONT_SIZE_XXSMALL;
                 index < NS_STYLE_FONT_SIZE_XXLARGE; index++)
              if (parentFont->mFont.size < (nscoord)((float)normal.size * kFontScale[index]))
                break;
            font->mFont.size = (nscoord)((float)normal.size * kFontScale[index]);
          }
          else if (NS_STYLE_FONT_SIZE_SMALLER == value) {
            PRInt32 index;
            for (index = NS_STYLE_FONT_SIZE_XXLARGE;
                 index > NS_STYLE_FONT_SIZE_XXSMALL; index--)
              if (parentFont->mFont.size > (nscoord)((float)normal.size * kFontScale[index]))
                break;
            font->mFont.size = (nscoord)((float)normal.size * kFontScale[index]);
          }
        }
        else if (ourFont->mSize.IsLengthUnit()) {
          font->mFont.size = CalcLength(ourFont->mSize, parentFont, aPresContext);
        }
        else if (ourFont->mSize.GetUnit() == eCSSUnit_Percent) {
          font->mFont.size = (nscoord)((float)(parentFont->mFont.size) * ourFont->mSize.GetPercentValue());
        }

        NS_IF_RELEASE(parentContext);
      }
    }

    nsCSSText*  ourText;
    if (NS_OK == mDeclaration->GetData(kCSSTextSID, (nsCSSStruct**)&ourText)) {
      if (nsnull != ourText) {
        // Get our text style and our parent's text style
        nsStyleText* text = (nsStyleText*) aContext->GetData(kStyleTextSID);

        // letter-spacing
        SetCoord(ourText->mLetterSpacing, text->mLetterSpacing, SETCOORD_LH | SETCOORD_NORMAL, 
                 font, aPresContext);

        // line-height
        SetCoord(ourText->mLineHeight, text->mLineHeight, SETCOORD_LPFHN, font, aPresContext);

        // text-align
        if (ourText->mTextAlign.GetUnit() == eCSSUnit_Enumerated) {
          text->mTextAlign = ourText->mTextAlign.GetIntValue();
        }

        // text-indent
        SetCoord(ourText->mTextIndent, text->mTextIndent, SETCOORD_LPH, font, aPresContext);

        // text-decoration: enum, int (bit field)
        if ((ourText->mDecoration.GetUnit() == eCSSUnit_Enumerated) ||
            (ourText->mDecoration.GetUnit() == eCSSUnit_Integer)) {
          PRInt32 td = ourText->mDecoration.GetIntValue();
          font->mFont.decorations = td;
          text->mTextDecoration = td;
        }

        // text-transform
        if (ourText->mTextTransform.GetUnit() == eCSSUnit_Enumerated) {
          text->mTextTransform = ourText->mTextTransform.GetIntValue();
        }

        // vertical-align
        SetCoord(ourText->mVerticalAlign, text->mVerticalAlign, SETCOORD_LPEH,
                 font, aPresContext);

        // white-space
        if (ourText->mWhiteSpace.GetUnit() == eCSSUnit_Enumerated) {
          text->mWhiteSpace = ourText->mWhiteSpace.GetIntValue();
        }

        // word-spacing
        SetCoord(ourText->mWordSpacing, text->mWordSpacing, SETCOORD_LH | SETCOORD_NORMAL,
                 font, aPresContext);
      }
    }

    nsCSSDisplay*  ourDisplay;
    if (NS_OK == mDeclaration->GetData(kCSSDisplaySID, (nsCSSStruct**)&ourDisplay)) {
      if (nsnull != ourDisplay) {
        // Get our style and our parent's style
        nsStyleDisplay* display = (nsStyleDisplay*)
          aContext->GetData(kStyleDisplaySID);

        // display
        if (ourDisplay->mDisplay.GetUnit() == eCSSUnit_Enumerated) {
          display->mDisplay = ourDisplay->mDisplay.GetIntValue();
        }

        // direction: enum
        if (ourDisplay->mDirection.GetUnit() == eCSSUnit_Enumerated) {
          display->mDirection = ourDisplay->mDirection.GetIntValue();
        }

        // clear: enum
        if (ourDisplay->mClear.GetUnit() == eCSSUnit_Enumerated) {
          display->mBreakType = ourDisplay->mClear.GetIntValue();
        }

        // float: enum
        if (ourDisplay->mFloat.GetUnit() == eCSSUnit_Enumerated) {
          display->mFloats = ourDisplay->mFloat.GetIntValue();
        }
      }
    }


    nsCSSColor*  ourColor;
    if (NS_OK == mDeclaration->GetData(kCSSColorSID, (nsCSSStruct**)&ourColor)) {
      if (nsnull != ourColor) {
        nsStyleColor* color = (nsStyleColor*)aContext->GetData(kStyleColorSID);

        // color: color
        if (ourColor->mColor.GetUnit() == eCSSUnit_Color) {
          color->mColor = ourColor->mColor.GetColorValue();
        }

        // cursor: enum, auto, inherit
        if (ourColor->mCursor.GetUnit() == eCSSUnit_Enumerated) {
          color->mCursor = ourColor->mCursor.GetIntValue();
        }
        else if (ourColor->mCursor.GetUnit() == eCSSUnit_Auto) {
          color->mCursor = NS_STYLE_CURSOR_AUTO;
        }
        else if (ourColor->mCursor.GetUnit() == eCSSUnit_Inherit) {
          color->mCursor = NS_STYLE_CURSOR_INHERIT;
        }

        // cursor-image: string
        if (ourColor->mCursorImage.GetUnit() == eCSSUnit_String) {
          ourColor->mCursorImage.GetStringValue(color->mCursorImage);
        }

        // background-color: color, enum (flags)
        if (ourColor->mBackColor.GetUnit() == eCSSUnit_Color) {
          color->mBackgroundColor = ourColor->mBackColor.GetColorValue();
          color->mBackgroundFlags &= ~NS_STYLE_BG_COLOR_TRANSPARENT;
        }
        else if (ourColor->mBackColor.GetUnit() == eCSSUnit_Enumerated) {
          color->mBackgroundFlags |= NS_STYLE_BG_COLOR_TRANSPARENT;
        }

        // background-image: string (url), none
        if (ourColor->mBackImage.GetUnit() == eCSSUnit_String) {
          ourColor->mBackImage.GetStringValue(color->mBackgroundImage);
          color->mBackgroundFlags &= ~NS_STYLE_BG_IMAGE_NONE;
        }
        else if (ourColor->mBackImage.GetUnit() == eCSSUnit_None) {
          color->mBackgroundImage.Truncate();
          color->mBackgroundFlags |= NS_STYLE_BG_IMAGE_NONE;
        }

        // background-repeat: enum
        if (ourColor->mBackRepeat.GetUnit() == eCSSUnit_Enumerated) {
          color->mBackgroundRepeat = ourColor->mBackRepeat.GetIntValue();
        }

        // background-attachment: enum
        if (ourColor->mBackAttachment.GetUnit() == eCSSUnit_Enumerated) {
          color->mBackgroundAttachment = ourColor->mBackAttachment.GetIntValue();
        }

        // background-position: length, percent (flags)
        if (ourColor->mBackPositionX.GetUnit() == eCSSUnit_Percent) {
          color->mBackgroundXPosition = (nscoord)(100.0f * ourColor->mBackPositionX.GetPercentValue());
          color->mBackgroundFlags |= NS_STYLE_BG_X_POSITION_PERCENT;
          color->mBackgroundFlags &= ~NS_STYLE_BG_X_POSITION_LENGTH;
        }
        else if (ourColor->mBackPositionX.IsLengthUnit()) {
          color->mBackgroundXPosition = CalcLength(ourColor->mBackPositionX,
                                                   font, aPresContext);
          color->mBackgroundFlags |= NS_STYLE_BG_X_POSITION_LENGTH;
          color->mBackgroundFlags &= ~NS_STYLE_BG_X_POSITION_PERCENT;
        }
        if (ourColor->mBackPositionY.GetUnit() == eCSSUnit_Percent) {
          color->mBackgroundYPosition = (nscoord)(100.0f * ourColor->mBackPositionY.GetPercentValue());
          color->mBackgroundFlags |= NS_STYLE_BG_Y_POSITION_PERCENT;
          color->mBackgroundFlags &= ~NS_STYLE_BG_Y_POSITION_LENGTH;
        }
        else if (ourColor->mBackPositionY.IsLengthUnit()) {
          color->mBackgroundYPosition = CalcLength(ourColor->mBackPositionY,
                                                   font, aPresContext);
          color->mBackgroundFlags |= NS_STYLE_BG_Y_POSITION_LENGTH;
          color->mBackgroundFlags &= ~NS_STYLE_BG_Y_POSITION_PERCENT;
        }

  // XXX: NYI        nsCSSValue mBackFilter;
      }
    }

    nsCSSMargin*  ourMargin;
    if (NS_OK == mDeclaration->GetData(kCSSMarginSID, (nsCSSStruct**)&ourMargin)) {
      if (nsnull != ourMargin) {
        nsStyleSpacing* spacing = (nsStyleSpacing*)
          aContext->GetData(kStyleSpacingSID);

        // margin: length, percent, auto, inherit
        if (nsnull != ourMargin->mMargin) {
          nsStyleCoord  coord;
          if (SetCoord(ourMargin->mMargin->mLeft, coord, SETCOORD_LPAH, font, aPresContext)) {
            spacing->mMargin.SetLeft(coord);
          }
          if (SetCoord(ourMargin->mMargin->mTop, coord, SETCOORD_LPAH, font, aPresContext)) {
            spacing->mMargin.SetTop(coord);
          }
          if (SetCoord(ourMargin->mMargin->mRight, coord, SETCOORD_LPAH, font, aPresContext)) {
            spacing->mMargin.SetRight(coord);
          }
          if (SetCoord(ourMargin->mMargin->mBottom, coord, SETCOORD_LPAH, font, aPresContext)) {
            spacing->mMargin.SetBottom(coord);
          }
        }

        // padding: length, percent, inherit
        if (nsnull != ourMargin->mPadding) {
          nsStyleCoord  coord;
          if (SetCoord(ourMargin->mPadding->mLeft, coord, SETCOORD_LPH, font, aPresContext)) {
            spacing->mPadding.SetLeft(coord);
          }
          if (SetCoord(ourMargin->mPadding->mTop, coord, SETCOORD_LPH, font, aPresContext)) {
            spacing->mPadding.SetTop(coord);
          }
          if (SetCoord(ourMargin->mPadding->mRight, coord, SETCOORD_LPH, font, aPresContext)) {
            spacing->mPadding.SetRight(coord);
          }
          if (SetCoord(ourMargin->mPadding->mBottom, coord, SETCOORD_LPH, font, aPresContext)) {
            spacing->mPadding.SetBottom(coord);
          }
        }

        // border-size: length, enum (percent), inherit
        if (nsnull != ourMargin->mBorder) {
          nsStyleCoord  coord;
          if (SetCoord(ourMargin->mBorder->mLeft, coord, SETCOORD_LPEH, font, aPresContext)) {
            spacing->mBorder.SetLeft(coord);
          }
          if (SetCoord(ourMargin->mBorder->mTop, coord, SETCOORD_LPEH, font, aPresContext)) {
            spacing->mBorder.SetTop(coord);
          }
          if (SetCoord(ourMargin->mBorder->mRight, coord, SETCOORD_LPEH, font, aPresContext)) {
            spacing->mBorder.SetRight(coord);
          }
          if (SetCoord(ourMargin->mBorder->mBottom, coord, SETCOORD_LPEH, font, aPresContext)) {
            spacing->mBorder.SetBottom(coord);
          }
        }

        // border-style
        if (nsnull != ourMargin->mStyle) {
          nsCSSRect* ourStyle = ourMargin->mStyle;
          if (ourStyle->mTop.GetUnit() == eCSSUnit_Enumerated) {
            spacing->mBorderStyle[NS_SIDE_TOP] = ourStyle->mTop.GetIntValue();
          }
          if (ourStyle->mRight.GetUnit() == eCSSUnit_Enumerated) {
            spacing->mBorderStyle[NS_SIDE_RIGHT] = ourStyle->mRight.GetIntValue();
          }
          if (ourStyle->mBottom.GetUnit() == eCSSUnit_Enumerated) {
            spacing->mBorderStyle[NS_SIDE_BOTTOM] = ourStyle->mBottom.GetIntValue();
          }
          if (ourStyle->mLeft.GetUnit() == eCSSUnit_Enumerated) {
            spacing->mBorderStyle[NS_SIDE_LEFT] = ourStyle->mLeft.GetIntValue();
          }
        }

        // border-color
        nsStyleColor* color = (nsStyleColor*)aContext->GetData(kStyleColorSID);
        if (nsnull != ourMargin->mColor) {
          nsCSSRect* ourColor = ourMargin->mColor;
          if (ourColor->mTop.GetUnit() == eCSSUnit_Color) {
            spacing->mBorderColor[NS_SIDE_TOP] = ourColor->mTop.GetColorValue();
          }
          if (ourColor->mRight.GetUnit() == eCSSUnit_Color) {
            spacing->mBorderColor[NS_SIDE_RIGHT] = ourColor->mRight.GetColorValue();
          }
          if (ourColor->mBottom.GetUnit() == eCSSUnit_Color) {
            spacing->mBorderColor[NS_SIDE_BOTTOM] = ourColor->mBottom.GetColorValue();
          }
          if (ourColor->mLeft.GetUnit() == eCSSUnit_Color) {
            spacing->mBorderColor[NS_SIDE_LEFT] = ourColor->mLeft.GetColorValue();
          }
        }
      }
    }

    nsCSSPosition*  ourPosition;
    if (NS_OK == mDeclaration->GetData(kCSSPositionSID, (nsCSSStruct**)&ourPosition)) {
      if (nsnull != ourPosition) {
        nsStylePosition* position = (nsStylePosition*)aContext->GetData(kStylePositionSID);

        // position: normal, enum, inherit
        if (ourPosition->mPosition.GetUnit() == eCSSUnit_Normal) {
          position->mPosition = NS_STYLE_POSITION_NORMAL;
        }
        else if (ourPosition->mPosition.GetUnit() == eCSSUnit_Enumerated) {
          position->mPosition = ourPosition->mPosition.GetIntValue();
        }
        else if (ourPosition->mPosition.GetUnit() == eCSSUnit_Inherit) {
          // explicit inheritance
          nsIStyleContext* parentContext = aContext->GetParent();
          if (nsnull != parentContext) {
            nsStylePosition* parentPosition = (nsStylePosition*)parentContext->GetData(kStylePositionSID);
            position->mPosition = parentPosition->mPosition;
          }
        }

        // overflow
        if (ourPosition->mOverflow.GetUnit() == eCSSUnit_Enumerated) {
          position->mOverflow = ourPosition->mOverflow.GetIntValue();
        }

        // box offsets: length, percent, auto, inherit
        SetCoord(ourPosition->mLeft, position->mLeftOffset, SETCOORD_LPAH, font, aPresContext);
        SetCoord(ourPosition->mTop, position->mTopOffset, SETCOORD_LPAH, font, aPresContext);
        SetCoord(ourPosition->mWidth, position->mWidth, SETCOORD_LPAH, font, aPresContext);
        SetCoord(ourPosition->mHeight, position->mHeight, SETCOORD_LPAH, font, aPresContext);

        // z-index
        SetCoord(ourPosition->mZIndex, position->mZIndex, SETCOORD_IAH, nsnull, nsnull);

        // clip property: length, auto, inherit
        if (nsnull != ourPosition->mClip) {
          if (ourPosition->mClip->mTop.GetUnit() == eCSSUnit_Inherit) { // if one is inherit, they all are
            position->mClipFlags = NS_STYLE_CLIP_INHERIT;
          }
          else {
            PRBool  fullAuto = PR_TRUE;

            position->mClipFlags = 0; // clear it

            if (ourPosition->mClip->mTop.GetUnit() == eCSSUnit_Auto) {
              position->mClip.top = 0;
              position->mClipFlags |= NS_STYLE_CLIP_TOP_AUTO;
            } else if (ourPosition->mClip->mTop.IsLengthUnit()) {
              position->mClip.top = CalcLength(ourPosition->mClip->mTop, font, aPresContext);
              fullAuto = PR_FALSE;
            }
            if (ourPosition->mClip->mRight.GetUnit() == eCSSUnit_Auto) {
              position->mClip.right = 0;
              position->mClipFlags |= NS_STYLE_CLIP_RIGHT_AUTO;
            } else if (ourPosition->mClip->mRight.IsLengthUnit()) {
              position->mClip.right = CalcLength(ourPosition->mClip->mRight, font, aPresContext);
              fullAuto = PR_FALSE;
            }
            if (ourPosition->mClip->mBottom.GetUnit() == eCSSUnit_Auto) {
              position->mClip.bottom = 0;
              position->mClipFlags |= NS_STYLE_CLIP_BOTTOM_AUTO;
            } else if (ourPosition->mClip->mBottom.IsLengthUnit()) {
              position->mClip.bottom = CalcLength(ourPosition->mClip->mBottom, font, aPresContext);
              fullAuto = PR_FALSE;
            }
            if (ourPosition->mClip->mLeft.GetUnit() == eCSSUnit_Auto) {
              position->mClip.left = 0;
              position->mClipFlags |= NS_STYLE_CLIP_LEFT_AUTO;
            } else if (ourPosition->mClip->mLeft.IsLengthUnit()) {
              position->mClip.left = CalcLength(ourPosition->mClip->mLeft, font, aPresContext);
              fullAuto = PR_FALSE;
            }
            position->mClipFlags &= ~NS_STYLE_CLIP_TYPE_MASK;
            if (fullAuto) {
              position->mClipFlags |= NS_STYLE_CLIP_AUTO;
            }
            else {
              position->mClipFlags |= NS_STYLE_CLIP_RECT;
            }
          }
        }
      }
    }

    nsCSSList* ourList;
    if (NS_OK == mDeclaration->GetData(kCSSListSID, (nsCSSStruct**)&ourList)) {
      if (nsnull != ourList) {
        nsStyleList* list = (nsStyleList*)aContext->GetData(kStyleListSID);

        // list-style-type: enum
        if (ourList->mType.GetUnit() == eCSSUnit_Enumerated) {
          list->mListStyleType = ourList->mType.GetIntValue();
        }

        if (ourList->mImage.GetUnit() == eCSSUnit_String) {
          // list-style-image: string
          ourList->mImage.GetStringValue(list->mListStyleImage);
        }
        else if (ourList->mImage.GetUnit() == eCSSUnit_Enumerated) {
          // list-style-image: none
          list->mListStyleImage = "";
        }

        // list-style-position: enum
        if (ourList->mPosition.GetUnit() == eCSSUnit_Enumerated) {
          list->mListStylePosition = ourList->mPosition.GetIntValue();
        }
      }
    }
  }
}



static void ListSelector(FILE* out, const nsCSSSelector* aSelector)
{
  nsAutoString buffer;

  if (nsnull != aSelector->mTag) {
    aSelector->mTag->ToString(buffer);
    fputs(buffer, out);
  }
  if (nsnull != aSelector->mID) {
    aSelector->mID->ToString(buffer);
    fputs("#", out);
    fputs(buffer, out);
  }
  if (nsnull != aSelector->mClass) {
    aSelector->mClass->ToString(buffer);
    fputs(".", out);
    fputs(buffer, out);
  }
  if (nsnull != aSelector->mPseudoClass) {
    aSelector->mPseudoClass->ToString(buffer);
    fputs(":", out);
    fputs(buffer, out);
  }
}

void CSSStyleRuleImpl::List(FILE* out, PRInt32 aIndent) const
{
  // Indent
  for (PRInt32 index = aIndent; --index >= 0; ) fputs("  ", out);

  const nsCSSSelector*  selector = &mSelector;

  while (nsnull != selector) {
    ListSelector(out, selector);
    fputs(" ", out);
    selector = selector->mNext;
  }

  nsAutoString buffer;

  buffer.Append("weight: ");
  buffer.Append(mWeight, 10);
  buffer.Append(" ");
  fputs(buffer, out);
  if (nsnull != mDeclaration) {
    mDeclaration->List(out);
  }
  else {
    fputs("{ null declaration }", out);
  }
  fputs("\n", out);
}

NS_HTML nsresult
  NS_NewCSSStyleRule(nsICSSStyleRule** aInstancePtrResult, const nsCSSSelector& aSelector)
{
  if (aInstancePtrResult == nsnull) {
    return NS_ERROR_NULL_POINTER;
  }

  CSSStyleRuleImpl  *it = new CSSStyleRuleImpl(aSelector);

  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return it->QueryInterface(kICSSStyleRuleIID, (void **) aInstancePtrResult);
}
