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
 */
#include "nsCOMPtr.h"
#include "nsIStyleSet.h"
#include "nsIStyleSheet.h"
#include "nsIStyleRuleProcessor.h"
#include "nsIStyleRule.h"
#include "nsIStyleContext.h"
#include "nsISupportsArray.h"
#include "nsIFrame.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIStyleFrameConstruction.h"
#include "nsLayoutAtoms.h"
#include "nsTimer.h"
#include "nsICSSStyleSheet.h"
#include "nsNetUtil.h"
#include "nsIStyleRuleSupplier.h"

#ifdef MOZ_PERF_METRICS
  #include "nsITimeRecorder.h"
  #define STYLESET_START_TIMER(a) \
    StartTimer(a)
  #define STYLESET_STOP_TIMER(a) \
    StopTimer(a)
#else
  #define STYLESET_START_TIMER(a) ((void)0)
  #define STYLESET_STOP_TIMER(a) ((void)0)
#endif

#include "nsISizeOfHandler.h"

static NS_DEFINE_IID(kIStyleSetIID, NS_ISTYLE_SET_IID);
static NS_DEFINE_IID(kIStyleFrameConstructionIID, NS_ISTYLE_FRAME_CONSTRUCTION_IID);

// XXX - fast cache cannot be used until we resolve problems with clients changing StyleContextData
//       behind our backs. GetMutableStyleData must be eliminated and callers converted over to
//       using Get/SetStyleData so we can re-evaluate the CRC and update the cache.
//       At that time we can utilize a fast cache with CRC-based first line lookup and get back
//       another 10% of the performance we lost in sharing style context data
//#define USE_FAST_CACHE

//#define DUMP_CACHE_STATS

class StyleSetImpl : public nsIStyleSet 
#ifdef MOZ_PERF_METRICS
                   , public nsITimeRecorder
#endif
{
public:
  StyleSetImpl();

  NS_DECL_ISUPPORTS

  virtual void AppendOverrideStyleSheet(nsIStyleSheet* aSheet);
  virtual void InsertOverrideStyleSheetAfter(nsIStyleSheet* aSheet,
                                             nsIStyleSheet* aAfterSheet);
  virtual void InsertOverrideStyleSheetBefore(nsIStyleSheet* aSheet,
                                              nsIStyleSheet* aBeforeSheet);
  virtual void RemoveOverrideStyleSheet(nsIStyleSheet* aSheet);
  virtual PRInt32 GetNumberOfOverrideStyleSheets();
  virtual nsIStyleSheet* GetOverrideStyleSheetAt(PRInt32 aIndex);

  virtual void AddDocStyleSheet(nsIStyleSheet* aSheet, nsIDocument* aDocument);
  virtual void RemoveDocStyleSheet(nsIStyleSheet* aSheet);
  virtual PRInt32 GetNumberOfDocStyleSheets();
  virtual nsIStyleSheet* GetDocStyleSheetAt(PRInt32 aIndex);

  virtual void AppendBackstopStyleSheet(nsIStyleSheet* aSheet);
  virtual void InsertBackstopStyleSheetAfter(nsIStyleSheet* aSheet,
                                             nsIStyleSheet* aAfterSheet);
  virtual void InsertBackstopStyleSheetBefore(nsIStyleSheet* aSheet,
                                              nsIStyleSheet* aBeforeSheet);
  virtual void RemoveBackstopStyleSheet(nsIStyleSheet* aSheet);
  virtual PRInt32 GetNumberOfBackstopStyleSheets();
  virtual nsIStyleSheet* GetBackstopStyleSheetAt(PRInt32 aIndex);
  virtual void ReplaceBackstopStyleSheets(nsISupportsArray* aNewSheets);
  
  NS_IMETHOD EnableQuirkStyleSheet(PRBool aEnable);

  NS_IMETHOD NotifyStyleSheetStateChanged(PRBool aDisabled);

  virtual nsIStyleContext* ResolveStyleFor(nsIPresContext* aPresContext,
                                           nsIContent* aContent,
                                           nsIStyleContext* aParentContext,
                                           PRBool aForceUnique = PR_FALSE);

  virtual nsIStyleContext* ResolvePseudoStyleFor(nsIPresContext* aPresContext,
                                                 nsIContent* aParentContent,
                                                 nsIAtom* aPseudoTag,
                                                 nsIStyleContext* aParentContext,
                                                 PRBool aForceUnique = PR_FALSE);

  virtual nsIStyleContext* ProbePseudoStyleFor(nsIPresContext* aPresContext,
                                               nsIContent* aParentContent,
                                               nsIAtom* aPseudoTag,
                                               nsIStyleContext* aParentContext,
                                               PRBool aForceUnique = PR_FALSE);

  NS_IMETHOD ReParentStyleContext(nsIPresContext* aPresContext,
                                  nsIStyleContext* aStyleContext, 
                                  nsIStyleContext* aNewParentContext,
                                  nsIStyleContext** aNewStyleContext);

  NS_IMETHOD  HasStateDependentStyle(nsIPresContext* aPresContext,
                                     nsIContent*     aContent);

  NS_IMETHOD ConstructRootFrame(nsIPresContext* aPresContext,
                                nsIContent*     aContent,
                                nsIFrame*&      aFrameSubTree);
  NS_IMETHOD ReconstructDocElementHierarchy(nsIPresContext* aPresContext);
  NS_IMETHOD ContentAppended(nsIPresContext* aPresContext,
                             nsIContent*     aContainer,
                             PRInt32         aNewIndexInContainer);
  NS_IMETHOD ContentInserted(nsIPresContext* aPresContext,
                             nsIContent*     aContainer,
                             nsIContent*     aChild,
                             PRInt32         aIndexInContainer);
  NS_IMETHOD ContentReplaced(nsIPresContext* aPresContext,
                             nsIContent*     aContainer,
                             nsIContent*     aOldChild,
                             nsIContent*     aNewChild,
                             PRInt32         aIndexInContainer);
  NS_IMETHOD ContentRemoved(nsIPresContext* aPresContext,
                            nsIContent*     aContainer,
                            nsIContent*     aChild,
                            PRInt32         aIndexInContainer);

  NS_IMETHOD ContentChanged(nsIPresContext*  aPresContext,
                            nsIContent* aContent,
                            nsISupports* aSubContent);
  NS_IMETHOD ContentStatesChanged(nsIPresContext* aPresContext, 
                                  nsIContent* aContent1,
                                  nsIContent* aContent2);
  NS_IMETHOD AttributeChanged(nsIPresContext*  aPresContext,
                              nsIContent* aChild,
                              PRInt32 aNameSpaceID,
                              nsIAtom* aAttribute,
                              PRInt32 aHint); // See nsStyleConsts fot hint values

  // xxx style rules enumeration

  // Style change notifications
  NS_IMETHOD StyleRuleChanged(nsIPresContext* aPresContext,
                              nsIStyleSheet* aStyleSheet,
                              nsIStyleRule* aStyleRule,
                              PRInt32 aHint); // See nsStyleConsts fot hint values
  NS_IMETHOD StyleRuleAdded(nsIPresContext* aPresContext,
                            nsIStyleSheet* aStyleSheet,
                            nsIStyleRule* aStyleRule);
  NS_IMETHOD StyleRuleRemoved(nsIPresContext* aPresContext,
                              nsIStyleSheet* aStyleSheet,
                              nsIStyleRule* aStyleRule);

  // Notification that we were unable to render a replaced element.
  NS_IMETHOD CantRenderReplacedElement(nsIPresContext* aPresContext,
                                       nsIFrame*       aFrame);
  
  // Request to create a continuing frame
  NS_IMETHOD CreateContinuingFrame(nsIPresContext* aPresContext,
                                   nsIFrame*       aFrame,
                                   nsIFrame*       aParentFrame,
                                   nsIFrame**      aContinuingFrame);
  
  // Request to find the primary frame associated with a given content object.
  // This is typically called by the pres shell when there is no mapping in
  // the pres shell hash table
  NS_IMETHOD FindPrimaryFrameFor(nsIPresContext*  aPresContext,
                                 nsIFrameManager* aFrameManager,
                                 nsIContent*      aContent,
                                 nsIFrame**       aFrame);

  // APIs for registering objects that can supply additional
  // rules during processing.
  NS_IMETHOD SetStyleRuleSupplier(nsIStyleRuleSupplier* aSupplier);
  NS_IMETHOD GetStyleRuleSupplier(nsIStyleRuleSupplier** aSupplier);

  virtual void List(FILE* out = stdout, PRInt32 aIndent = 0);

  virtual void SizeOf(nsISizeOfHandler *aSizeofHandler, PRUint32 &aSize);
  virtual void ResetUniqueStyleItems(void);

#ifdef SHARE_STYLECONTEXTS
  // add and remove from the cache of all contexts
  NS_IMETHOD AddStyleContext(nsIStyleContext *aNewStyleContext);
  NS_IMETHOD RemoveStyleContext(nsIStyleContext *aNewStyleContext);
  NS_IMETHOD FindMatchingContext(nsIStyleContext *aStyleContextToMatch, 
                                 nsIStyleContext **aMatchingContext);
#endif

#ifdef MOZ_PERF_METRICS
  NS_DECL_NSITIMERECORDER
#endif

  NS_IMETHOD AttributeAffectsStyle(nsIAtom *aAttribute, nsIContent *aContent,
                                   PRBool &aAffects);

  void WalkRuleProcessors(nsISupportsArrayEnumFunc aFunc, void* aData,
                          nsIContent* aContent);

private:
  // These are not supported and are not implemented!
  StyleSetImpl(const StyleSetImpl& aCopy);
  StyleSetImpl& operator=(const StyleSetImpl& aCopy);

protected:
  virtual ~StyleSetImpl();
  PRBool EnsureArray(nsISupportsArray** aArray);
  void RecycleArray(nsISupportsArray** aArray);
  
  void ClearRuleProcessors(void);
  void ClearOverrideRuleProcessors(void);
  void ClearBackstopRuleProcessors(void);
  void ClearDocRuleProcessors(void);

  nsresult  GatherRuleProcessors(void);

  nsIStyleContext* GetContext(nsIPresContext* aPresContext, nsIStyleContext* aParentContext, 
                              nsIAtom* aPseudoTag, nsISupportsArray* aRules, PRBool aForceUnique, 
                              PRBool& aUsedRules);
  void  List(FILE* out, PRInt32 aIndent, nsISupportsArray* aSheets);
  void  ListContexts(nsIStyleContext* aRootContext, FILE* out, PRInt32 aIndent);

  nsISupportsArray* mOverrideSheets;  // most significant first
  nsISupportsArray* mDocSheets;       // " "
  nsISupportsArray* mBackstopSheets;  // " "

  nsISupportsArray* mBackstopRuleProcessors;  // least significant first
  nsISupportsArray* mDocRuleProcessors; // " "
  nsISupportsArray* mOverrideRuleProcessors; // " "

  nsISupportsArray* mRecycler;

  nsIStyleFrameConstruction* mFrameConstructor;
  nsIStyleSheet*    mQuirkStyleSheet; // cached instance for enabling/disabling

  nsCOMPtr<nsIStyleRuleSupplier> mStyleRuleSupplier; 

#ifdef SHARE_STYLECONTEXTS

#ifdef USE_FAST_CACHE
  // a cache of all style contexts for faster searching
  // when we want to find style contexts in GetContext
  StyleContextCache mStyleContextCache;
#else
  nsVoidArray       mStyleContextCache;
#endif
#endif

  MOZ_TIMER_DECLARE(mStyleResolutionWatch)

#ifdef MOZ_PERF_METRICS
  PRBool            mTimerEnabled;   // true if timing is enabled, false if disabled
#endif

};

StyleSetImpl::StyleSetImpl()
  : mOverrideSheets(nsnull),
    mDocSheets(nsnull),
    mBackstopSheets(nsnull),
    mBackstopRuleProcessors(nsnull),
    mDocRuleProcessors(nsnull),
    mOverrideRuleProcessors(nsnull),
    mRecycler(nsnull),
    mFrameConstructor(nsnull),
    mQuirkStyleSheet(nsnull),
    mStyleRuleSupplier(nsnull)
#ifdef SHARE_STYLECONTEXTS
#ifdef USE_FAST_CACHE
    ,mStyleContextCache()
#else
    ,mStyleContextCache(0)
#endif // USE_FAST_CACHE
#endif
#ifdef MOZ_PERF_METRICS
    ,mTimerEnabled(PR_FALSE)
#endif
{
  NS_INIT_REFCNT();
}

StyleSetImpl::~StyleSetImpl()
{
  NS_IF_RELEASE(mOverrideSheets);
  NS_IF_RELEASE(mDocSheets);
  NS_IF_RELEASE(mBackstopSheets);
  NS_IF_RELEASE(mBackstopRuleProcessors);
  NS_IF_RELEASE(mDocRuleProcessors);
  NS_IF_RELEASE(mOverrideRuleProcessors);
  NS_IF_RELEASE(mFrameConstructor);
  NS_IF_RELEASE(mRecycler);
  NS_IF_RELEASE(mQuirkStyleSheet);
#ifdef SHARE_STYLECONTEXTS
 #ifdef DEBUG
  NS_ASSERTION( mStyleContextCache.Count() == 0, "StyleContextCache is not empty in StyleSet destructor: style contexts are being leaked");
  if (mStyleContextCache.Count() > 0) {
    printf("*** Leaking %d style context instances (reported by the StyleContextCache) ***\n", mStyleContextCache.Count());
  }
 #endif
#endif
}

#ifndef MOZ_PERF_METRICS
NS_IMPL_ISUPPORTS(StyleSetImpl, kIStyleSetIID)
#else
NS_IMPL_ISUPPORTS2(StyleSetImpl, nsIStyleSet, nsITimeRecorder)
#endif

PRBool StyleSetImpl::EnsureArray(nsISupportsArray** aArray)
{
  if (nsnull == *aArray) {
    (*aArray) = mRecycler;
    mRecycler = nsnull;
    if (nsnull == *aArray) {
      if (NS_OK != NS_NewISupportsArray(aArray)) {
        return PR_FALSE;
      }
    }
  }
  return PR_TRUE;
}

void
StyleSetImpl::RecycleArray(nsISupportsArray** aArray)
{
  if (! mRecycler) {
    mRecycler = *aArray;  // take ref
    mRecycler->Clear();
    *aArray = nsnull; 
  }
  else {  // already have a recycled array
    NS_RELEASE(*aArray);
  }
}

void  
StyleSetImpl::ClearRuleProcessors(void)
{
  ClearBackstopRuleProcessors();
  ClearDocRuleProcessors();
  ClearOverrideRuleProcessors();
}

void  
StyleSetImpl::ClearBackstopRuleProcessors(void)
{
  if (mBackstopRuleProcessors)
    RecycleArray(&mBackstopRuleProcessors);
}

void  
StyleSetImpl::ClearDocRuleProcessors(void)
{
  if (mDocRuleProcessors)
    RecycleArray(&mDocRuleProcessors);
}

void  
StyleSetImpl::ClearOverrideRuleProcessors(void)
{
  if (mOverrideRuleProcessors)
    RecycleArray(&mOverrideRuleProcessors);
}

struct RuleProcessorData {
  RuleProcessorData(nsISupportsArray* aRuleProcessors) 
    : mRuleProcessors(aRuleProcessors),
      mPrevProcessor(nsnull)
  {}

  nsISupportsArray*       mRuleProcessors;
  nsIStyleRuleProcessor*  mPrevProcessor;
};

static PRBool
EnumRuleProcessor(nsISupports* aSheet, void* aData)
{
  nsIStyleSheet*  sheet = (nsIStyleSheet*)aSheet;
  RuleProcessorData* data = (RuleProcessorData*)aData;

  nsIStyleRuleProcessor* processor = nsnull;
  nsresult result = sheet->GetStyleRuleProcessor(processor, data->mPrevProcessor);
  if (NS_SUCCEEDED(result) && processor) {
    if (processor != data->mPrevProcessor) {
      data->mRuleProcessors->AppendElement(processor);
      data->mPrevProcessor = processor; // ref is held by array
    }
    NS_RELEASE(processor);
  }
  return PR_TRUE;
}

nsresult
StyleSetImpl::GatherRuleProcessors(void)
{
  nsresult result = NS_ERROR_OUT_OF_MEMORY;
  if (mBackstopSheets && !mBackstopRuleProcessors) {
    if (EnsureArray(&mBackstopRuleProcessors)) {
      RuleProcessorData data(mBackstopRuleProcessors);
      mBackstopSheets->EnumerateBackwards(EnumRuleProcessor, &data);
      PRUint32 count;
      mBackstopRuleProcessors->Count(&count);
      if (0 == count) {
        RecycleArray(&mBackstopRuleProcessors);
      }
    } else return result;
  }

  if (mDocSheets && !mDocRuleProcessors) {
    if (EnsureArray(&mDocRuleProcessors)) {
      RuleProcessorData data(mDocRuleProcessors);
      mDocSheets->EnumerateBackwards(EnumRuleProcessor, &data);
      PRUint32 count;
      mDocRuleProcessors->Count(&count);
      if (0 == count) {
        RecycleArray(&mDocRuleProcessors);
      }
    } else return result;
  }

  if (mOverrideSheets && !mOverrideRuleProcessors) {
    if (EnsureArray(&mOverrideRuleProcessors)) {
      RuleProcessorData data(mOverrideRuleProcessors);
      mOverrideSheets->EnumerateBackwards(EnumRuleProcessor, &data);
      PRUint32 count;
      mOverrideRuleProcessors->Count(&count);
      if (0 == count) {
        RecycleArray(&mOverrideRuleProcessors);
      }
    } else return result;
  }

  return NS_OK;
}


// ----- Override sheets

void StyleSetImpl::AppendOverrideStyleSheet(nsIStyleSheet* aSheet)
{
  NS_PRECONDITION(nsnull != aSheet, "null arg");
  if (EnsureArray(&mOverrideSheets)) {
    mOverrideSheets->RemoveElement(aSheet);
    mOverrideSheets->AppendElement(aSheet);
    ClearOverrideRuleProcessors();
  }
}

void StyleSetImpl::InsertOverrideStyleSheetAfter(nsIStyleSheet* aSheet,
                                                 nsIStyleSheet* aAfterSheet)
{
  NS_PRECONDITION(nsnull != aSheet, "null arg");
  if (EnsureArray(&mOverrideSheets)) {
    mOverrideSheets->RemoveElement(aSheet);
    PRInt32 index = mOverrideSheets->IndexOf(aAfterSheet);
    mOverrideSheets->InsertElementAt(aSheet, ++index);
    ClearOverrideRuleProcessors();
  }
}

void StyleSetImpl::InsertOverrideStyleSheetBefore(nsIStyleSheet* aSheet,
                                                  nsIStyleSheet* aBeforeSheet)
{
  NS_PRECONDITION(nsnull != aSheet, "null arg");
  if (EnsureArray(&mOverrideSheets)) {
    mOverrideSheets->RemoveElement(aSheet);
    PRInt32 index = mOverrideSheets->IndexOf(aBeforeSheet);
    mOverrideSheets->InsertElementAt(aSheet, ((-1 < index) ? index : 0));
    ClearOverrideRuleProcessors();
  }
}

void StyleSetImpl::RemoveOverrideStyleSheet(nsIStyleSheet* aSheet)
{
  NS_PRECONDITION(nsnull != aSheet, "null arg");

  if (nsnull != mOverrideSheets) {
    mOverrideSheets->RemoveElement(aSheet);
    ClearOverrideRuleProcessors();
  }
}

PRInt32 StyleSetImpl::GetNumberOfOverrideStyleSheets()
{
  if (nsnull != mOverrideSheets) {
    PRUint32 cnt;
    nsresult rv = mOverrideSheets->Count(&cnt);
    if (NS_FAILED(rv)) return 0;        // XXX error?
    return cnt;
  }
  return 0;
}

nsIStyleSheet* StyleSetImpl::GetOverrideStyleSheetAt(PRInt32 aIndex)
{
  nsIStyleSheet* sheet = nsnull;
  if (nsnull != mOverrideSheets) {
    sheet = (nsIStyleSheet*)mOverrideSheets->ElementAt(aIndex);
  }
  return sheet;
}

// -------- Doc Sheets

void StyleSetImpl::AddDocStyleSheet(nsIStyleSheet* aSheet, nsIDocument* aDocument)
{
  NS_PRECONDITION((nsnull != aSheet) && (nsnull != aDocument), "null arg");
  if (EnsureArray(&mDocSheets)) {
    mDocSheets->RemoveElement(aSheet);
    // lowest index last
    PRInt32 newDocIndex = aDocument->GetIndexOfStyleSheet(aSheet);
    PRUint32 count;
    nsresult rv = mDocSheets->Count(&count);
    if (NS_FAILED(rv)) return;  // XXX error?
    PRUint32 index;
    for (index = 0; index < count; index++) {
      nsIStyleSheet* sheet = (nsIStyleSheet*)mDocSheets->ElementAt(index);
      PRInt32 sheetDocIndex = aDocument->GetIndexOfStyleSheet(sheet);
      if (sheetDocIndex < newDocIndex) {
        mDocSheets->InsertElementAt(aSheet, index);
        index = count; // break loop
      }
      NS_RELEASE(sheet);
    }
    PRUint32 cnt;
    rv = mDocSheets->Count(&cnt);
    if (NS_FAILED(rv)) return;  // XXX error?
    if (cnt == count) {  // didn't insert it
      mDocSheets->AppendElement(aSheet);
    }

    if (nsnull == mFrameConstructor) {
      aSheet->QueryInterface(kIStyleFrameConstructionIID, (void **)&mFrameConstructor);
    }
    ClearDocRuleProcessors();
  }
}

void StyleSetImpl::RemoveDocStyleSheet(nsIStyleSheet* aSheet)
{
  NS_PRECONDITION(nsnull != aSheet, "null arg");

  if (nsnull != mDocSheets) {
    mDocSheets->RemoveElement(aSheet);
    ClearDocRuleProcessors();
  }
}

PRInt32 StyleSetImpl::GetNumberOfDocStyleSheets()
{
  if (nsnull != mDocSheets) {
    PRUint32 cnt;
    nsresult rv = mDocSheets->Count(&cnt);
    if (NS_FAILED(rv)) return 0;        // XXX error?
    return cnt;
  }
  return 0;
}

nsIStyleSheet* StyleSetImpl::GetDocStyleSheetAt(PRInt32 aIndex)
{
  nsIStyleSheet* sheet = nsnull;
  if (nsnull != mDocSheets) {
    sheet = (nsIStyleSheet*)mDocSheets->ElementAt(aIndex);
  }
  return sheet;
}

// ------ backstop sheets

void StyleSetImpl::AppendBackstopStyleSheet(nsIStyleSheet* aSheet)
{
  NS_PRECONDITION(nsnull != aSheet, "null arg");
  if (EnsureArray(&mBackstopSheets)) {
    mBackstopSheets->RemoveElement(aSheet);
    mBackstopSheets->AppendElement(aSheet);
    ClearBackstopRuleProcessors();
  }
}

void StyleSetImpl::InsertBackstopStyleSheetAfter(nsIStyleSheet* aSheet,
                                                 nsIStyleSheet* aAfterSheet)
{
  NS_PRECONDITION(nsnull != aSheet, "null arg");
  if (EnsureArray(&mBackstopSheets)) {
    mBackstopSheets->RemoveElement(aSheet);
    PRInt32 index = mBackstopSheets->IndexOf(aAfterSheet);
    mBackstopSheets->InsertElementAt(aSheet, ++index);
    ClearBackstopRuleProcessors();
  }
}

void StyleSetImpl::InsertBackstopStyleSheetBefore(nsIStyleSheet* aSheet,
                                                  nsIStyleSheet* aBeforeSheet)
{
  NS_PRECONDITION(nsnull != aSheet, "null arg");
  if (EnsureArray(&mBackstopSheets)) {
    mBackstopSheets->RemoveElement(aSheet);
    PRInt32 index = mBackstopSheets->IndexOf(aBeforeSheet);
    mBackstopSheets->InsertElementAt(aSheet, ((-1 < index) ? index : 0));
    ClearBackstopRuleProcessors();
  }
}

void StyleSetImpl::RemoveBackstopStyleSheet(nsIStyleSheet* aSheet)
{
  NS_PRECONDITION(nsnull != aSheet, "null arg");

  if (nsnull != mBackstopSheets) {
    mBackstopSheets->RemoveElement(aSheet);
    ClearBackstopRuleProcessors();
  }
}

PRInt32 StyleSetImpl::GetNumberOfBackstopStyleSheets()
{
  if (nsnull != mBackstopSheets) {
    PRUint32 cnt;
    nsresult rv = mBackstopSheets->Count(&cnt);
    if (NS_FAILED(rv)) return 0;        // XXX error?
    return cnt;
  }
  return 0;
}

NS_IMETHODIMP StyleSetImpl::EnableQuirkStyleSheet(PRBool aEnable)
{
  const char kQuirk_href[] = "resource:/res/quirk.css";
  nsresult rv = NS_OK;
  if (nsnull == mQuirkStyleSheet) {
    // first find the quirk sheet:
    // - run through all of the backstop sheets and check for a CSSStyleSheet that
    //   has the URL we want
    PRUint32 i, nSheets = GetNumberOfBackstopStyleSheets();
    for (i=0; i< nSheets; i++) {
      nsCOMPtr<nsIStyleSheet> sheet;
      sheet = getter_AddRefs(GetBackstopStyleSheetAt(i));
      if (sheet) {
        nsCOMPtr<nsICSSStyleSheet> cssSheet;
        sheet->QueryInterface(nsICSSStyleSheet::GetIID(), getter_AddRefs(cssSheet));
        if (cssSheet) {
          static nsIURI *url = nsnull;
          if (url == nsnull) NS_NewURI(&url, kQuirk_href);
          nsCOMPtr<nsIStyleSheet> quirkSheet;
          PRBool bHasSheet = PR_FALSE;
          if (NS_SUCCEEDED(cssSheet->ContainsStyleSheet(url, bHasSheet, getter_AddRefs(quirkSheet))) && url && bHasSheet) {
            NS_ASSERTION(quirkSheet, "QuirkSheet must be set: ContainsStyleSheet is hosed");
            // cache the sheet for faster lookup next time
            mQuirkStyleSheet = quirkSheet.get();
            // addref for our cached reference
            NS_ADDREF(mQuirkStyleSheet);
          }
        }
      }
    }
  }
  if (mQuirkStyleSheet) {
#ifdef DEBUG
    printf( "%s Quirk StyleSheet\n", aEnable ? "Enabling" : "Disabling" );
#endif
    mQuirkStyleSheet->SetEnabled(aEnable);
  }
  return rv;
}

NS_IMETHODIMP 
StyleSetImpl::NotifyStyleSheetStateChanged(PRBool aDisabled)
{
  ClearRuleProcessors();
  GatherRuleProcessors();
  return NS_OK;
}

nsIStyleSheet* StyleSetImpl::GetBackstopStyleSheetAt(PRInt32 aIndex)
{
  nsIStyleSheet* sheet = nsnull;
  if (nsnull != mBackstopSheets) {
    sheet = (nsIStyleSheet*)mBackstopSheets->ElementAt(aIndex);
  }
  return sheet;
}

void
StyleSetImpl::ReplaceBackstopStyleSheets(nsISupportsArray* aNewBackstopSheets)
{
  ClearBackstopRuleProcessors();
  NS_IF_RELEASE(mBackstopSheets);
  mBackstopSheets = aNewBackstopSheets;
  NS_IF_ADDREF(mBackstopSheets);
}
 
struct RulesMatchingData {
  RulesMatchingData(nsIPresContext* aPresContext,
                    nsIAtom* aMedium,
                    nsIContent* aContent,
                    nsIStyleContext* aParentContext,
                    nsISupportsArray* aResults)
    : mPresContext(aPresContext),
      mMedium(aMedium),
      mContent(aContent),
      mParentContext(aParentContext),
      mResults(aResults)
  {
  }
  nsIPresContext*   mPresContext;
  nsIAtom*          mMedium;
  nsIContent*       mContent;
  nsIStyleContext*  mParentContext;
  nsISupportsArray* mResults;
};

static PRBool
EnumRulesMatching(nsISupports* aProcessor, void* aData)
{
  nsIStyleRuleProcessor*  processor = (nsIStyleRuleProcessor*)aProcessor;
  RulesMatchingData* data = (RulesMatchingData*)aData;

  processor->RulesMatching(data->mPresContext, data->mMedium, data->mContent, 
                           data->mParentContext, data->mResults);
  return PR_TRUE;
}

#ifdef SHARE_STYLECONTEXTS
  #ifdef NOISY_DEBUG
    static int gNewCount=0;
    static int gSharedCount=0;
  #endif
#endif

nsIStyleContext* StyleSetImpl::GetContext(nsIPresContext* aPresContext, 
                                          nsIStyleContext* aParentContext, nsIAtom* aPseudoTag, 
                                          nsISupportsArray* aRules,
                                          PRBool aForceUnique, PRBool& aUsedRules)
{
  nsIStyleContext* result = nsnull;
  aUsedRules = PR_FALSE;

  if ((PR_FALSE == aForceUnique) && (nsnull != aParentContext)) {
    aParentContext->FindChildWithRules(aPseudoTag, aRules, result);
  }

  if (nsnull == result) {
    if (NS_SUCCEEDED(NS_NewStyleContext(&result, aParentContext, aPseudoTag, aRules, aPresContext))) {
      if (PR_TRUE == aForceUnique) {
        result->ForceUnique();
      }
      aUsedRules = PRBool(nsnull != aRules);
    }
#ifdef NOISY_DEBUG
    fprintf(stdout, "+++ NewSC %d +++\n", ++gNewCount);
#endif
  }
#ifdef NOISY_DEBUG
  else {
    fprintf(stdout, "--- SharedSC %d ---\n", ++gSharedCount);
  }
#endif

  return result;
}

// XXX for now only works for strength 0 & 1
static void SortRulesByStrength(nsISupportsArray* aRules)
{
  PRUint32 cnt;
  nsresult rv = aRules->Count(&cnt);
  if (NS_FAILED(rv)) return;    // XXX error?
  PRInt32 count = (PRInt32)cnt;

  if (1 < count) {
    PRInt32 index;
    PRInt32 strength;
    for (index = 0; index < count; ) {
      nsIStyleRule* rule = (nsIStyleRule*)aRules->ElementAt(index);
      rule->GetStrength(strength);
      if (0 < strength) {
        aRules->RemoveElementAt(index);
        aRules->AppendElement(rule);
        count--;
      }
      else {
        index++;
      }
      NS_RELEASE(rule);
    }
  }
}

#ifdef NS_DEBUG
#define NS_ASSERT_REFCOUNT(ptr,cnt,msg) { \
  nsrefcnt  count = ptr->AddRef();        \
  ptr->Release();                         \
  NS_ASSERTION(--count == cnt, msg);      \
}
#else
#define NS_ASSERT_REFCOUNT(ptr,cnt,msg) {}
#endif

nsIStyleContext* StyleSetImpl::ResolveStyleFor(nsIPresContext* aPresContext,
                                               nsIContent* aContent,
                                               nsIStyleContext* aParentContext,
                                               PRBool aForceUnique)
{
  MOZ_TIMER_DEBUGLOG(("Start: StyleSetImpl::ResolveStyleFor(), this=%p\n", this));
  STYLESET_START_TIMER(NS_TIMER_STYLE_RESOLUTION);

  nsIStyleContext*  result = nsnull;

  NS_ASSERTION(aContent, "must have content");
  NS_ASSERTION(aPresContext, "must have pres context");

  if (aContent && aPresContext) {
    GatherRuleProcessors();
    if (mBackstopRuleProcessors || mDocRuleProcessors || mOverrideRuleProcessors) {
      nsISupportsArray*  rules = nsnull;
      if (EnsureArray(&rules)) {
        nsIAtom* medium = nsnull;
        aPresContext->GetMedium(&medium);
        RulesMatchingData data(aPresContext, medium, aContent, aParentContext, rules);
        WalkRuleProcessors(EnumRulesMatching, &data, aContent);
        
        PRBool usedRules = PR_FALSE;
        PRUint32 ruleCount = 0;
        rules->Count(&ruleCount);
        if (0 < ruleCount) {
          SortRulesByStrength(rules);
          result = GetContext(aPresContext, aParentContext, nsnull, rules, aForceUnique, usedRules);
          if (usedRules) {
            NS_ASSERT_REFCOUNT(rules, 2, "rules array was used elsewhere");
            NS_RELEASE(rules);
          }
          else {
            NS_ASSERT_REFCOUNT(rules, 1, "rules array was used elsewhere");
            RecycleArray(&rules);
          }
        }
        else {
          NS_ASSERT_REFCOUNT(rules, 1, "rules array was used elsewhere");
          RecycleArray(&rules);
          result = GetContext(aPresContext, aParentContext, nsnull, nsnull, aForceUnique, usedRules);
        }
        NS_RELEASE(medium);
      }
    }
  }

  MOZ_TIMER_DEBUGLOG(("Stop: StyleSetImpl::ResolveStyleFor(), this=%p\n", this));
  STYLESET_STOP_TIMER(NS_TIMER_STYLE_RESOLUTION);
  return result;
}

struct PseudoRulesMatchingData {
  PseudoRulesMatchingData(nsIPresContext* aPresContext,
                          nsIAtom* aMedium,
                          nsIContent* aParentContent,
                          nsIAtom* aPseudoTag,
                          nsIStyleContext* aParentContext,
                          nsISupportsArray* aResults)
    : mPresContext(aPresContext),
      mMedium(aMedium),
      mParentContent(aParentContent),
      mPseudoTag(aPseudoTag),
      mParentContext(aParentContext),
      mResults(aResults)
  {
  }
  nsIPresContext*   mPresContext;
  nsIAtom*          mMedium;
  nsIContent*       mParentContent;
  nsIAtom*          mPseudoTag;
  nsIStyleContext*  mParentContext;
  nsISupportsArray* mResults;
};

static PRBool
EnumPseudoRulesMatching(nsISupports* aProcessor, void* aData)
{
  nsIStyleRuleProcessor*  processor = (nsIStyleRuleProcessor*)aProcessor;
  PseudoRulesMatchingData* data = (PseudoRulesMatchingData*)aData;

  processor->RulesMatching(data->mPresContext, data->mMedium,
                           data->mParentContent, data->mPseudoTag, 
                           data->mParentContext, data->mResults);
  return PR_TRUE;
}

nsIStyleContext* StyleSetImpl::ResolvePseudoStyleFor(nsIPresContext* aPresContext,
                                                     nsIContent* aParentContent,
                                                     nsIAtom* aPseudoTag,
                                                     nsIStyleContext* aParentContext,
                                                     PRBool aForceUnique)
{
  MOZ_TIMER_DEBUGLOG(("Start: StyleSetImpl::ResolvePseudoStyleFor(), this=%p\n", this));
  STYLESET_START_TIMER(NS_TIMER_STYLE_RESOLUTION);

  nsIStyleContext*  result = nsnull;

  NS_ASSERTION(aPseudoTag, "must have pseudo tag");
  NS_ASSERTION(aPresContext, "must have pres context");

  if (aPseudoTag && aPresContext) {
    GatherRuleProcessors();
    if (mBackstopRuleProcessors || mDocRuleProcessors || mOverrideRuleProcessors) {
      nsISupportsArray*  rules = nsnull;
      if (EnsureArray(&rules)) {
        nsIAtom* medium = nsnull;
        aPresContext->GetMedium(&medium);
        PseudoRulesMatchingData data(aPresContext, medium, aParentContent, 
                                     aPseudoTag, aParentContext, rules);
        WalkRuleProcessors(EnumPseudoRulesMatching, &data, aParentContent);
        
        PRBool usedRules = PR_FALSE;
        PRUint32 ruleCount = 0;
        rules->Count(&ruleCount);
        if (0 < ruleCount) {
          SortRulesByStrength(rules);
          result = GetContext(aPresContext, aParentContext, aPseudoTag, rules, aForceUnique, usedRules);
          if (usedRules) {
            NS_ASSERT_REFCOUNT(rules, 2, "rules array was used elsewhere");
            NS_RELEASE(rules);
          }
          else {
            NS_ASSERT_REFCOUNT(rules, 1, "rules array was used elsewhere");
            rules->Clear();
            RecycleArray(&rules);
          }
        }
        else {
          NS_ASSERT_REFCOUNT(rules, 1, "rules array was used elsewhere");
          RecycleArray(&rules);
          result = GetContext(aPresContext, aParentContext, aPseudoTag, nsnull, aForceUnique, usedRules);
        }
        NS_IF_RELEASE(medium);
      }
    }
  }

  MOZ_TIMER_DEBUGLOG(("Stop: StyleSetImpl::ResolvePseudoStyleFor(), this=%p\n", this));
  STYLESET_STOP_TIMER(NS_TIMER_STYLE_RESOLUTION);
  return result;
}

nsIStyleContext* StyleSetImpl::ProbePseudoStyleFor(nsIPresContext* aPresContext,
                                                   nsIContent* aParentContent,
                                                   nsIAtom* aPseudoTag,
                                                   nsIStyleContext* aParentContext,
                                                   PRBool aForceUnique)
{
  MOZ_TIMER_DEBUGLOG(("Start: StyleSetImpl::ProbePseudoStyleFor(), this=%p\n", this));
  STYLESET_START_TIMER(NS_TIMER_STYLE_RESOLUTION);

  nsIStyleContext*  result = nsnull;

  NS_ASSERTION(aPseudoTag, "must have pseudo tag");
  NS_ASSERTION(aPresContext, "must have pres context");

  if (aPseudoTag && aPresContext) {
    GatherRuleProcessors();
    if (mBackstopRuleProcessors || mDocRuleProcessors || mOverrideRuleProcessors) {
      nsISupportsArray*  rules = nsnull;
      if (EnsureArray(&rules)) {
        nsIAtom* medium = nsnull;
        aPresContext->GetMedium(&medium);
        PseudoRulesMatchingData data(aPresContext, medium, aParentContent, 
                                     aPseudoTag, aParentContext, rules);
        WalkRuleProcessors(EnumPseudoRulesMatching, &data, aParentContent);
        
        PRBool usedRules = PR_FALSE;
        PRUint32 ruleCount;
        rules->Count(&ruleCount);
        if (0 < ruleCount) {
          SortRulesByStrength(rules);
          result = GetContext(aPresContext, aParentContext, aPseudoTag, rules, aForceUnique, usedRules);
          if (usedRules) {
            NS_ASSERT_REFCOUNT(rules, 2, "rules array was used elsewhere");
            NS_RELEASE(rules);
          }
          else {
            NS_ASSERT_REFCOUNT(rules, 1, "rules array was used elsewhere");
            rules->Clear();
            RecycleArray(&rules);
          }
        }
        else {
          NS_ASSERT_REFCOUNT(rules, 1, "rules array was used elsewhere");
          RecycleArray(&rules);
        }
        NS_IF_RELEASE(medium);
      }
    }
  }
  
  MOZ_TIMER_DEBUGLOG(("Stop: StyleSetImpl::ProbePseudoStyleFor(), this=%p\n", this));
  STYLESET_STOP_TIMER(NS_TIMER_STYLE_RESOLUTION);
  return result;
}


NS_IMETHODIMP
StyleSetImpl::ReParentStyleContext(nsIPresContext* aPresContext,
                                   nsIStyleContext* aStyleContext, 
                                   nsIStyleContext* aNewParentContext,
                                   nsIStyleContext** aNewStyleContext)
{
  NS_ASSERTION(aPresContext, "must have pres context");
  NS_ASSERTION(aStyleContext, "must have style context");
  NS_ASSERTION(aNewStyleContext, "must have new style context");

  nsresult result = NS_ERROR_NULL_POINTER;

  if (aPresContext && aStyleContext && aNewStyleContext) {
    nsIStyleContext* oldParent = aStyleContext->GetParent();

    if (oldParent == aNewParentContext) {
      result = NS_OK;
      NS_ADDREF(aStyleContext);   // for return
      *aNewStyleContext = aStyleContext;
    }
    else {  // really a new parent
      nsIStyleContext*  newChild = nsnull;
      nsIAtom*  pseudoTag = nsnull;
      aStyleContext->GetPseudoType(pseudoTag);
      nsISupportsArray* rules = aStyleContext->GetStyleRules();
      if (aNewParentContext) {
        result = aNewParentContext->FindChildWithRules(pseudoTag, rules, newChild);
      }
      if (newChild) { // new parent already has one
        *aNewStyleContext = newChild;
      }
      else {  // need to make one in the new parent
        nsISupportsArray*  newRules = nsnull;
        if (rules) {
          if (EnsureArray(&newRules)) {
            newRules->AppendElements(rules);
          }
        }
        result = NS_NewStyleContext(aNewStyleContext, aNewParentContext, pseudoTag,
                                    newRules, aPresContext);
        NS_IF_RELEASE(newRules);
      }

      NS_IF_RELEASE(rules);
      NS_IF_RELEASE(pseudoTag);
    }

    NS_IF_RELEASE(oldParent);
  }
  return result;
}

struct StatefulData {
  StatefulData(nsIPresContext* aPresContext, nsIAtom* aMedium, nsIContent* aContent)
    : mPresContext(aPresContext),
      mMedium(aMedium),
      mContent(aContent),
      mStateful(PR_FALSE)
  {}
  nsIPresContext* mPresContext;
  nsIAtom*        mMedium;
  nsIContent*     mContent;
  PRBool          mStateful;
}; 

static PRBool SheetHasStatefulStyle(nsISupports* aProcessor, void *aData)
{
  nsIStyleRuleProcessor* processor = (nsIStyleRuleProcessor*)aProcessor;
  StatefulData* data = (StatefulData*)aData;
  if (NS_OK == processor->HasStateDependentStyle(data->mPresContext, data->mMedium,
                                                 data->mContent)) {
    data->mStateful = PR_TRUE;
    return PR_FALSE;  // stop iteration
  }
  return PR_TRUE; // continue
}

// Test if style is dependent on content state
NS_IMETHODIMP
StyleSetImpl::HasStateDependentStyle(nsIPresContext* aPresContext,
                                     nsIContent*     aContent)
{
  GatherRuleProcessors();
  if (mBackstopRuleProcessors || mDocRuleProcessors || mOverrideRuleProcessors) {  
    nsIAtom* medium = nsnull;
    aPresContext->GetMedium(&medium);
    StatefulData data(aPresContext, medium, aContent);
    WalkRuleProcessors(SheetHasStatefulStyle, &data, aContent);
    NS_IF_RELEASE(medium);
    return ((data.mStateful) ? NS_OK : NS_COMFALSE);
  }
  return NS_COMFALSE;
}


NS_IMETHODIMP StyleSetImpl::ConstructRootFrame(nsIPresContext* aPresContext,
                                               nsIContent*     aDocElement,
                                               nsIFrame*&      aFrameSubTree)
{
  nsCOMPtr<nsIPresShell> shell;
  aPresContext->GetShell(getter_AddRefs(shell));
  return mFrameConstructor->ConstructRootFrame(shell, aPresContext, aDocElement,
                                               aFrameSubTree);
}

NS_IMETHODIMP   
StyleSetImpl::ReconstructDocElementHierarchy(nsIPresContext* aPresContext)
{
  return mFrameConstructor->ReconstructDocElementHierarchy(aPresContext);
}

NS_IMETHODIMP StyleSetImpl::ContentAppended(nsIPresContext* aPresContext,
                                            nsIContent*     aContainer,
                                            PRInt32         aNewIndexInContainer)
{
  return mFrameConstructor->ContentAppended(aPresContext, 
                                            aContainer, aNewIndexInContainer);
}

NS_IMETHODIMP StyleSetImpl::ContentInserted(nsIPresContext* aPresContext,
                                            nsIContent*     aContainer,
                                            nsIContent*     aChild,
                                            PRInt32         aIndexInContainer)
{
  return mFrameConstructor->ContentInserted(aPresContext, aContainer,
                                            aChild, aIndexInContainer, nsnull);
}

NS_IMETHODIMP StyleSetImpl::ContentReplaced(nsIPresContext* aPresContext,
                                            nsIContent*     aContainer,
                                            nsIContent*     aOldChild,
                                            nsIContent*     aNewChild,
                                            PRInt32         aIndexInContainer)
{
  return mFrameConstructor->ContentReplaced(aPresContext, aContainer,
                                            aOldChild, aNewChild, aIndexInContainer);
}

NS_IMETHODIMP StyleSetImpl::ContentRemoved(nsIPresContext* aPresContext,
                                           nsIContent*     aContainer,
                                           nsIContent*     aChild,
                                           PRInt32         aIndexInContainer)
{
  return mFrameConstructor->ContentRemoved(aPresContext, aContainer,
                                           aChild, aIndexInContainer);
}

NS_IMETHODIMP
StyleSetImpl::ContentChanged(nsIPresContext* aPresContext,
                             nsIContent* aContent,
                             nsISupports* aSubContent)
{
  return mFrameConstructor->ContentChanged(aPresContext, 
                                           aContent, aSubContent);
}

NS_IMETHODIMP
StyleSetImpl::ContentStatesChanged(nsIPresContext* aPresContext, 
                                   nsIContent* aContent1,
                                   nsIContent* aContent2)
{
  return mFrameConstructor->ContentStatesChanged(aPresContext, aContent1, aContent2);
}


NS_IMETHODIMP
StyleSetImpl::AttributeChanged(nsIPresContext* aPresContext,
                               nsIContent* aContent,
                               PRInt32 aNameSpaceID,
                               nsIAtom* aAttribute,
                               PRInt32 aHint)
{
  return mFrameConstructor->AttributeChanged(aPresContext, aContent, 
                                             aNameSpaceID, aAttribute, aHint);
}


// Style change notifications
NS_IMETHODIMP
StyleSetImpl::StyleRuleChanged(nsIPresContext* aPresContext,
                               nsIStyleSheet* aStyleSheet,
                               nsIStyleRule* aStyleRule,
                               PRInt32 aHint)
{
  return mFrameConstructor->StyleRuleChanged(aPresContext, aStyleSheet, aStyleRule, aHint);
}

NS_IMETHODIMP
StyleSetImpl::StyleRuleAdded(nsIPresContext* aPresContext,
                             nsIStyleSheet* aStyleSheet,
                             nsIStyleRule* aStyleRule)
{
  return mFrameConstructor->StyleRuleAdded(aPresContext, aStyleSheet, aStyleRule);
}

NS_IMETHODIMP
StyleSetImpl::StyleRuleRemoved(nsIPresContext* aPresContext,
                               nsIStyleSheet* aStyleSheet,
                               nsIStyleRule* aStyleRule)
{
  return mFrameConstructor->StyleRuleRemoved(aPresContext, aStyleSheet, aStyleRule);
}

NS_IMETHODIMP
StyleSetImpl::CantRenderReplacedElement(nsIPresContext* aPresContext,
                                        nsIFrame*       aFrame)
{
  nsCOMPtr<nsIPresShell> shell;
  aPresContext->GetShell(getter_AddRefs(shell));
  return mFrameConstructor->CantRenderReplacedElement(shell, aPresContext, aFrame);
}

NS_IMETHODIMP
StyleSetImpl::CreateContinuingFrame(nsIPresContext* aPresContext,
                                    nsIFrame*       aFrame,
                                    nsIFrame*       aParentFrame,
                                    nsIFrame**      aContinuingFrame)
{
  nsCOMPtr<nsIPresShell> shell;
  aPresContext->GetShell(getter_AddRefs(shell));
  return mFrameConstructor->CreateContinuingFrame(shell, aPresContext, aFrame, aParentFrame,
                                                  aContinuingFrame);
}

// Request to find the primary frame associated with a given content object.
// This is typically called by the pres shell when there is no mapping in
// the pres shell hash table
NS_IMETHODIMP
StyleSetImpl::FindPrimaryFrameFor(nsIPresContext*  aPresContext,
                                  nsIFrameManager* aFrameManager,
                                  nsIContent*      aContent,
                                  nsIFrame**       aFrame)
{
  return mFrameConstructor->FindPrimaryFrameFor(aPresContext, aFrameManager,
                                                aContent, aFrame);
}

void StyleSetImpl::List(FILE* out, PRInt32 aIndent, nsISupportsArray* aSheets)
{
  PRUint32 cnt = 0;
  if (aSheets) {
    nsresult rv = aSheets->Count(&cnt);
    if (NS_FAILED(rv)) return;    // XXX error?
  }

  for (PRInt32 index = 0; index < (PRInt32)cnt; index++) {
    nsIStyleSheet* sheet = (nsIStyleSheet*)aSheets->ElementAt(index);
    sheet->List(out, aIndent);
    fputs("\n", out);
    NS_RELEASE(sheet);
  }
}

// APIs for registering objects that can supply additional
// rules during processing.
NS_IMETHODIMP
StyleSetImpl::SetStyleRuleSupplier(nsIStyleRuleSupplier* aSupplier)
{
  mStyleRuleSupplier = aSupplier;
  return NS_OK;
}

NS_IMETHODIMP
StyleSetImpl::GetStyleRuleSupplier(nsIStyleRuleSupplier** aSupplier)
{
  *aSupplier = mStyleRuleSupplier;
  NS_IF_ADDREF(*aSupplier);
  return NS_OK;
}


void StyleSetImpl::List(FILE* out, PRInt32 aIndent)
{
//  List(out, aIndent, mOverrideSheets);
  List(out, aIndent, mDocSheets);
//  List(out, aIndent, mBackstopSheets);
}


void StyleSetImpl::ListContexts(nsIStyleContext* aRootContext, FILE* out, PRInt32 aIndent)
{
  aRootContext->List(out, aIndent);
}


NS_LAYOUT nsresult
NS_NewStyleSet(nsIStyleSet** aInstancePtrResult)
{
  if (aInstancePtrResult == nsnull) {
    return NS_ERROR_NULL_POINTER;
  }

  StyleSetImpl  *it = new StyleSetImpl();

  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return it->QueryInterface(kIStyleSetIID, (void **) aInstancePtrResult);
}

// nsITimeRecorder implementation

#ifdef MOZ_PERF_METRICS

NS_IMETHODIMP
StyleSetImpl::EnableTimer(PRUint32 aTimerID)
{
  nsresult rv = NS_OK;

  if (NS_TIMER_STYLE_RESOLUTION == aTimerID) {
    mTimerEnabled = PR_TRUE;
  }
  else 
    rv = NS_ERROR_NOT_IMPLEMENTED;
  
  return rv;
}

NS_IMETHODIMP
StyleSetImpl::DisableTimer(PRUint32 aTimerID)
{
  nsresult rv = NS_OK;

  if (NS_TIMER_STYLE_RESOLUTION == aTimerID) {
    mTimerEnabled = PR_FALSE;
  }
  else 
    rv = NS_ERROR_NOT_IMPLEMENTED;
  
  return rv;
}

NS_IMETHODIMP
StyleSetImpl::IsTimerEnabled(PRBool *aIsEnabled, PRUint32 aTimerID)
{
  NS_ASSERTION(aIsEnabled != nsnull, "aIsEnabled paramter cannot be null" );
  nsresult rv = NS_OK;

  if (NS_TIMER_STYLE_RESOLUTION == aTimerID) {
	  if (*aIsEnabled != nsnull) {
      *aIsEnabled = mTimerEnabled;		
	  }
  }
  else 
    rv = NS_ERROR_NOT_IMPLEMENTED;
  
  return rv;
}

NS_IMETHODIMP
StyleSetImpl::ResetTimer(PRUint32 aTimerID)
{
  nsresult rv = NS_OK;

  if (NS_TIMER_STYLE_RESOLUTION == aTimerID) {
    MOZ_TIMER_RESET(mStyleResolutionWatch);
  }
  else 
    rv = NS_ERROR_NOT_IMPLEMENTED;
  
  return rv;
}

NS_IMETHODIMP
StyleSetImpl::StartTimer(PRUint32 aTimerID)
{
  nsresult rv = NS_OK;

  if (NS_TIMER_STYLE_RESOLUTION == aTimerID) {
    // only do it if enabled
    if (mTimerEnabled) {
      MOZ_TIMER_START(mStyleResolutionWatch);
    } else {
#ifdef NOISY_DEBUG
      printf( "Attempt to start timer while disabled - ignoring\n" );
#endif
    }
  }
  else 
    rv = NS_ERROR_NOT_IMPLEMENTED;
  
  return rv;
}

NS_IMETHODIMP
StyleSetImpl::StopTimer(PRUint32 aTimerID)
{
  nsresult rv = NS_OK;

  if (NS_TIMER_STYLE_RESOLUTION == aTimerID) {
    // only do it if enabled
    if (mTimerEnabled) {
      MOZ_TIMER_STOP(mStyleResolutionWatch);
    } else {
#ifdef NOISY_DEBUG
      printf( "Attempt to stop timer while disabled - ignoring\n" );
#endif
    }
  }
  else 
    rv = NS_ERROR_NOT_IMPLEMENTED;
  
  return rv;
}

NS_IMETHODIMP
StyleSetImpl::PrintTimer(PRUint32 aTimerID)
{
  nsresult rv = NS_OK;

  if (NS_TIMER_STYLE_RESOLUTION == aTimerID) {
    MOZ_TIMER_PRINT(mStyleResolutionWatch);
  }
  else 
    rv = NS_ERROR_NOT_IMPLEMENTED;
  
  return rv;
}

#endif

//-----------------------------------------------------------------------------

#ifdef SHARE_STYLECONTEXTS
#ifdef USE_FAST_CACHE
// StyleContextCache is a hash table based data structure that maintains
// a list of entries on each hashkey. Thus, multiple items with the same
// ID (key) can be stored in the hash table. The hash-lookup gets the list of
// items having that ID (key). The list is unsorted and vector-based so it is
// best if kept short.
//
// This choice of data structures was based upon knowledge that there are not
// many items sharing the same ID, and that there are many unique IDs, so
// this is a comprimise between a sorted-list based lookup and a hash-type 
// lookup which is not possible due to non-guaranteed-unique keys.

MOZ_DECL_CTOR_COUNTER(StyleContextCache);

StyleContextCache::StyleContextCache(void)
:mCount(0)
{
  MOZ_COUNT_CTOR(StyleContextCache);
}

StyleContextCache:: ~StyleContextCache(void)
{
  mHashTable.Reset();
  MOZ_COUNT_DTOR(StyleContextCache);
}

PRUint32 StyleContextCache::Count(void)
{ 
  // XXX : todo - do debug-checking on the counter...

#ifdef DEBUG
  Tickle("From Count()");
#endif

#ifdef NOISY_DEBUG
  printf("StyleContextCache count: %ld\n", (long)mCount);
#endif

  return mCount;
}

nsresult StyleContextCache::AddContext(scKey aKey, nsIStyleContext *aContext)
{
  // verify we have a list
  nsresult rv = VerifyList(aKey);
  if (NS_SUCCEEDED(rv)){
    nsVoidArray *pList = GetList(aKey);
    NS_ASSERTION(pList != nsnull, "VerifyList failed me!");
    NS_ASSERTION(pList->IndexOf(aContext) == -1, "Context already in list");
    if(pList->IndexOf(aContext) == -1){
      if (!(pList->AppendElement(aContext))) {
        rv = NS_ERROR_FAILURE;
      } else {
        mCount++;
      }
      DumpStats();
    } else {
#ifdef DEBUG
      printf( "Context already in list in StyleContextCache::AddContext\n");
#endif      
      rv = NS_ERROR_FAILURE;
    }
  }
  return rv;
}

nsresult StyleContextCache::RemoveContext(scKey aKey, nsIStyleContext *aContext)
{
  nsresult rv = NS_ERROR_FAILURE;
  nsVoidArray *pResults = nsnull;

  if (NS_SUCCEEDED(GetContexts(aKey,&pResults)) && pResults) {
    PRUint32 nCountBefore = Count();
    if (nCountBefore > 0 && pResults->RemoveElement(aContext)) {
      mCount--;
    }
    DumpStats();

    static PRBool bRemoveEmptyLists = PR_FALSE;
    // if no more entries left, remove the list and all...
    if (bRemoveEmptyLists && pResults->Count() == 0) {
      rv = RemoveAllContexts(aKey);
      if (NS_SUCCEEDED(rv)) {
        NS_ASSERTION(GetList(aKey) == nsnull, "Failed to delete list in StyleContextCache::RemoveContext");
      } 
#ifdef DEBUG
      else {
        printf( "Error removing all contexts in StyleContextCache::RemoveContext\n");
      }
#endif
    }
  }
  return rv;
}

nsresult StyleContextCache::RemoveAllContexts(scKey aKey)
{
  nsresult rv = NS_OK;
  nsVoidKey key((const void *)aKey);
  nsVoidArray *pResults = (nsVoidArray *)mHashTable.Remove(&key);
  if (pResults) {
    delete pResults;
  }
  return rv;
}

nsresult StyleContextCache::GetContexts(scKey aKey, nsVoidArray **aResults)
{
  nsresult rv = NS_OK;
  *aResults = GetList(aKey);
  if (nsnull==aResults) rv = NS_ERROR_FAILURE;
  return rv;
}

// make sure there is a list for the specified key
nsresult StyleContextCache::VerifyList(scKey aKey)
{
  nsresult rv = NS_OK;
  nsVoidKey key((const void *)aKey);
  if (GetList(aKey) == nsnull) {
    nsVoidArray *pList = new nsVoidArray();
    if (pList) {
      mHashTable.Put(&key,pList);
    } else {
      rv = NS_ERROR_FAILURE;
    }
  }
  return rv;
}

// returns the list for the key, may be null
nsVoidArray *StyleContextCache::GetList(scKey aKey)
{
  nsVoidKey key((const void *)aKey);
  return (nsVoidArray *)mHashTable.Get(&key);
}

PRBool HashTableEnumDump(nsHashKey *aKey, void *aData, void* closure);
PRBool HashTableEnumDump(nsHashKey *aKey, void *aData, void* closure)
{
  static PRUint32 nTotal, nCount, nMax, nMin, nUnary;
  if (nsnull == aKey && nsnull == aData && nsnull == closure) {
    // reset the counters
    nTotal = nCount = nMax = nMin = nUnary = 0;
    return PR_TRUE;
  }
  if (nsnull == aKey && nsnull == aData && closure != nsnull) {
    // dump the cumulatives
    printf("----------------------------------------------------------------------------------\n");
    printf("(%d lists, %ld contexts) List Lengths: Min=%ld Max=%ld Average=%ld Unary=%ld\n", 
           (int)nCount, (long)nTotal, (long)nMin, (long)nMax, (long)(nCount>0 ? nTotal/nCount : 0), (long)nUnary);
    printf("----------------------------------------------------------------------------------\n");
    return PR_TRUE;
  }

  // actually do the dump
  nsVoidArray *pList = (nsVoidArray *)aData;
  if (pList) {
    PRUint32 count = pList->Count();
    printf("List Length at key %lu:\t%ld\n", 
           (unsigned long)aKey->HashValue(),
           (long)count );
    nCount++;
    nTotal += count;
    if (count > nMax) nMax = count;
    if (count < nMin) nMin = count;
    if (count == 1) nUnary++;
  }
  return PR_TRUE;
}

PRBool HashTableEnumTickle(nsHashKey *aKey, void *aData, void* closure);
PRBool HashTableEnumTickle(nsHashKey *aKey, void *aData, void* closure)
{
  PRUint32 nCount = 0;

  // each entry is a list
  nsVoidArray *pList = (nsVoidArray *)aData;
  if (pList) {
    PRUint32 count = pList->Count();
    PRUint32 i;
    // walk the list and get each context's key (just a way to tickle it)
    for (i=0; i < count; i++) {
      nsIStyleContext *pContext = (nsIStyleContext *)pList->ElementAt(i);
      if (pContext) {
        scKey key;
        pContext->GetStyleContextKey(key);
        printf( "%p tickled\n");
      }
    }
  }
  return PR_TRUE;
}

void StyleContextCache::DumpStats(void)
{
#ifdef DUMP_CACHE_STATS
  printf("StyleContextCache DumpStats BEGIN\n");
  HashTableEnumDump(nsnull, nsnull, nsnull);
  mHashTable.Enumerate(HashTableEnumDump);
  HashTableEnumDump(nsnull, nsnull, "HACK!");
  printf("StyleContextCache DumpStats END\n");
#endif
}

void StyleContextCache::Tickle(const char *msg)
{
#ifdef DEBUG
  printf("Tickling: %s\n", msg ? msg : "");
  mHashTable.Enumerate(HashTableEnumTickle);
  printf("Tickle done.\n");
#endif
}

#endif // USE_FASTCACHE

NS_IMETHODIMP
StyleSetImpl::AddStyleContext(nsIStyleContext *aNewStyleContext)
{
  nsresult rv = NS_OK;

  // ASSERT the input is valid
  NS_ASSERTION(aNewStyleContext != nsnull, "NULL style context not allowed in AddStyleContext");

#ifdef USE_FAST_CACHE

  if (aNewStyleContext) {
    scKey key;
    aNewStyleContext->GetStyleContextKey(key);
    rv = mStyleContextCache.AddContext(key,aNewStyleContext);
#ifdef NOISY_DEBUG
    printf( "StyleContextCount: %ld\n", (long)mStyleContextCache.Count());
#endif
  }

#else // DONT USE_FAST_CACHE

  // ASSERT it is not already in the collection
  NS_ASSERTION((mStyleContextCache.IndexOf(aNewStyleContext) == -1), "StyleContext added in AddStyleContext is already in cache");
  if (aNewStyleContext) {
    rv = mStyleContextCache.AppendElement(aNewStyleContext);
#ifdef NOISY_DEBUG
    printf( "StyleContextCount: %ld\n", (long)mStyleContextCache.Count());
#endif
  }

#endif // USE_FAST_CACHE

  return rv;
}

NS_IMETHODIMP
StyleSetImpl::RemoveStyleContext(nsIStyleContext *aNewStyleContext)
{
  nsresult rv = NS_OK;

  // ASSERT the input is valid
  NS_ASSERTION(aNewStyleContext != nsnull, "NULL style context not allowed in RemoveStyleContext");

#ifdef USE_FAST_CACHE

  if (aNewStyleContext) {
    scKey key;
    aNewStyleContext->GetStyleContextKey(key);
    rv = mStyleContextCache.RemoveContext(key,aNewStyleContext);
#ifdef NOISY_DEBUG
    printf( "StyleContextCount: %ld\n", (long)mStyleContextCache.Count());
#endif
  }

#else //DONT USE_FAST_CACHE

  // ASSERT it is already in the collection
  NS_ASSERTION((mStyleContextCache.IndexOf(aNewStyleContext) != -1), "StyleContext removed in AddStyleContext is not in cache");
  if (aNewStyleContext) {
    rv = mStyleContextCache.RemoveElement(aNewStyleContext);
#ifdef NOISY_DEBUG
    printf( "StyleContextCount: %ld\n", (long)mStyleContextCache.Count());
#endif
  }

#endif //#ifdef USE_FAST_CACHE

  return rv;
}

NS_IMETHODIMP
StyleSetImpl::FindMatchingContext(nsIStyleContext *aStyleContextToMatch, 
                                  nsIStyleContext **aMatchingContext)
{
  nsresult rv = NS_ERROR_FAILURE;
  *aMatchingContext = nsnull;

#ifdef USE_FAST_CACHE
  nsVoidArray *pList = nsnull;
  scKey key;
  aStyleContextToMatch->GetStyleContextKey(key);
  if (NS_SUCCEEDED(mStyleContextCache.GetContexts(key, &pList)) && pList) {
    PRInt32 count = pList->Count();
    PRInt32 i;
    for (i=0; i<count;i++) {
      nsIStyleContext *pContext = (nsIStyleContext *)pList->ElementAt(i);
      if (pContext && pContext != aStyleContextToMatch) {
        PRBool bMatches = PR_FALSE;
        NS_ADDREF(pContext);
        if (NS_SUCCEEDED(pContext->StyleDataMatches(aStyleContextToMatch, &bMatches)) && 
            bMatches) {
          *aMatchingContext = pContext;
          rv = NS_OK;
          break;
        } else {
          NS_RELEASE(pContext);
        }
      }
    }
  }

#else // DONT USE_FAST_CACHE

  PRInt32 count = mStyleContextCache.Count();
  PRInt32 i;
  for (i=0; i<count;i++) {
    nsIStyleContext *pContext = (nsIStyleContext *)mStyleContextCache.ElementAt(i);
    if (pContext && pContext != aStyleContextToMatch) {
      PRBool bMatches = PR_FALSE;
      NS_ADDREF(pContext);
      if (NS_SUCCEEDED(pContext->StyleDataMatches(aStyleContextToMatch, &bMatches)) && 
          bMatches) {
        *aMatchingContext = pContext;
        rv = NS_OK;
        break;
      } else {
        NS_RELEASE(pContext);
      }
    }
  }

#endif // #ifdef USE_FAST_CACHE

  return rv;
}

#endif // SHARE_STYLECONTEXTS

//-----------------------------------------------------------------------------

// static 
nsUniqueStyleItems *nsUniqueStyleItems ::mInstance = nsnull;

void StyleSetImpl::ResetUniqueStyleItems(void)
{
  UNIQUE_STYLE_ITEMS(uniqueItems);
  uniqueItems->Clear();  
}

struct AttributeContentPair {
  nsIAtom *attribute;
  nsIContent *content;
};

static PRBool
EnumAffectsStyle(nsISupports *aElement, void *aData)
{
  nsIStyleSheet *sheet = NS_STATIC_CAST(nsIStyleSheet *, aElement);
  AttributeContentPair *pair = (AttributeContentPair *)aData;
  PRBool affects;
  if (NS_FAILED(sheet->AttributeAffectsStyle(pair->attribute, pair->content,
                                             affects)) || affects)
    return PR_FALSE;            // stop checking

  return PR_TRUE;
}

NS_IMETHODIMP
StyleSetImpl::AttributeAffectsStyle(nsIAtom *aAttribute, nsIContent *aContent,
                                    PRBool &aAffects)
{
  AttributeContentPair pair;
  pair.attribute = aAttribute;
  pair.content = aContent;

  /* check until we find a sheet that will be affected */
  if ((mDocSheets && !mDocSheets->EnumerateForwards(EnumAffectsStyle, &pair)) ||
      (mOverrideSheets && !mOverrideSheets->EnumerateForwards(EnumAffectsStyle,
                                                              &pair)) ||
      (mBackstopSheets && !mBackstopSheets->EnumerateForwards(EnumAffectsStyle,
                                                              &pair))) {
    aAffects = PR_TRUE;
  } else {
    aAffects = PR_FALSE;
  }

  return NS_OK;
}

/******************************************************************************
* SizeOf method:
*
*  Self (reported as StyleSetImpl's size): 
*    1) sizeof(*this) + sizeof (overhead only) each collection that exists
*       and the FrameConstructor overhead
*
*  Contained / Aggregated data (not reported as StyleSetImpl's size):
*    1) Override Sheets, DocSheets, BackstopSheets, RuleProcessors, Recycler
*       are all delegated to.
*
*  Children / siblings / parents:
*    none
*    
******************************************************************************/
void StyleSetImpl::SizeOf(nsISizeOfHandler *aSizeOfHandler, PRUint32 &aSize)
{
  NS_ASSERTION(aSizeOfHandler != nsnull, "SizeOf handler cannot be null");

  // first get the unique items collection
  UNIQUE_STYLE_ITEMS(uniqueItems);

  if(! uniqueItems->AddItem((void*)this) ){
    NS_ASSERTION(0, "StyleSet has already been conted in SizeOf operation");
    // styleset has already been accounted for
    return;
  }
  // get or create a tag for this instance
  nsCOMPtr<nsIAtom> tag;
  tag = getter_AddRefs(NS_NewAtom("StyleSet"));
  // get the size of an empty instance and add to the sizeof handler
  aSize = sizeof(StyleSetImpl);

  // Next get the size of the OVERHEAD of objects we will delegate to:
  if (mOverrideSheets && uniqueItems->AddItem(mOverrideSheets)){
    aSize += sizeof(*mOverrideSheets);
  }
  if (mDocSheets && uniqueItems->AddItem(mDocSheets)){
    aSize += sizeof(*mDocSheets);
  }
  if (mBackstopSheets && uniqueItems->AddItem(mBackstopSheets)){
    aSize += sizeof(*mBackstopSheets);
  }
  if (mBackstopRuleProcessors && uniqueItems->AddItem(mBackstopRuleProcessors)){
    aSize += sizeof(*mBackstopRuleProcessors);
  }
  if (mDocRuleProcessors && uniqueItems->AddItem(mDocRuleProcessors)){
    aSize += sizeof(*mDocRuleProcessors);
  }
  if (mOverrideRuleProcessors && uniqueItems->AddItem(mOverrideRuleProcessors)){
    aSize += sizeof(*mOverrideRuleProcessors);
  }
  if (mRecycler && uniqueItems->AddItem(mRecycler)){
    aSize += sizeof(*mRecycler);
  }
  if (mQuirkStyleSheet) {
    aSize += sizeof(mQuirkStyleSheet);  // just the pointer: the sheet is counted elsewhere
  }
  ///////////////////////////////////////////////
  // now the FrameConstructor
  if(mFrameConstructor && uniqueItems->AddItem((void*)mFrameConstructor)){
    aSize += sizeof(mFrameConstructor);
  }
  aSizeOfHandler->AddSize(tag,aSize);

  ///////////////////////////////////////////////
  // Now travers the collections and delegate
  PRInt32 numSheets, curSheet;
  PRUint32 localSize=0;
  numSheets = GetNumberOfOverrideStyleSheets();
  for(curSheet=0; curSheet < numSheets; curSheet++){
    nsIStyleSheet* pSheet = GetOverrideStyleSheetAt(curSheet); //addref
    if(pSheet){
      localSize=0;
      pSheet->SizeOf(aSizeOfHandler, localSize);
    }
    NS_IF_RELEASE(pSheet);
  }

  numSheets = GetNumberOfDocStyleSheets();
  for(curSheet=0; curSheet < numSheets; curSheet++){
    nsIStyleSheet* pSheet = GetDocStyleSheetAt(curSheet);
    if(pSheet){
      localSize=0;
      pSheet->SizeOf(aSizeOfHandler, localSize);
    }
    NS_IF_RELEASE(pSheet);
  }

  numSheets = GetNumberOfBackstopStyleSheets();
  for(curSheet=0; curSheet < numSheets; curSheet++){
    nsIStyleSheet* pSheet = GetBackstopStyleSheetAt(curSheet);
    if(pSheet){
      localSize=0;
      pSheet->SizeOf(aSizeOfHandler, localSize);
    }
    NS_IF_RELEASE(pSheet);
  }
  ///////////////////////////////////////////////
  // rule processors
  PRUint32 numRuleProcessors,curRuleProcessor;
  if(mBackstopRuleProcessors){
    mBackstopRuleProcessors->Count(&numRuleProcessors);
    for(curRuleProcessor=0; curRuleProcessor < numRuleProcessors; curRuleProcessor++){
      nsIStyleRuleProcessor* processor = 
        (nsIStyleRuleProcessor* )mBackstopRuleProcessors->ElementAt(curRuleProcessor);
      if(processor){
        localSize=0;
        processor->SizeOf(aSizeOfHandler, localSize);
      }
      NS_IF_RELEASE(processor);
    }
  }
  if(mDocRuleProcessors){
    mDocRuleProcessors->Count(&numRuleProcessors);
    for(curRuleProcessor=0; curRuleProcessor < numRuleProcessors; curRuleProcessor++){
      nsIStyleRuleProcessor* processor = 
        (nsIStyleRuleProcessor* )mDocRuleProcessors->ElementAt(curRuleProcessor);
      if(processor){
        localSize=0;
        processor->SizeOf(aSizeOfHandler, localSize);
      }
      NS_IF_RELEASE(processor);
    }
  }
  if(mOverrideRuleProcessors){
    mOverrideRuleProcessors->Count(&numRuleProcessors);
    for(curRuleProcessor=0; curRuleProcessor < numRuleProcessors; curRuleProcessor++){
      nsIStyleRuleProcessor* processor = 
        (nsIStyleRuleProcessor* )mOverrideRuleProcessors->ElementAt(curRuleProcessor);
      if(processor){
        localSize=0;
        processor->SizeOf(aSizeOfHandler, localSize);
      }
      NS_IF_RELEASE(processor);
    }
  }
  
  ///////////////////////////////////////////////
  // and the recycled ones too
  if(mRecycler){
    mRecycler->Count(&numRuleProcessors);
    for(curRuleProcessor=0; curRuleProcessor < numRuleProcessors; curRuleProcessor++){
      nsIStyleRuleProcessor* processor = 
        (nsIStyleRuleProcessor* )mRecycler->ElementAt(curRuleProcessor);
      if(processor && uniqueItems->AddItem((void*)processor)){
        localSize=0;
        processor->SizeOf(aSizeOfHandler, localSize);
      }
      NS_IF_RELEASE(processor);
    }
  }

  // XXX - do the stylecontext cache too

  // now delegate the sizeof to the larger or more complex aggregated objects
  // - none
}

void
StyleSetImpl::WalkRuleProcessors(nsISupportsArrayEnumFunc aFunc, void* aData,
                                 nsIContent* aContent)
{
  // Walk the backstop rules first.
  if (mBackstopRuleProcessors)
    mBackstopRuleProcessors->EnumerateForwards(aFunc, aData);

  PRBool useRuleProcessors = PR_TRUE;
  if (mStyleRuleSupplier) {
    // We can supply additional document-level sheets that should be walked.
    mStyleRuleSupplier->WalkRules(this, aFunc, aData, aContent);
    mStyleRuleSupplier->UseDocumentRules(aContent, &useRuleProcessors);
  }

  // Walk the doc rules next.
  if (mDocRuleProcessors && useRuleProcessors)
    mDocRuleProcessors->EnumerateForwards(aFunc, aData);
  
  // Walk the override rules last.
  if (mOverrideRuleProcessors)
    mOverrideRuleProcessors->EnumerateForwards(aFunc, aData);
}
