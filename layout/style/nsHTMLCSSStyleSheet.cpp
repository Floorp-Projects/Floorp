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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
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

#include "nsIHTMLCSSStyleSheet.h"
#include "nsIArena.h"
#include "nsCRT.h"
#include "nsIAtom.h"
#include "nsIURL.h"
#include "nsISupportsArray.h"
#include "nsHTMLAtoms.h"
#include "nsIHTMLContent.h"
#include "nsIStyleRule.h"
#include "nsIFrame.h"
#include "nsHTMLIIDs.h"
#include "nsICSSStyleRule.h"
#include "nsIStyleRuleProcessor.h"
#include "nsIStyleContext.h"
#include "nsIPresContext.h"
#include "nsIDocument.h"
#include "nsCOMPtr.h"

#include "nsIStyleSet.h"
#include "nsIRuleWalker.h"

#include "nsISizeOfHandler.h"


class CSSFirstLineRule : public nsIStyleRule {
public:
  CSSFirstLineRule(nsIHTMLCSSStyleSheet* aSheet);
  virtual ~CSSFirstLineRule();

  NS_DECL_ISUPPORTS

  NS_IMETHOD Equals(const nsIStyleRule* aRule, PRBool& aValue) const;
  NS_IMETHOD HashValue(PRUint32& aValue) const;
  NS_IMETHOD GetStyleSheet(nsIStyleSheet*& aSheet) const;
  NS_IMETHOD GetStrength(PRInt32& aStrength) const;
  
  // The new mapping function.
  NS_IMETHOD MapRuleInfoInto(nsRuleData* aRuleData);

#ifdef DEBUG
  NS_IMETHOD List(FILE* out = stdout, PRInt32 aIndent = 0) const;

  virtual void SizeOf(nsISizeOfHandler *aSizeofHandler, PRUint32 &aSize);
#endif

  nsIHTMLCSSStyleSheet*  mSheet;
};

CSSFirstLineRule::CSSFirstLineRule(nsIHTMLCSSStyleSheet* aSheet)
  : mSheet(aSheet)
{
  NS_INIT_REFCNT();
}

CSSFirstLineRule::~CSSFirstLineRule()
{
}

NS_IMPL_ISUPPORTS1(CSSFirstLineRule, nsIStyleRule)

NS_IMETHODIMP
CSSFirstLineRule::Equals(const nsIStyleRule* aRule, PRBool& aResult) const
{
  aResult = PRBool(this == aRule);
  return NS_OK;
}

NS_IMETHODIMP
CSSFirstLineRule::HashValue(PRUint32& aValue) const
{
  aValue = (PRUint32)7;         // XXX got a better suggestion?
  return NS_OK;
}

NS_IMETHODIMP
CSSFirstLineRule::GetStyleSheet(nsIStyleSheet*& aSheet) const
{
  NS_IF_ADDREF(mSheet);
  aSheet = mSheet;
  return NS_OK;
}

// Strength is an out-of-band weighting, always 0 here
NS_IMETHODIMP
CSSFirstLineRule::GetStrength(PRInt32& aStrength) const
{
  aStrength = 0;
  return NS_OK;
}

NS_IMETHODIMP
CSSFirstLineRule::MapRuleInfoInto(nsRuleData* aData)
{
  if (!aData)
    return NS_OK;

  if (aData->mSID == eStyleStruct_Border && aData->mMarginData) {
    // Disable the border.
    nsCSSValue styleVal(NS_STYLE_BORDER_STYLE_NONE, eCSSUnit_Enumerated);
    if (aData->mMarginData->mBorderStyle->mLeft.GetUnit() == eCSSUnit_Null)
      aData->mMarginData->mBorderStyle->mLeft = styleVal;
    if (aData->mMarginData->mBorderStyle->mRight.GetUnit() == eCSSUnit_Null)
      aData->mMarginData->mBorderStyle->mRight = styleVal;
    if (aData->mMarginData->mBorderStyle->mTop.GetUnit() == eCSSUnit_Null)
      aData->mMarginData->mBorderStyle->mTop = styleVal;
    if (aData->mMarginData->mBorderStyle->mBottom.GetUnit() == eCSSUnit_Null)
      aData->mMarginData->mBorderStyle->mBottom = styleVal;
  }

  return NS_OK;
}

#ifdef DEBUG
NS_IMETHODIMP
CSSFirstLineRule::List(FILE* out, PRInt32 aIndent) const
{
  return NS_OK;
}

/******************************************************************************
* SizeOf method:
*
*  Self (reported as CSSFirstLineRule's size): 
*    1) sizeof(*this) 
*
*  Contained / Aggregated data (not reported as CSSFirstLineRule's size):
*    1) Delegate to mSheet if it exists
*
*  Children / siblings / parents:
*    none
*    
******************************************************************************/
void CSSFirstLineRule::SizeOf(nsISizeOfHandler *aSizeOfHandler, PRUint32 &aSize)
{
  NS_ASSERTION(aSizeOfHandler != nsnull, "SizeOf handler cannot be null");

  // first get the unique items collection
  UNIQUE_STYLE_ITEMS(uniqueItems);

  if(! uniqueItems->AddItem((void*)this) ){
    // object has already been accounted for
    return;
  }

  // get or create a tag for this instance
  nsCOMPtr<nsIAtom> tag;
  tag = getter_AddRefs(NS_NewAtom("CSSFirstLine-LetterRule"));
  // get the size of an empty instance and add to the sizeof handler
  aSize = sizeof(*this);
  aSizeOfHandler->AddSize(tag,aSize);

  if(mSheet){
    PRUint32 localSize=0;
    mSheet->SizeOf(aSizeOfHandler, localSize);
  }
}
#endif

// -----------------------------------------------------------

class CSSFirstLetterRule : public CSSFirstLineRule {
public:
  CSSFirstLetterRule(nsIHTMLCSSStyleSheet* aSheet);
};

CSSFirstLetterRule::CSSFirstLetterRule(nsIHTMLCSSStyleSheet* aSheet)
  : CSSFirstLineRule(aSheet)
{
}

// -----------------------------------------------------------

class HTMLCSSStyleSheetImpl : public nsIHTMLCSSStyleSheet,
                              public nsIStyleRuleProcessor {
public:
  void* operator new(size_t size);
  void* operator new(size_t size, nsIArena* aArena);
  void operator delete(void* ptr);

  HTMLCSSStyleSheetImpl();

  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);
  NS_IMETHOD_(nsrefcnt) AddRef();
  NS_IMETHOD_(nsrefcnt) Release();

  // basic style sheet data
  NS_IMETHOD Init(nsIURI* aURL, nsIDocument* aDocument);
  NS_IMETHOD Reset(nsIURI* aURL);
  NS_IMETHOD GetURL(nsIURI*& aURL) const;
  NS_IMETHOD GetTitle(nsString& aTitle) const;
  NS_IMETHOD GetType(nsString& aType) const;
  NS_IMETHOD GetMediumCount(PRInt32& aCount) const;
  NS_IMETHOD GetMediumAt(PRInt32 aIndex, nsIAtom*& aMedium) const;
  NS_IMETHOD_(PRBool) UseForMedium(nsIAtom* aMedium) const;

  NS_IMETHOD GetEnabled(PRBool& aEnabled) const;
  NS_IMETHOD SetEnabled(PRBool aEnabled);

  // style sheet owner info
  NS_IMETHOD GetParentSheet(nsIStyleSheet*& aParent) const;  // will be null
  NS_IMETHOD GetOwningDocument(nsIDocument*& aDocument) const;
  NS_IMETHOD SetOwningDocument(nsIDocument* aDocument);

  NS_IMETHOD GetStyleRuleProcessor(nsIStyleRuleProcessor*& aProcessor,
                                   nsIStyleRuleProcessor* aPrevProcessor);

  // nsIStyleRuleProcessor api
  NS_IMETHOD RulesMatching(nsIPresContext* aPresContext,
                           nsIAtom* aMedium,
                           nsIContent* aContent,
                           nsIStyleContext* aParentContext,
                           nsIRuleWalker* aRuleWalker);

  NS_IMETHOD RulesMatching(nsIPresContext* aPresContext,
                           nsIAtom* aMedium,
                           nsIContent* aParentContent,
                           nsIAtom* aPseudoTag,
                           nsIStyleContext* aParentContext,
                           nsICSSPseudoComparator* aComparator,
                           nsIRuleWalker* aRuleWalker);

  NS_IMETHOD HasStateDependentStyle(nsIPresContext* aPresContext,
                                    nsIAtom*        aMedium,
                                    nsIContent*     aContent);

  // XXX style rule enumerations

#ifdef DEBUG
  virtual void List(FILE* out = stdout, PRInt32 aIndent = 0) const;

  virtual void SizeOf(nsISizeOfHandler *aSizeofHandler, PRUint32 &aSize);
#endif

  // If changing the given attribute cannot affect style context, aAffects
  // will be PR_FALSE on return.
  NS_IMETHOD AttributeAffectsStyle(nsIAtom *aAttribute, nsIContent *aContent,
                                   PRBool &aAffects);
private: 
  // These are not supported and are not implemented! 
  HTMLCSSStyleSheetImpl(const HTMLCSSStyleSheetImpl& aCopy); 
  HTMLCSSStyleSheetImpl& operator=(const HTMLCSSStyleSheetImpl& aCopy); 

protected:
  virtual ~HTMLCSSStyleSheetImpl();

protected:
  PRUint32 mInHeap : 1;
  PRUint32 mRefCnt : 31;
  NS_DECL_OWNINGTHREAD // for thread-safety checking

  nsIURI*         mURL;
  nsIDocument*    mDocument;

  CSSFirstLineRule* mFirstLineRule;
  CSSFirstLetterRule* mFirstLetterRule;
};


void* HTMLCSSStyleSheetImpl::operator new(size_t size)
{
  HTMLCSSStyleSheetImpl* rv = (HTMLCSSStyleSheetImpl*) ::operator new(size);
#ifdef NS_DEBUG
  if (nsnull != rv) {
    nsCRT::memset(rv, 0xEE, size);
  }
#endif
  rv->mInHeap = 1;
  return (void*) rv;
}

void* HTMLCSSStyleSheetImpl::operator new(size_t size, nsIArena* aArena)
{
  HTMLCSSStyleSheetImpl* rv = (HTMLCSSStyleSheetImpl*) aArena->Alloc(PRInt32(size));
#ifdef NS_DEBUG
  if (nsnull != rv) {
    nsCRT::memset(rv, 0xEE, size);
  }
#endif
  rv->mInHeap = 0;
  return (void*) rv;
}

void HTMLCSSStyleSheetImpl::operator delete(void* ptr)
{
  HTMLCSSStyleSheetImpl* sheet = (HTMLCSSStyleSheetImpl*) ptr;
  if (nsnull != sheet) {
    if (sheet->mInHeap) {
      ::operator delete(ptr);
    }
  }
}

HTMLCSSStyleSheetImpl::HTMLCSSStyleSheetImpl()
  : nsIHTMLCSSStyleSheet(),
    mURL(nsnull),
    mDocument(nsnull),
    mFirstLineRule(nsnull),
    mFirstLetterRule(nsnull)
{
  NS_INIT_REFCNT();
}

HTMLCSSStyleSheetImpl::~HTMLCSSStyleSheetImpl()
{
  NS_RELEASE(mURL);
  if (nsnull != mFirstLineRule) {
    mFirstLineRule->mSheet = nsnull;
    NS_RELEASE(mFirstLineRule);
  }
  if (nsnull != mFirstLetterRule) {
    mFirstLetterRule->mSheet = nsnull;
    NS_RELEASE(mFirstLetterRule);
  }
}

NS_IMPL_ADDREF(HTMLCSSStyleSheetImpl)
NS_IMPL_RELEASE(HTMLCSSStyleSheetImpl)

nsresult HTMLCSSStyleSheetImpl::QueryInterface(const nsIID& aIID,
                                               void** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null pointer");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
  if (aIID.Equals(NS_GET_IID(nsIHTMLCSSStyleSheet))) {
    *aInstancePtrResult = (void*) ((nsIHTMLCSSStyleSheet*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsIStyleSheet))) {
    *aInstancePtrResult = (void*) ((nsIStyleSheet*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsIStyleRuleProcessor))) {
    *aInstancePtrResult = (void*) ((nsIStyleRuleProcessor*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtrResult = (void*) ((nsISupports*)(nsIStyleSheet*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

NS_IMETHODIMP
HTMLCSSStyleSheetImpl::GetStyleRuleProcessor(nsIStyleRuleProcessor*& aProcessor,
                                             nsIStyleRuleProcessor* /*aPrevProcessor*/)
{
  aProcessor = this;
  NS_ADDREF(aProcessor);
  return NS_OK;
}

NS_IMETHODIMP
HTMLCSSStyleSheetImpl::RulesMatching(nsIPresContext* aPresContext,
                                     nsIAtom* aMedium,
                                     nsIContent* aContent,
                                     nsIStyleContext* aParentContext,
                                     nsIRuleWalker* aRuleWalker)
{
  NS_PRECONDITION(nsnull != aPresContext, "null arg");
  NS_PRECONDITION(nsnull != aContent, "null arg");
  NS_PRECONDITION(nsnull != aRuleWalker, "null arg");

  nsCOMPtr<nsIStyledContent> styledContent(do_QueryInterface(aContent));
  
  if (styledContent) 
    // just get the one and only style rule from the content's STYLE attribute
    styledContent->WalkInlineStyleRules(aRuleWalker);

  return NS_OK;
}

NS_IMETHODIMP
HTMLCSSStyleSheetImpl::RulesMatching(nsIPresContext* aPresContext,
                                     nsIAtom* aMedium,
                                     nsIContent* aParentContent,
                                     nsIAtom* aPseudoTag,
                                     nsIStyleContext* aParentContext,
                                     nsICSSPseudoComparator* aComparator,
                                     nsIRuleWalker* aRuleWalker)
{
  if (aPseudoTag == nsHTMLAtoms::firstLinePseudo) {
    PRBool atRoot = PR_FALSE;
    aRuleWalker->AtRoot(&atRoot);
    if (!atRoot) {
      if (nsnull == mFirstLineRule) {
        mFirstLineRule = new CSSFirstLineRule(this);
        if (mFirstLineRule) {
          NS_ADDREF(mFirstLineRule);
        }
      }
      if (mFirstLineRule) {
        aRuleWalker->Forward(mFirstLineRule);
        return NS_OK; 
      }
    } 
  }
  if (aPseudoTag == nsHTMLAtoms::firstLetterPseudo) {
    PRBool atRoot = PR_FALSE;
    aRuleWalker->AtRoot(&atRoot);
    if (!atRoot) {
      if (nsnull == mFirstLetterRule) {
        mFirstLetterRule = new CSSFirstLetterRule(this);
        if (mFirstLetterRule) {
          NS_ADDREF(mFirstLetterRule);
        }
      }
      if (mFirstLetterRule) {
        aRuleWalker->Forward(mFirstLetterRule);
        return NS_OK; 
      }
    } 
  }
  // else no pseudo frame style... 
  return NS_OK;
}

NS_IMETHODIMP
HTMLCSSStyleSheetImpl::Init(nsIURI* aURL, nsIDocument* aDocument)
{
  NS_PRECONDITION(aURL && aDocument, "null ptr");
  if (! aURL || ! aDocument)
    return NS_ERROR_NULL_POINTER;

  if (mURL || mDocument)
    return NS_ERROR_ALREADY_INITIALIZED;

  mDocument = aDocument; // not refcounted!
  mURL = aURL;
  NS_ADDREF(mURL);
  return NS_OK;
}

// Test if style is dependent on content state
NS_IMETHODIMP
HTMLCSSStyleSheetImpl::HasStateDependentStyle(nsIPresContext* aPresContext,
                                              nsIAtom*        aMedium,
                                              nsIContent*     aContent)
{
  return NS_COMFALSE;
}



NS_IMETHODIMP 
HTMLCSSStyleSheetImpl::Reset(nsIURI* aURL)
{
  NS_IF_RELEASE(mURL);
  mURL = aURL;
  NS_ADDREF(mURL);
  if (nsnull != mFirstLineRule) {
    mFirstLineRule->mSheet = nsnull;
    NS_RELEASE(mFirstLineRule);
  }
  if (nsnull != mFirstLetterRule) {
    mFirstLetterRule->mSheet = nsnull;
    NS_RELEASE(mFirstLetterRule);
  }
  return NS_OK;
}

NS_IMETHODIMP
HTMLCSSStyleSheetImpl::GetURL(nsIURI*& aURL) const
{
  NS_IF_ADDREF(mURL);
  aURL = mURL;
  return NS_OK;
}

NS_IMETHODIMP
HTMLCSSStyleSheetImpl::GetTitle(nsString& aTitle) const
{
  aTitle.AssignWithConversion("Internal HTML/CSS Style Sheet");
  return NS_OK;
}

NS_IMETHODIMP
HTMLCSSStyleSheetImpl::GetType(nsString& aType) const
{
  aType.AssignWithConversion("text/html");
  return NS_OK;
}

NS_IMETHODIMP
HTMLCSSStyleSheetImpl::GetMediumCount(PRInt32& aCount) const
{
  aCount = 0;
  return NS_OK;
}

NS_IMETHODIMP
HTMLCSSStyleSheetImpl::GetMediumAt(PRInt32 aIndex, nsIAtom*& aMedium) const
{
  aMedium = nsnull;
  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP_(PRBool)
HTMLCSSStyleSheetImpl::UseForMedium(nsIAtom* aMedium) const
{
  return PR_TRUE; // works for all media
}

NS_IMETHODIMP
HTMLCSSStyleSheetImpl::GetEnabled(PRBool& aEnabled) const
{
  aEnabled = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
HTMLCSSStyleSheetImpl::SetEnabled(PRBool aEnabled)
{ // these can't be disabled
  return NS_OK;
}

// style sheet owner info
NS_IMETHODIMP
HTMLCSSStyleSheetImpl::GetParentSheet(nsIStyleSheet*& aParent) const
{
  aParent = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
HTMLCSSStyleSheetImpl::GetOwningDocument(nsIDocument*& aDocument) const
{
  NS_IF_ADDREF(mDocument);
  aDocument = mDocument;
  return NS_OK;
}

NS_IMETHODIMP
HTMLCSSStyleSheetImpl::SetOwningDocument(nsIDocument* aDocument)
{
  mDocument = aDocument;
  return NS_OK;
}

#ifdef DEBUG
void HTMLCSSStyleSheetImpl::List(FILE* out, PRInt32 aIndent) const
{
  // Indent
  for (PRInt32 index = aIndent; --index >= 0; ) fputs("  ", out);

  fputs("HTML CSS Style Sheet: ", out);
  char* urlSpec = nsnull;
  mURL->GetSpec(&urlSpec);
  if (urlSpec) {
    fputs(urlSpec, out);
    nsCRT::free(urlSpec);
  }
  fputs("\n", out);
}

/******************************************************************************
* SizeOf method:
*
*  Self (reported as HTMLCSSStyleSheetImpl's size): 
*    1) sizeof(*this) 
*
*  Contained / Aggregated data (not reported as HTMLCSSStyleSheetImpl's size):
*    1) We don't really delegate but count seperately the FirstLineRule and 
*       the FirstLetterRule if the exist and are unique instances
*
*  Children / siblings / parents:
*    none
*    
******************************************************************************/
void HTMLCSSStyleSheetImpl::SizeOf(nsISizeOfHandler *aSizeOfHandler, PRUint32 &aSize)
{
  NS_ASSERTION(aSizeOfHandler != nsnull, "SizeOf handler cannot be null");

  // first get the unique items collection
  UNIQUE_STYLE_ITEMS(uniqueItems);
  if(! uniqueItems->AddItem((void*)this)){
    // this style sheet is lared accounted for
    return;
  }

  PRUint32 localSize=0;

  // create a tag for this instance
  nsCOMPtr<nsIAtom> tag;
  tag = getter_AddRefs(NS_NewAtom("HTMLCSSStyleSheet"));
  // get the size of an empty instance and add to the sizeof handler
  aSize = sizeof(HTMLCSSStyleSheetImpl);
  aSizeOfHandler->AddSize(tag,aSize);

  // Now the associated rules (if they exist)
  // - mFirstLineRule
  // - mFirstLetterRule
  if(mFirstLineRule && uniqueItems->AddItem((void*)mFirstLineRule)){
    localSize = sizeof(*mFirstLineRule);
    aSize += localSize;
    tag = getter_AddRefs(NS_NewAtom("FirstLineRule"));
    aSizeOfHandler->AddSize(tag,localSize);
  }
  if(mFirstLetterRule && uniqueItems->AddItem((void*)mFirstLetterRule)){
    localSize = sizeof(*mFirstLetterRule);
    aSize += localSize;
    tag = getter_AddRefs(NS_NewAtom("FirstLetterRule"));
    aSizeOfHandler->AddSize(tag,localSize);
  }
}
#endif

NS_IMETHODIMP
HTMLCSSStyleSheetImpl::AttributeAffectsStyle(nsIAtom *aAttribute,
                                             nsIContent *aContent,
                                             PRBool &aAffects)
{
  // XXX can attributes affect rules in these?
  aAffects = PR_FALSE;
  return NS_OK;
}

// XXX For backwards compatibility and convenience
NS_EXPORT nsresult
  NS_NewHTMLCSSStyleSheet(nsIHTMLCSSStyleSheet** aInstancePtrResult, nsIURI* aURL,
                          nsIDocument* aDocument)
{
  nsresult rv;
  nsIHTMLCSSStyleSheet* sheet;
  if (NS_FAILED(rv = NS_NewHTMLCSSStyleSheet(&sheet)))
    return rv;

  if (NS_FAILED(rv = sheet->Init(aURL, aDocument))) {
    NS_RELEASE(sheet);
    return rv;
  }

  *aInstancePtrResult = sheet;
  return NS_OK;
}

NS_EXPORT nsresult
  NS_NewHTMLCSSStyleSheet(nsIHTMLCSSStyleSheet** aInstancePtrResult)
{
  if (aInstancePtrResult == nsnull) {
    return NS_ERROR_NULL_POINTER;
  }

  HTMLCSSStyleSheetImpl*  it = new HTMLCSSStyleSheetImpl();

  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(it);
  *aInstancePtrResult = it;
  return NS_OK;
}
