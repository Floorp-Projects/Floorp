/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * This Original Code has been modified by IBM Corporation. Modifications made by IBM 
 * described herein are Copyright (c) International Business Machines Corporation, 2000.
 * Modifications to Mozilla code or documentation identified per MPL Section 3.3
 *
 * Date             Modified by     Description of modification
 * 04/20/2000       IBM Corp.      OS/2 VisualAge build.
 */
#include "nsINameSpaceManager.h"
#include "nsIHTMLStyleSheet.h"
#include "nsIArena.h"
#include "nsCRT.h"
#include "nsIAtom.h"
#include "nsIURL.h"
#include "nsIServiceManager.h"
#include "nsISupportsArray.h"
#include "nsHashtable.h"
#include "nsIHTMLContent.h"
#include "nsIHTMLAttributes.h"
#include "nsILink.h"
#include "nsStyleUtil.h"
#include "nsIStyleRuleProcessor.h"
#include "nsIStyleRule.h"
#include "nsIFrame.h"
#include "nsIStyleContext.h"
#include "nsHTMLAtoms.h"
#include "nsIPresContext.h"
#include "nsIEventStateManager.h"
#include "nsIDocument.h"
#include "nsHTMLIIDs.h"
#include "nsICSSFrameConstructor.h"
#include "nsIStyleFrameConstruction.h"
#include "nsHTMLParts.h"
#include "nsIPresShell.h"
#include "nsStyleConsts.h"
#include "nsINameSpaceManager.h"
#include "nsLayoutAtoms.h"
#include "nsContentCID.h"
#include "nsLayoutCID.h"

#include "nsRuleWalker.h"

#include "nsIStyleSet.h"
#include "nsISizeOfHandler.h"

static NS_DEFINE_IID(kIStyleFrameConstructionIID, NS_ISTYLE_FRAME_CONSTRUCTION_IID);
static NS_DEFINE_CID(kHTMLAttributesCID, NS_HTMLATTRIBUTES_CID);
static NS_DEFINE_CID(kCSSFrameConstructorCID, NS_CSSFRAMECONSTRUCTOR_CID);

class HTMLColorRule : public nsIStyleRule {
public:
  HTMLColorRule(nsIHTMLStyleSheet* aSheet);
  virtual ~HTMLColorRule();

  NS_DECL_ISUPPORTS

  NS_IMETHOD Equals(const nsIStyleRule* aRule, PRBool& aValue) const;
  NS_IMETHOD HashValue(PRUint32& aValue) const;
  NS_IMETHOD GetStyleSheet(nsIStyleSheet*& aSheet) const;
  // Strength is an out-of-band weighting, always 0 here
  NS_IMETHOD GetStrength(PRInt32& aStrength) const;

  // The new mapping function.
  NS_IMETHOD MapRuleInfoInto(nsRuleData* aRuleData);

#ifdef DEBUG
  NS_IMETHOD List(FILE* out = stdout, PRInt32 aIndent = 0) const;

  virtual void SizeOf(nsISizeOfHandler *aSizeOfHandler, PRUint32 &aSize);
#endif

  nscolor             mColor;
  nsIHTMLStyleSheet*  mSheet;
};

class HTMLDocumentColorRule : public HTMLColorRule {
public:
  HTMLDocumentColorRule(nsIHTMLStyleSheet* aSheet);
  virtual ~HTMLDocumentColorRule();

  NS_IMETHOD MapRuleInfoInto(nsRuleData* aRuleData);

#ifdef DEBUG
  virtual void SizeOf(nsISizeOfHandler *aSizeofHandler, PRUint32 &aSize);
#endif

  void Reset() {
    mForegroundSet = PR_FALSE;
    mBackgroundSet = PR_FALSE;
  }

  nscolor mBackgroundColor;
  PRBool mForegroundSet;
  PRBool mBackgroundSet;
};

HTMLColorRule::HTMLColorRule(nsIHTMLStyleSheet* aSheet)
  : mSheet(aSheet)
{
  NS_INIT_REFCNT();
}

HTMLColorRule::~HTMLColorRule()
{
}

NS_IMPL_ISUPPORTS1(HTMLColorRule, nsIStyleRule)

NS_IMETHODIMP
HTMLColorRule::Equals(const nsIStyleRule* aRule, PRBool& aResult) const
{
  aResult = PRBool(this == aRule);
  return NS_OK;
}

NS_IMETHODIMP
HTMLColorRule::HashValue(PRUint32& aValue) const
{
  aValue = (PRUint32)(mColor);
  return NS_OK;
}

NS_IMETHODIMP
HTMLColorRule::GetStyleSheet(nsIStyleSheet*& aSheet) const
{
  NS_IF_ADDREF(mSheet);
  aSheet = mSheet;
  return NS_OK;
}

// Strength is an out-of-band weighting, always 0 here
NS_IMETHODIMP
HTMLColorRule::GetStrength(PRInt32& aStrength) const
{
  aStrength = 0;
  return NS_OK;
}

NS_IMETHODIMP
HTMLColorRule::MapRuleInfoInto(nsRuleData* aRuleData)
{
  if (aRuleData->mSID == eStyleStruct_Color && aRuleData->mColorData) {
    if (aRuleData->mColorData->mColor.GetUnit() == eCSSUnit_Null)
      aRuleData->mColorData->mColor = nsCSSValue(mColor);
  }
  return NS_OK;
}

#ifdef DEBUG
NS_IMETHODIMP
HTMLColorRule::List(FILE* out, PRInt32 aIndent) const
{
  return NS_OK;
}

/******************************************************************************
* SizeOf method:
*
*  Self (reported as HTMLColorRule's size): 
*    1) sizeof(*this) + 
*
*  Contained / Aggregated data (not reported as HTMLColorRule's size):
*    1) delegate to the mSheet
*
*  Children / siblings / parents:
*    none
*    
******************************************************************************/
void HTMLColorRule::SizeOf(nsISizeOfHandler *aSizeOfHandler, PRUint32 &aSize)
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
  tag = getter_AddRefs(NS_NewAtom("HTMLColorRule"));
  // get the size of an empty instance and add to the sizeof handler
  aSize = sizeof(*this);
  aSizeOfHandler->AddSize(tag,aSize);

  if(mSheet){
    PRUint32 localSize=0;
    mSheet->SizeOf(aSizeOfHandler, localSize);
  }
}
#endif

HTMLDocumentColorRule::HTMLDocumentColorRule(nsIHTMLStyleSheet* aSheet) 
  : HTMLColorRule(aSheet)
{
  Reset();
}

HTMLDocumentColorRule::~HTMLDocumentColorRule()
{
}

NS_IMETHODIMP
HTMLDocumentColorRule::MapRuleInfoInto(nsRuleData* aRuleData)
{
  if (mForegroundSet && aRuleData->mSID == eStyleStruct_Color && aRuleData->mColorData) {
    nsCSSValue val; val.SetColorValue(mColor);
    aRuleData->mColorData->mColor = val;
  }
  else if (mBackgroundSet && aRuleData->mSID == eStyleStruct_Background && aRuleData->mColorData) {
    nsCSSValue val; val.SetColorValue(mBackgroundColor);
    aRuleData->mColorData->mBackColor = val;
  }
  return NS_OK;
}

#ifdef DEBUG
/******************************************************************************
* SizeOf method:
*
*  Self (reported as HTMLDocumentColorRule's size): 
*    1) sizeof(*this)
*
*  Contained / Aggregated data (not reported as HTMLDocumentColorRule's size):
*    1) Delegate to the mSheet
*
*  Children / siblings / parents:
*    none
*    
******************************************************************************/
void HTMLDocumentColorRule::SizeOf(nsISizeOfHandler *aSizeOfHandler, PRUint32 &aSize)
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
  tag = getter_AddRefs(NS_NewAtom("HTMLDocumentColorRule"));
  // get the size of an empty instance and add to the sizeof handler
  aSize = sizeof(*this);
  aSizeOfHandler->AddSize(tag,aSize);

  if(mSheet){
    PRUint32 localSize;
    mSheet->SizeOf(aSizeOfHandler, localSize);
  }
}
#endif

class GenericTableRule: public nsIStyleRule {
public:
  GenericTableRule(nsIHTMLStyleSheet* aSheet);
  virtual ~GenericTableRule();

  NS_DECL_ISUPPORTS

  NS_IMETHOD Equals(const nsIStyleRule* aRule, PRBool& aResult) const;
  NS_IMETHOD HashValue(PRUint32& aValue) const;
  NS_IMETHOD GetStyleSheet(nsIStyleSheet*& aSheet) const;

  // Strength is an out-of-band weighting, always 0 here
  NS_IMETHOD GetStrength(PRInt32& aStrength) const;

  // The new mapping function.
  NS_IMETHOD MapRuleInfoInto(nsRuleData* aRuleData);

#ifdef DEBUG
  NS_IMETHOD List(FILE* out = stdout, PRInt32 aIndent = 0) const;

  virtual void SizeOf(nsISizeOfHandler *aSizeofHandler, PRUint32 &aSize);
#endif

  void Reset()
  {
  }

  nsIHTMLStyleSheet*  mSheet; // not ref-counted, cleared by content
};

GenericTableRule::GenericTableRule(nsIHTMLStyleSheet* aSheet)
{
  NS_INIT_REFCNT();
  mSheet = aSheet;
}

GenericTableRule::~GenericTableRule()
{
}

NS_IMPL_ISUPPORTS1(GenericTableRule, nsIStyleRule)

NS_IMETHODIMP
GenericTableRule::Equals(const nsIStyleRule* aRule, PRBool& aResult) const
{
  aResult = PRBool(this == aRule);
  return NS_OK;
}

NS_IMETHODIMP
GenericTableRule::HashValue(PRUint32& aValue) const
{
  aValue = 0;
  return NS_OK;
}

NS_IMETHODIMP
GenericTableRule::GetStyleSheet(nsIStyleSheet*& aSheet) const
{
  aSheet = mSheet;
  return NS_OK;
}

// Strength is an out-of-band weighting, useful for mapping CSS ! important
// always 0 here
NS_IMETHODIMP
GenericTableRule::GetStrength(PRInt32& aStrength) const
{
  aStrength = 0;
  return NS_OK;
}

NS_IMETHODIMP
GenericTableRule::MapRuleInfoInto(nsRuleData* aRuleData)
{
  // Nothing to do.
  return NS_OK;
}

#ifdef DEBUG
NS_IMETHODIMP
GenericTableRule::List(FILE* out, PRInt32 aIndent) const
{
  return NS_OK;
}

/******************************************************************************
* SizeOf method:
*
*  Self (reported as GenericTableRule's size): 
*    1) sizeof(*this) + 
*
*  Contained / Aggregated data (not reported as GenericTableRule's size):
*    1) Delegate to the mSheet if it exists
*
*  Children / siblings / parents:
*    none
*    
******************************************************************************/
void GenericTableRule::SizeOf(nsISizeOfHandler *aSizeOfHandler, PRUint32 &aSize)
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
  tag = getter_AddRefs(NS_NewAtom("GenericTableRule"));
  // get the size of an empty instance and add to the sizeof handler
  aSize = sizeof(*this);
  aSizeOfHandler->AddSize(tag,aSize);

  if(mSheet){
    PRUint32 localSize;
    mSheet->SizeOf(aSizeOfHandler, localSize);
  }
}
#endif

// -----------------------------------------------------------
// this rule handles <th> inheritance
// -----------------------------------------------------------
class TableTHRule: public GenericTableRule {
public:
  TableTHRule(nsIHTMLStyleSheet* aSheet);
  virtual ~TableTHRule();

  void Reset()
  {
    GenericTableRule::Reset();
  }

  NS_IMETHOD MapRuleInfoInto(nsRuleData* aRuleData);
};

TableTHRule::TableTHRule(nsIHTMLStyleSheet* aSheet)
: GenericTableRule(aSheet)
{
}

TableTHRule::~TableTHRule()
{
}

static void PostResolveCallback(nsStyleStruct* aStyleStruct, nsRuleData* aRuleData)
{
  nsStyleText* text = (nsStyleText*)aStyleStruct;
  if (text->mTextAlign == NS_STYLE_TEXT_ALIGN_DEFAULT) {
    nsCOMPtr<nsIStyleContext> parentContext = getter_AddRefs(aRuleData->mStyleContext->GetParent());

    if (parentContext) {
      const nsStyleText* parentStyleText = 
          (const nsStyleText*)parentContext->GetStyleData(eStyleStruct_Text);
      PRUint8 parentAlign = parentStyleText->mTextAlign;
      text->mTextAlign = (NS_STYLE_TEXT_ALIGN_DEFAULT == parentAlign)
                              ? NS_STYLE_TEXT_ALIGN_CENTER : parentAlign;
    }
  }
}

NS_IMETHODIMP
TableTHRule::MapRuleInfoInto(nsRuleData* aRuleData)
{
  if (aRuleData && aRuleData->mSID == eStyleStruct_Text) {
    aRuleData->mCanStoreInRuleTree = PR_FALSE;
    aRuleData->mPostResolveCallback = &PostResolveCallback;
  }
  return NS_OK;
}

// -----------------------------------------------------------

class AttributeKey: public nsHashKey
{
public:
  AttributeKey(nsIHTMLMappedAttributes* aAttributes);
  virtual ~AttributeKey(void);

  PRBool      Equals(const nsHashKey* aOther) const;
  PRUint32    HashCode(void) const;
  nsHashKey*  Clone(void) const;

private:
  AttributeKey(void);
  AttributeKey(const AttributeKey& aCopy);
  AttributeKey& operator=(const AttributeKey& aCopy);

public:
  nsIHTMLMappedAttributes*  mAttributes;
  union {
    struct {
      PRUint32  mHashSet: 1;
      PRUint32  mHashCode: 31;
    } mBits;
    PRUint32    mInitializer;
  } mHash;
};

MOZ_DECL_CTOR_COUNTER(AttributeKey)

AttributeKey::AttributeKey(nsIHTMLMappedAttributes* aAttributes)
  : mAttributes(aAttributes)
{
  MOZ_COUNT_CTOR(AttributeKey);
  NS_ADDREF(mAttributes);
  mHash.mInitializer = 0;
}

AttributeKey::~AttributeKey(void)
{
  MOZ_COUNT_DTOR(AttributeKey);
  NS_RELEASE(mAttributes);
}

PRBool AttributeKey::Equals(const nsHashKey* aOther) const
{
  const AttributeKey* other = (const AttributeKey*)aOther;
  PRBool  equals;
  mAttributes->Equals(other->mAttributes, equals);
  return equals;
}

PRUint32 AttributeKey::HashCode(void) const
{
  if (0 == mHash.mBits.mHashSet) {
    AttributeKey* self = (AttributeKey*)this; // break const
    PRUint32  hash;
    mAttributes->HashValue(hash);
    self->mHash.mBits.mHashCode = (0x7FFFFFFF & hash);
    self->mHash.mBits.mHashSet = 1;
  }
  return mHash.mBits.mHashCode;
}

nsHashKey* AttributeKey::Clone(void) const
{
  AttributeKey* clown = new AttributeKey(mAttributes);
  if (nsnull != clown) {
    clown->mHash.mInitializer = mHash.mInitializer;
  }
  return clown;
}

// -----------------------------------------------------------

class HTMLStyleSheetImpl : public nsIHTMLStyleSheet,
                           public nsIStyleRuleProcessor {
public:
  void* operator new(size_t size);
  void* operator new(size_t size, nsIArena* aArena);
  void operator delete(void* ptr);

  HTMLStyleSheetImpl(void);
  nsresult Init();

  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);
  NS_IMETHOD_(nsrefcnt) AddRef();
  NS_IMETHOD_(nsrefcnt) Release();

  // nsIStyleSheet api
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

  NS_IMETHOD SetOwningDocument(nsIDocument* aDocumemt);

  NS_IMETHOD GetStyleRuleProcessor(nsIStyleRuleProcessor*& aProcessor,
                                   nsIStyleRuleProcessor* aPrevProcessor);

  // nsIStyleRuleProcessor API
  NS_IMETHOD RulesMatching(nsIPresContext* aPresContext,
                           nsIAtom* aMedium,
                           nsIContent* aContent,
                           nsIStyleContext* aParentContext,
                           nsRuleWalker* aRuleWalker);

  NS_IMETHOD RulesMatching(nsIPresContext* aPresContext,
                           nsIAtom* aMedium,
                           nsIContent* aParentContent,
                           nsIAtom* aPseudoTag,
                           nsIStyleContext* aParentContext,
                           nsICSSPseudoComparator* aComparator,
                           nsRuleWalker* aRuleWalker);

  NS_IMETHOD HasStateDependentStyle(nsIPresContext* aPresContext,
                                    nsIAtom*        aMedium,
                                    nsIContent*     aContent);

  // nsIHTMLStyleSheet api
  NS_IMETHOD Init(nsIURI* aURL, nsIDocument* aDocument);
  NS_IMETHOD Reset(nsIURI* aURL);
  NS_IMETHOD GetLinkColor(nscolor& aColor);
  NS_IMETHOD GetActiveLinkColor(nscolor& aColor);
  NS_IMETHOD GetVisitedLinkColor(nscolor& aColor);
  NS_IMETHOD GetDocumentForegroundColor(nscolor& aColor);
  NS_IMETHOD GetDocumentBackgroundColor(nscolor& aColor);
  NS_IMETHOD SetLinkColor(nscolor aColor);
  NS_IMETHOD SetActiveLinkColor(nscolor aColor);
  NS_IMETHOD SetVisitedLinkColor(nscolor aColor);
  NS_IMETHOD SetDocumentForegroundColor(nscolor aColor);
  NS_IMETHOD SetDocumentBackgroundColor(nscolor aColor);

  // Attribute management methods, aAttributes is an in/out param
  NS_IMETHOD SetAttributesFor(nsIHTMLContent* aContent, 
                              nsIHTMLAttributes*& aAttributes);
  NS_IMETHOD SetAttributeFor(nsIAtom* aAttribute,
                             const nsAReadableString& aValue, 
                             PRBool aMappedToStyle,
                             nsIHTMLContent* aContent, 
                             nsIHTMLAttributes*& aAttributes);
  NS_IMETHOD SetAttributeFor(nsIAtom* aAttribute, const nsHTMLValue& aValue, 
                             PRBool aMappedToStyle,
                             nsIHTMLContent* aContent, 
                             nsIHTMLAttributes*& aAttributes);
  NS_IMETHOD UnsetAttributeFor(nsIAtom* aAttribute, nsIHTMLContent* aContent, 
                               nsIHTMLAttributes*& aAttributes);

  // Mapped Attribute management methods
  NS_IMETHOD UniqueMappedAttributes(nsIHTMLMappedAttributes* aMapped,
                                    nsIHTMLMappedAttributes*& aUniqueMapped);
  NS_IMETHOD DropMappedAttributes(nsIHTMLMappedAttributes* aMapped);

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
  HTMLStyleSheetImpl(const HTMLStyleSheetImpl& aCopy); 
  HTMLStyleSheetImpl& operator=(const HTMLStyleSheetImpl& aCopy); 

protected:
  virtual ~HTMLStyleSheetImpl();

protected:
  PRUint32 mInHeap : 1;
  PRUint32 mRefCnt : 31;
  NS_DECL_OWNINGTHREAD // for thread-safety checking

  nsIURI*              mURL;
  nsIDocument*         mDocument;
  HTMLColorRule*       mLinkRule;
  HTMLColorRule*       mVisitedRule;
  HTMLColorRule*       mActiveRule;
  HTMLDocumentColorRule* mDocumentColorRule;
  TableTHRule*         mTableTHRule;
    // NOTE: if adding more rules, be sure to update 
    // the SizeOf method to include them

  nsHashtable          mMappedAttrTable;
};


void* HTMLStyleSheetImpl::operator new(size_t size)
{
  HTMLStyleSheetImpl* rv = (HTMLStyleSheetImpl*) ::operator new(size);
#ifdef NS_DEBUG
  if (nsnull != rv) {
    nsCRT::memset(rv, 0xEE, size);
  }
#endif
  rv->mInHeap = 1;
  return (void*) rv;
}

void* HTMLStyleSheetImpl::operator new(size_t size, nsIArena* aArena)
{
  HTMLStyleSheetImpl* rv = (HTMLStyleSheetImpl*) aArena->Alloc(PRInt32(size));
#ifdef NS_DEBUG
  if (nsnull != rv) {
    nsCRT::memset(rv, 0xEE, size);
  }
#endif
  rv->mInHeap = 0;
  return (void*) rv;
}

void HTMLStyleSheetImpl::operator delete(void* ptr)
{
  HTMLStyleSheetImpl* sheet = (HTMLStyleSheetImpl*) ptr;
  if (nsnull != sheet) {
    if (sheet->mInHeap) {
      ::operator delete(ptr);
    }
  }
}

HTMLStyleSheetImpl::HTMLStyleSheetImpl(void)
  : nsIHTMLStyleSheet(),
    mURL(nsnull),
    mDocument(nsnull),
    mLinkRule(nsnull),
    mVisitedRule(nsnull),
    mActiveRule(nsnull),
    mDocumentColorRule(nsnull)
{
  NS_INIT_REFCNT();
}

nsresult
HTMLStyleSheetImpl::Init()
{
  mTableTHRule = new TableTHRule(this);
  if (!mTableTHRule)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(mTableTHRule);

  mDocumentColorRule = new HTMLDocumentColorRule(this);
  if (!mDocumentColorRule)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(mDocumentColorRule);

  return NS_OK;
}

PRBool PR_CALLBACK MappedDropSheet(nsHashKey *aKey, void *aData, void* closure)
{
  nsIHTMLMappedAttributes* mapped = (nsIHTMLMappedAttributes*)aData;
  mapped->DropStyleSheetReference();
  return PR_TRUE;
}

HTMLStyleSheetImpl::~HTMLStyleSheetImpl()
{
  NS_IF_RELEASE(mURL);
  if (nsnull != mLinkRule) {
    mLinkRule->mSheet = nsnull;
    NS_RELEASE(mLinkRule);
  }
  if (nsnull != mVisitedRule) {
    mVisitedRule->mSheet = nsnull;
    NS_RELEASE(mVisitedRule);
  }
  if (nsnull != mActiveRule) {
    mActiveRule->mSheet = nsnull;
    NS_RELEASE(mActiveRule);
  }
  if (nsnull != mDocumentColorRule) {
    mDocumentColorRule->mSheet = nsnull;
    NS_RELEASE(mDocumentColorRule);
  }
  if (nsnull != mTableTHRule) {
    mTableTHRule->mSheet = nsnull;
    NS_RELEASE(mTableTHRule);
  }
  mMappedAttrTable.Enumerate(MappedDropSheet);
}

NS_IMPL_ADDREF(HTMLStyleSheetImpl)
NS_IMPL_RELEASE(HTMLStyleSheetImpl)

nsresult HTMLStyleSheetImpl::QueryInterface(const nsIID& aIID,
                                            void** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null pointer");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
  if (aIID.Equals(NS_GET_IID(nsIHTMLStyleSheet))) {
    *aInstancePtrResult = (void*) ((nsIHTMLStyleSheet*)this);
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
  if (aIID.Equals(kIStyleFrameConstructionIID)) {
    // XXX this breaks XPCOM rules since it isn't a proper delegate
    // This is a temporary method of connecting the constructor for now
    nsresult result;
    nsCOMPtr<nsICSSFrameConstructor> constructor(do_CreateInstance(kCSSFrameConstructorCID,&result));
    if (NS_SUCCEEDED(result)) {
      result = constructor->Init(mDocument);
      if (NS_SUCCEEDED(result)) {
        result = constructor->QueryInterface(aIID, aInstancePtrResult);
      }
    }
    return result;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtrResult = (void*) this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

NS_IMETHODIMP
HTMLStyleSheetImpl::GetStyleRuleProcessor(nsIStyleRuleProcessor*& aProcessor,
                                          nsIStyleRuleProcessor* /*aPrevProcessor*/)
{
  aProcessor = this;
  NS_ADDREF(aProcessor);
  return NS_OK;
}

NS_IMETHODIMP
HTMLStyleSheetImpl::RulesMatching(nsIPresContext* aPresContext,
                                  nsIAtom* aMedium,
                                  nsIContent* aContent,
                                  nsIStyleContext* aParentContext,
                                  nsRuleWalker* aRuleWalker)
{
  NS_PRECONDITION(nsnull != aPresContext, "null arg");
  NS_PRECONDITION(nsnull != aContent, "null arg");
  NS_PRECONDITION(nsnull != aRuleWalker, "null arg");

  nsIStyledContent* styledContent;
  if (NS_SUCCEEDED(aContent->QueryInterface(NS_GET_IID(nsIStyledContent), (void**)&styledContent))) {
    PRInt32 nameSpace;
    styledContent->GetNameSpaceID(nameSpace);
    if (kNameSpaceID_HTML == nameSpace) {
      nsIAtom*  tag;
      styledContent->GetTag(tag);
      // if we have anchor colors, check if this is an anchor with an href
      if (tag == nsHTMLAtoms::a) {
        if (mLinkRule || mVisitedRule || mActiveRule) {
          nsLinkState linkState;
          if (nsStyleUtil::IsHTMLLink(aContent, tag, aPresContext, &linkState)) {
            switch (linkState) {
              case eLinkState_Unvisited:
                if (mLinkRule)
                  aRuleWalker->Forward(mLinkRule);
                break;
              case eLinkState_Visited:
                if (mVisitedRule)
                  aRuleWalker->Forward(mVisitedRule);
                break;
              default:
                break;
            }
            // No need to add to the active rule if it's not a link
            if (mActiveRule) {  // test active state of link
              nsIEventStateManager* eventStateManager;

              if ((NS_OK == aPresContext->GetEventStateManager(&eventStateManager)) &&
                  (nsnull != eventStateManager)) {
                PRInt32 state;
                if (NS_OK == eventStateManager->GetContentState(aContent, state)) {
                  if (state & NS_EVENT_STATE_ACTIVE)
                    aRuleWalker->Forward(mActiveRule);
                }
                NS_RELEASE(eventStateManager);
              }
            } // end active rule
          }
        } // end link/visited/active rules
      } // end A tag
      // add the rule to handle text-align for a <th>
      else if  (tag == nsHTMLAtoms::th) {
          aRuleWalker->Forward(mTableTHRule);
      }
      else if (tag == nsHTMLAtoms::table) {
        nsCompatibility mode;
        aPresContext->GetCompatibilityMode(&mode);
        if (eCompatibility_NavQuirks == mode) {
          aRuleWalker->Forward(mDocumentColorRule);
        }
      }
      else if (tag == nsHTMLAtoms::html) {
        aRuleWalker->Forward(mDocumentColorRule);
      }
      NS_IF_RELEASE(tag);
    } // end html namespace

    // just get the style rules from the content
    styledContent->WalkContentStyleRules(aRuleWalker);

    NS_RELEASE(styledContent);
  }

  return NS_OK;
}

// Test if style is dependent on content state
NS_IMETHODIMP
HTMLStyleSheetImpl::HasStateDependentStyle(nsIPresContext* aPresContext,
                                           nsIAtom*        aMedium,
                                           nsIContent*     aContent)
{
  nsresult result = NS_COMFALSE;

  if (mActiveRule || mLinkRule || mVisitedRule) { // do we have any rules dependent on state?
    nsIStyledContent* styledContent;
    if (NS_SUCCEEDED(aContent->QueryInterface(NS_GET_IID(nsIStyledContent), (void**)&styledContent))) {
      PRInt32 nameSpace;
      styledContent->GetNameSpaceID(nameSpace);
      if (kNameSpaceID_HTML == nameSpace) {
        nsIAtom*  tag;
        styledContent->GetTag(tag);
        // if we have anchor colors, check if this is an anchor with an href
        if (tag == nsHTMLAtoms::a) {
          nsAutoString href;
          nsresult attrState = styledContent->GetAttr(kNameSpaceID_None, nsHTMLAtoms::href, href);
          if (NS_CONTENT_ATTR_HAS_VALUE == attrState) {
            result = NS_OK; // yes, style will depend on link state
          }
        }
        NS_IF_RELEASE(tag);
      }
      NS_RELEASE(styledContent);
    }
  }
  return result;
}



NS_IMETHODIMP
HTMLStyleSheetImpl::RulesMatching(nsIPresContext* aPresContext,
                                  nsIAtom* aMedium,
                                  nsIContent* aParentContent,
                                  nsIAtom* aPseudoTag,
                                  nsIStyleContext* aParentContext,
                                  nsICSSPseudoComparator* aComparator,
                                  nsRuleWalker* aRuleWalker)
{
  // no pseudo frame style
  return NS_OK;
}


  // nsIStyleSheet api
NS_IMETHODIMP
HTMLStyleSheetImpl::GetURL(nsIURI*& aURL) const
{
  NS_IF_ADDREF(mURL);
  aURL = mURL;
  return NS_OK;
}

NS_IMETHODIMP
HTMLStyleSheetImpl::GetTitle(nsString& aTitle) const
{
  aTitle.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
HTMLStyleSheetImpl::GetType(nsString& aType) const
{
  aType.AssignWithConversion("text/html");
  return NS_OK;
}

NS_IMETHODIMP
HTMLStyleSheetImpl::GetMediumCount(PRInt32& aCount) const
{
  aCount = 0;
  return NS_OK;
}

NS_IMETHODIMP
HTMLStyleSheetImpl::GetMediumAt(PRInt32 aIndex, nsIAtom*& aMedium) const
{
  aMedium = nsnull;
  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP_(PRBool)
HTMLStyleSheetImpl::UseForMedium(nsIAtom* aMedium) const
{
  return PR_TRUE; // works for all media
}


NS_IMETHODIMP
HTMLStyleSheetImpl::GetEnabled(PRBool& aEnabled) const
{
  aEnabled = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
HTMLStyleSheetImpl::SetEnabled(PRBool aEnabled)
{ // these can't be disabled
  return NS_OK;
}

NS_IMETHODIMP
HTMLStyleSheetImpl::GetParentSheet(nsIStyleSheet*& aParent) const
{
  aParent = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
HTMLStyleSheetImpl::GetOwningDocument(nsIDocument*& aDocument) const
{
  NS_IF_ADDREF(mDocument);
  aDocument = mDocument;
  return NS_OK;
}

NS_IMETHODIMP
HTMLStyleSheetImpl::SetOwningDocument(nsIDocument* aDocument)
{
  mDocument = aDocument; // not refcounted
  return NS_OK;
}

NS_IMETHODIMP HTMLStyleSheetImpl::Init(nsIURI* aURL, nsIDocument* aDocument)
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

NS_IMETHODIMP HTMLStyleSheetImpl::Reset(nsIURI* aURL)
{
  NS_IF_RELEASE(mURL);
  mURL = aURL;
  NS_ADDREF(mURL);
  
  if (nsnull != mLinkRule) {
    mLinkRule->mSheet = nsnull;
    NS_RELEASE(mLinkRule);
  }
  if (nsnull != mVisitedRule) {
    mVisitedRule->mSheet = nsnull;
    NS_RELEASE(mVisitedRule);
  }
  if (nsnull != mActiveRule) {
    mActiveRule->mSheet = nsnull;
    NS_RELEASE(mActiveRule);
  }
  mDocumentColorRule->Reset();
  mTableTHRule->Reset();

  mMappedAttrTable.Enumerate(MappedDropSheet);
  mMappedAttrTable.Reset();

  return NS_OK;
}

NS_IMETHODIMP HTMLStyleSheetImpl::GetLinkColor(nscolor& aColor)
{
  if (nsnull == mLinkRule) {
    return NS_HTML_STYLE_PROPERTY_NOT_THERE;
  }
  else {
    aColor = mLinkRule->mColor;
    return NS_OK;
  }
}

NS_IMETHODIMP HTMLStyleSheetImpl::GetActiveLinkColor(nscolor& aColor)
{
  if (nsnull == mActiveRule) {
    return NS_HTML_STYLE_PROPERTY_NOT_THERE;
  }
  else {
    aColor = mActiveRule->mColor;
    return NS_OK;
  }
}

NS_IMETHODIMP HTMLStyleSheetImpl::GetVisitedLinkColor(nscolor& aColor)
{
  if (nsnull == mVisitedRule) {
    return NS_HTML_STYLE_PROPERTY_NOT_THERE;
  }
  else {
    aColor = mVisitedRule->mColor;
    return NS_OK;
  }
}

NS_IMETHODIMP HTMLStyleSheetImpl::GetDocumentForegroundColor(nscolor& aColor)
{
  if (!mDocumentColorRule->mForegroundSet)
    return NS_HTML_STYLE_PROPERTY_NOT_THERE;

  aColor = mDocumentColorRule->mColor;
  return NS_OK;
}

NS_IMETHODIMP HTMLStyleSheetImpl::GetDocumentBackgroundColor(nscolor& aColor)
{
  if (!mDocumentColorRule->mBackgroundSet)
    return NS_HTML_STYLE_PROPERTY_NOT_THERE;

  aColor = mDocumentColorRule->mBackgroundColor;
  return NS_OK;
}

NS_IMETHODIMP HTMLStyleSheetImpl::SetLinkColor(nscolor aColor)
{
  if (nsnull == mLinkRule) {
    mLinkRule = new HTMLColorRule(this);
    if (nsnull == mLinkRule) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    NS_ADDREF(mLinkRule);
  }
  mLinkRule->mColor = aColor;
  return NS_OK;
}


NS_IMETHODIMP HTMLStyleSheetImpl::SetActiveLinkColor(nscolor aColor)
{
  if (nsnull == mActiveRule) {
    mActiveRule = new HTMLColorRule(this);
    if (nsnull == mActiveRule) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    NS_ADDREF(mActiveRule);
  }
  mActiveRule->mColor = aColor;
  return NS_OK;
}

NS_IMETHODIMP HTMLStyleSheetImpl::SetVisitedLinkColor(nscolor aColor)
{
  if (nsnull == mVisitedRule) {
    mVisitedRule = new HTMLColorRule(this);
    if (nsnull == mVisitedRule) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    NS_ADDREF(mVisitedRule);
  }
  mVisitedRule->mColor = aColor;
  return NS_OK;
}

NS_IMETHODIMP HTMLStyleSheetImpl::SetDocumentForegroundColor(nscolor aColor)
{
  mDocumentColorRule->mColor = aColor;
  mDocumentColorRule->mForegroundSet = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP HTMLStyleSheetImpl::SetDocumentBackgroundColor(nscolor aColor)
{
  mDocumentColorRule->mBackgroundColor = aColor;
  mDocumentColorRule->mBackgroundSet = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP HTMLStyleSheetImpl::SetAttributesFor(nsIHTMLContent* aContent, nsIHTMLAttributes*& aAttributes)
{
  if (aAttributes) {
    aAttributes->SetStyleSheet(this);
  }
  return NS_OK;
}

NS_IMETHODIMP HTMLStyleSheetImpl::SetAttributeFor(nsIAtom* aAttribute, 
                                                  const nsAReadableString& aValue,
                                                  PRBool aMappedToStyle,
                                                  nsIHTMLContent* aContent, 
                                                  nsIHTMLAttributes*& aAttributes)
{
  nsresult  result = NS_OK;

  if (! aAttributes) {
    result = nsComponentManager::CreateInstance(kHTMLAttributesCID, nsnull,
                                                NS_GET_IID(nsIHTMLAttributes),
                                                (void**)&aAttributes);
  }
  if (aAttributes) {
    result = aAttributes->SetAttributeFor(aAttribute, aValue, aMappedToStyle, 
                                          aContent, this);
  }

  return result;
}

NS_IMETHODIMP HTMLStyleSheetImpl::SetAttributeFor(nsIAtom* aAttribute, 
                                                  const nsHTMLValue& aValue,
                                                  PRBool aMappedToStyle,
                                                  nsIHTMLContent* aContent, 
                                                  nsIHTMLAttributes*& aAttributes)
{
  nsresult  result = NS_OK;

  if ((! aAttributes) && (eHTMLUnit_Null != aValue.GetUnit())) {
    result = nsComponentManager::CreateInstance(kHTMLAttributesCID, nsnull,
                                                NS_GET_IID(nsIHTMLAttributes),
                                                (void**)&aAttributes);
  }
  if (aAttributes) {
    PRInt32 count;
    result = aAttributes->SetAttributeFor(aAttribute, aValue, aMappedToStyle,
                                          aContent, this, count);
    if (0 == count) {
      NS_RELEASE(aAttributes);
    }
  }

  return result;
}

NS_IMETHODIMP HTMLStyleSheetImpl::UnsetAttributeFor(nsIAtom* aAttribute,
                                                    nsIHTMLContent* aContent, 
                                                    nsIHTMLAttributes*& aAttributes)
{
  nsresult  result = NS_OK;

  if (aAttributes) {
    PRInt32 count;
    result = aAttributes->UnsetAttributeFor(aAttribute, aContent, this, count);
    if (0 == count) {
      NS_RELEASE(aAttributes);
    }
  }

  return result;

}

NS_IMETHODIMP
HTMLStyleSheetImpl::UniqueMappedAttributes(nsIHTMLMappedAttributes* aMapped,
                                           nsIHTMLMappedAttributes*& aUniqueMapped)
{
  nsresult result = NS_OK;

  AttributeKey  key(aMapped);
  nsIHTMLMappedAttributes* sharedAttrs = (nsIHTMLMappedAttributes*)mMappedAttrTable.Get(&key);
  if (nsnull == sharedAttrs) {  // we have a new unique set
    mMappedAttrTable.Put(&key, aMapped);
    aMapped->SetUniqued(PR_TRUE);
    NS_ADDREF(aMapped);
    aUniqueMapped = aMapped;
  }
  else {  // found existing set
    aUniqueMapped = sharedAttrs;
    NS_ADDREF(aUniqueMapped);
  }
  return result;
}

NS_IMETHODIMP
HTMLStyleSheetImpl::DropMappedAttributes(nsIHTMLMappedAttributes* aMapped)
{
  if (aMapped) {
    PRBool inTable = PR_FALSE;
    aMapped->GetUniqued(inTable);
    if (inTable) {
      AttributeKey  key(aMapped);
      nsIHTMLMappedAttributes* old = (nsIHTMLMappedAttributes*)mMappedAttrTable.Remove(&key);
      NS_ASSERTION(old == aMapped, "not in table");
      aMapped->SetUniqued(PR_FALSE);
    }
  }
  return NS_OK;
}

#ifdef DEBUG
void HTMLStyleSheetImpl::List(FILE* out, PRInt32 aIndent) const
{
  // Indent
  for (PRInt32 index = aIndent; --index >= 0; ) fputs("  ", out);

  fputs("HTML Style Sheet: ", out);
  char* urlSpec = nsnull;
  mURL->GetSpec(&urlSpec);
  if (urlSpec) {
    fputs(urlSpec, out);
    nsCRT::free(urlSpec);
  }
  fputs("\n", out);
}


struct MappedAttributeSizeEnumData {
  MappedAttributeSizeEnumData(nsISizeOfHandler *aSizeOfHandler, 
                              nsUniqueStyleItems *aUniqueStyleItem)

  {
    aHandler = aSizeOfHandler;
    uniqueItems = aUniqueStyleItem;
  }
    // weak references all 'round

  nsISizeOfHandler  *aHandler;
  nsUniqueStyleItems *uniqueItems;
};

static
PRBool PR_CALLBACK MappedSizeAttributes(nsHashKey *aKey, void *aData, void* closure)
{
  MappedAttributeSizeEnumData *pData = (MappedAttributeSizeEnumData *)closure;
  NS_ASSERTION(pData,"null closure is not supported");
  nsIHTMLMappedAttributes* mapped = (nsIHTMLMappedAttributes*)aData;
  NS_ASSERTION(mapped, "null item in enumeration fcn is not supported");
  // if there is an attribute and it has not been counted, the get its size
  if(mapped){
    PRUint32 size=0;
    mapped->SizeOf(pData->aHandler, size);
  }  
  return PR_TRUE;
}


/******************************************************************************
* SizeOf method:
*
*  Self (reported as HTMLStyleSheetImpl's size): 
*    1) sizeof(*this)
*
*  Contained / Aggregated data (not reported as HTMLStyleSheetImpl's size):
*    1) Not really delegated, but counted seperately:
*       - mLinkRule
*       - mVisitedRule
*       - mActiveRule
*       - mDocumentColorRule
*       - mTableTHRule
*       - mMappedAttrTable
*    2) Delegates (really) to the MappedAttributes in the mMappedAttrTable
*
*  Children / siblings / parents:
*    none
*    
******************************************************************************/
void HTMLStyleSheetImpl::SizeOf(nsISizeOfHandler *aSizeOfHandler, PRUint32 &aSize)
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
  tag = getter_AddRefs(NS_NewAtom("HTMLStyleSheet"));
  // get the size of an empty instance and add to the sizeof handler
  aSize = sizeof(HTMLStyleSheetImpl);
  aSizeOfHandler->AddSize(tag,aSize);

  // now gather up the sizes of the data members
  // - mLinkRule : sizeof object
  // - mVisitedRule  : sizeof object
  // - mActiveRule  : sizeof object
  // - mDocumentColorRule  : sizeof object
  // - mTableTHRule : sizeof object
  // - mMappedAttrTable

  if(mLinkRule && uniqueItems->AddItem((void*)mLinkRule)){
    localSize = sizeof(*mLinkRule);
    tag = getter_AddRefs(NS_NewAtom("LinkRule"));
    aSizeOfHandler->AddSize(tag,localSize);
  }
  if(mVisitedRule && uniqueItems->AddItem((void*)mVisitedRule)){
    localSize = sizeof(*mVisitedRule);
    tag = getter_AddRefs(NS_NewAtom("VisitedRule"));
    aSizeOfHandler->AddSize(tag,localSize);
  }
  if(mActiveRule && uniqueItems->AddItem((void*)mActiveRule)){
    localSize = sizeof(*mActiveRule);
    tag = getter_AddRefs(NS_NewAtom("ActiveRule"));
    aSizeOfHandler->AddSize(tag,localSize);
  }
  if(uniqueItems->AddItem((void*)mDocumentColorRule)){
    localSize = sizeof(*mDocumentColorRule);
    tag = getter_AddRefs(NS_NewAtom("DocumentColorRule"));
    aSizeOfHandler->AddSize(tag,localSize);
  }
  if(uniqueItems->AddItem((void*)mTableTHRule)){
    localSize = sizeof(*mTableTHRule);
    tag = getter_AddRefs(NS_NewAtom("TableTHRule"));
    aSizeOfHandler->AddSize(tag,localSize);
  }
  
  // for the AttrTable it is kindof sleezy: 
  //  We want the hash table overhead as well as the entries it contains
  //
  //  we get the overall size of the hashtable, and if there are entries,
  //  we calculate a rough overhead estimate as:
  //   number of entries X sizeof each hash-entry 
  //   + the size of a hash table (see plhash.h and nsHashTable.h)
  //  then we add up the size of each unique attribute
  localSize = sizeof(mMappedAttrTable);
  if(mMappedAttrTable.Count() >0){
    localSize += sizeof(PLHashTable);
    localSize += mMappedAttrTable.Count() * sizeof(PLHashEntry);
  }
  tag = getter_AddRefs(NS_NewAtom("MappedAttrTable"));
  aSizeOfHandler->AddSize(tag,localSize);

  // now get each unique attribute
  MappedAttributeSizeEnumData sizeEnumData(aSizeOfHandler, uniqueItems);
  mMappedAttrTable.Enumerate(MappedSizeAttributes, &sizeEnumData);

  // that's it
}
#endif

NS_IMETHODIMP
HTMLStyleSheetImpl::AttributeAffectsStyle(nsIAtom *aAttribute,
                                          nsIContent *aContent,
                                          PRBool &aAffects)
{
  // XXX we should be checking to see if this is an href on an <A> being
  // XXX tweaked, in which case we really want to restyle
  aAffects = PR_FALSE;
  return NS_OK;
}

// XXX For convenience and backwards compatibility
NS_EXPORT nsresult
NS_NewHTMLStyleSheet(nsIHTMLStyleSheet** aInstancePtrResult, nsIURI* aURL, 
                     nsIDocument* aDocument)
{
  nsresult rv;
  nsIHTMLStyleSheet* sheet;
  if (NS_FAILED(rv = NS_NewHTMLStyleSheet(&sheet)))
    return rv;

  if (NS_FAILED(rv = sheet->Init(aURL, aDocument))) {
    NS_RELEASE(sheet);
    return rv;
  }

  *aInstancePtrResult = sheet;
  return NS_OK;
}


NS_EXPORT nsresult
NS_NewHTMLStyleSheet(nsIHTMLStyleSheet** aInstancePtrResult)
{
  NS_PRECONDITION(aInstancePtrResult, "null out param");

  HTMLStyleSheetImpl *it = new HTMLStyleSheetImpl();
  if (!it) {
    *aInstancePtrResult = nsnull;
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(it);
  nsresult rv = it->Init();
  if (NS_FAILED(rv))
    NS_RELEASE(it);

  *aInstancePtrResult = it; // NS_ADDREF above, or set to null by NS_RELEASE
  return rv;
}
