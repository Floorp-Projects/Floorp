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
#include "nsUnitConversion.h"

//#define DEBUG_REFS

static NS_DEFINE_IID(kIStyleRuleIID, NS_ISTYLE_RULE_IID);
static NS_DEFINE_IID(kICSSDeclarationIID, NS_ICSS_DECLARATION_IID);
static NS_DEFINE_IID(kICSSStyleRuleIID, NS_ICSS_STYLE_RULE_IID);

static NS_DEFINE_IID(kStyleFontSID, NS_STYLEFONT_SID);
static NS_DEFINE_IID(kStyleColorSID, NS_STYLECOLOR_SID);
static NS_DEFINE_IID(kStyleListSID, NS_STYLELIST_SID);
static NS_DEFINE_IID(kStyleSpacingSID, NS_STYLESPACING_SID);
static NS_DEFINE_IID(kStyleMoleculeSID, NS_STYLEMOLECULE_SID);

static NS_DEFINE_IID(kCSSFontSID, NS_CSS_FONT_SID);
static NS_DEFINE_IID(kCSSColorSID, NS_CSS_COLOR_SID);
static NS_DEFINE_IID(kCSSTextSID, NS_CSS_TEXT_SID);
static NS_DEFINE_IID(kCSSMarginSID, NS_CSS_MARGIN_SID);
static NS_DEFINE_IID(kCSSPositionSID, NS_CSS_POSITION_SID);
static NS_DEFINE_IID(kCSSListSID, NS_CSS_LIST_SID);


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
  NS_IF_RELEASE(mTag);
  NS_IF_RELEASE(mID);
  NS_IF_RELEASE(mClass);
  NS_IF_RELEASE(mPseudoClass);
  if (0 < aTag.Length()) {
    mTag = NS_NewAtom(aTag);
  }
  if (0 < aID.Length()) {
    mID = NS_NewAtom(aID);
  }
  if (0 < aClass.Length()) {
    mClass = NS_NewAtom(aClass);
  }
  if (0 < aPseudoClass.Length()) {
    mPseudoClass = NS_NewAtom(aPseudoClass);
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
  nsCSSSelector*  selector = new nsCSSSelector(aSelector);
  nsCSSSelector*  last = &mSelector;

  while (nsnull != last->mNext) {
    last = last->mNext;
  }
  last->mNext = selector;
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

nscoord CSSStyleRuleImpl::CalcLength(const nsCSSValue& aValue, nsStyleFont* aFont, 
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
        if (ourFont->mWeight.GetUnit() == eCSSUnit_Absolute) {
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
          font->mFont.size = (nscoord)((float)(parentFont->mFont.size) * ourFont->mSize.GetFloatValue());
        }

        NS_IF_RELEASE(parentContext);
      }
    }

    nsCSSText*  ourText;
    if (NS_OK == mDeclaration->GetData(kCSSTextSID, (nsCSSStruct**)&ourText)) {
      if (nsnull != ourText) {

        // text-decoration: enum, absolute (bit field)
        if (ourText->mDecoration.GetUnit() == eCSSUnit_Enumerated) {
          font->mFont.decorations = ourText->mDecoration.GetIntValue();
        }
        else if (ourText->mDecoration.GetUnit() == eCSSUnit_Absolute) {
          font->mFont.decorations = ourText->mDecoration.GetIntValue();
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

        // background-color: color, enum (flags)
        if (ourColor->mBackColor.GetUnit() == eCSSUnit_Color) {
          color->mBackgroundColor = ourColor->mBackColor.GetColorValue();
          color->mBackgroundFlags &= ~NS_STYLE_BG_COLOR_TRANSPARENT;
        }
        else if (ourColor->mBackColor.GetUnit() == eCSSUnit_Enumerated) {
          color->mBackgroundFlags |= NS_STYLE_BG_COLOR_TRANSPARENT;
        }

        // background-image: string, enum (flags)
        if (ourColor->mBackImage.GetUnit() == eCSSUnit_String) {
          ourColor->mBackImage.GetStringValue(color->mBackgroundImage);
          color->mBackgroundFlags &= ~NS_STYLE_BG_IMAGE_NONE;
        }
        else if (ourColor->mBackImage.GetUnit() == eCSSUnit_Enumerated) {
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
          color->mBackgroundXPosition = (nscoord)(TWIPS_CONST_FLOAT * ourColor->mBackPositionX.GetFloatValue());
          color->mBackgroundFlags |= NS_STYLE_BG_X_POSITION_PCT;
          color->mBackgroundFlags &= ~NS_STYLE_BG_X_POSITION_LENGTH;
        }
        else if (ourColor->mBackPositionX.IsLengthUnit()) {
          color->mBackgroundXPosition = CalcLength(ourColor->mBackPositionX,
                                                   font, aPresContext);
          color->mBackgroundFlags |= NS_STYLE_BG_X_POSITION_LENGTH;
          color->mBackgroundFlags &= ~NS_STYLE_BG_X_POSITION_PCT;
        }
        if (ourColor->mBackPositionY.GetUnit() == eCSSUnit_Percent) {
          color->mBackgroundYPosition = (nscoord)(TWIPS_CONST_FLOAT * ourColor->mBackPositionY.GetFloatValue());
          color->mBackgroundFlags |= NS_STYLE_BG_Y_POSITION_PCT;
          color->mBackgroundFlags &= ~NS_STYLE_BG_Y_POSITION_LENGTH;
        }
        else if (ourColor->mBackPositionY.IsLengthUnit()) {
          color->mBackgroundYPosition = CalcLength(ourColor->mBackPositionY,
                                                   font, aPresContext);
          color->mBackgroundFlags |= NS_STYLE_BG_Y_POSITION_LENGTH;
          color->mBackgroundFlags &= ~NS_STYLE_BG_Y_POSITION_PCT;
        }

  // XXX: NYI        nsCSSValue mBackFilter;
      }
    }

    nsCSSMargin*  ourMargin;
    if (NS_OK == mDeclaration->GetData(kCSSMarginSID, (nsCSSStruct**)&ourMargin)) {
      if (nsnull != ourMargin) {
        nsStyleSpacing* spacing = (nsStyleSpacing*)aContext->GetData(kStyleSpacingSID);

        // margin
        if (nsnull != ourMargin->mMargin) {
          if (ourMargin->mMargin->mLeft.IsLengthUnit()) {
            spacing->mMargin.left = CalcLength(ourMargin->mMargin->mLeft, font, aPresContext);
          } else if (ourMargin->mMargin->mLeft.GetUnit() != eCSSUnit_Null) {
            // XXX handle percent properly, this isn't it
            spacing->mMargin.left = (nscoord)ourMargin->mMargin->mLeft.GetFloatValue();
          }
          if (ourMargin->mMargin->mTop.IsLengthUnit()) {
            spacing->mMargin.top = CalcLength(ourMargin->mMargin->mTop, font, aPresContext);
          } else if (ourMargin->mMargin->mTop.GetUnit() != eCSSUnit_Null) {
            // XXX handle percent properly, this isn't it
            spacing->mMargin.top = (nscoord)ourMargin->mMargin->mTop.GetFloatValue();
          }
          if (ourMargin->mMargin->mRight.IsLengthUnit()) {
            spacing->mMargin.right = CalcLength(ourMargin->mMargin->mRight, font, aPresContext);
          } else if (ourMargin->mMargin->mRight.GetUnit() != eCSSUnit_Null) {
            // XXX handle percent properly, this isn't it
            spacing->mMargin.right = (nscoord)ourMargin->mMargin->mRight.GetFloatValue();
          }
          if (ourMargin->mMargin->mBottom.IsLengthUnit()) {
            spacing->mMargin.bottom = CalcLength(ourMargin->mMargin->mBottom, font, aPresContext);
          } else if (ourMargin->mMargin->mBottom.GetUnit() != eCSSUnit_Null) {
            // XXX handle percent properly, this isn't it
            spacing->mMargin.bottom = (nscoord)ourMargin->mMargin->mBottom.GetFloatValue();
          }
        }
      }
    }

    nsCSSPosition*  ourPosition;
    if (NS_OK == mDeclaration->GetData(kCSSPositionSID, (nsCSSStruct**)&ourPosition)) {
      if (nsnull != ourPosition) {
        nsStyleMolecule* hack = (nsStyleMolecule*)aContext->GetData(kStyleMoleculeSID);

        if (ourPosition->mPosition.GetUnit() == eCSSUnit_Enumerated) {
          hack->positionFlags = ourPosition->mPosition.GetIntValue();
        } else {
          hack->positionFlags = NS_STYLE_POSITION_STATIC;
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

        // list-style-image: string
        if (ourList->mImage.GetUnit() == eCSSUnit_String) {
          ourList->mImage.GetStringValue(list->mListStyleImage);
        }
        else if (ourList->mImage.GetUnit() == eCSSUnit_Enumerated) {  // handle "none"
          list->mListStyleImage = "";
        }

        // list-style-position: enum
        if (ourList->mPosition.GetUnit() == eCSSUnit_Enumerated) {
          list->mListStyleType = ourList->mPosition.GetIntValue();
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
