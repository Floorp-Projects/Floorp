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
 *   Daniel Glazman <glazman@netscape.com>
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
#include "nsCOMPtr.h"
#include "nsIStyleSet.h"
#include "nsIStyleSheet.h"
#include "nsIStyleRuleProcessor.h"
#include "nsIStyleRule.h"
#include "nsICSSStyleRule.h"
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
#include "nsRuleNode.h"
#include "nsRuleWalker.h"
#include "nsIBodySuper.h"
#include "nsIHTMLDocument.h"
#include "nsIDOMHTMLBodyElement.h"

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

static NS_DEFINE_IID(kIStyleFrameConstructionIID, NS_ISTYLE_FRAME_CONSTRUCTION_IID);

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
                                                 PRBool aForceUnique = PR_FALSE,
                                                 nsICSSPseudoComparator* aComparator = nsnull);

  virtual nsIStyleContext* ProbePseudoStyleFor(nsIPresContext* aPresContext,
                                               nsIContent* aParentContent,
                                               nsIAtom* aPseudoTag,
                                               nsIStyleContext* aParentContext,
                                               PRBool aForceUnique = PR_FALSE);

  NS_IMETHOD Shutdown();

  virtual nsresult GetRuleTree(nsRuleNode** aResult);
  virtual nsresult ClearCachedDataInRuleTree(nsIStyleRule* aRule);

  virtual nsresult ClearStyleData(nsIPresContext* aPresContext, nsIStyleRule* aRule, nsIStyleContext* aContext);

  virtual nsresult RemoveBodyFixupRule(nsIDocument *aDocument);

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
                              PRInt32 aModType, 
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
                                 nsIFrame**       aFrame,
                                 nsFindFrameHint* aHint);

  // Get the XBL insertion point for a child
  NS_IMETHOD GetInsertionPoint(nsIPresShell* aPresShell,
                               nsIFrame*     aParentFrame,
                               nsIContent*   aChildContent,
                               nsIFrame**    aInsertionPoint);

  // APIs for registering objects that can supply additional
  // rules during processing.
  NS_IMETHOD SetStyleRuleSupplier(nsIStyleRuleSupplier* aSupplier);
  NS_IMETHOD GetStyleRuleSupplier(nsIStyleRuleSupplier** aSupplier);

#ifdef DEBUG
  virtual void List(FILE* out = stdout, PRInt32 aIndent = 0);

  virtual void SizeOf(nsISizeOfHandler *aSizeofHandler, PRUint32 &aSize);
#endif
  virtual void ResetUniqueStyleItems(void);

  void AddImportantRules(nsRuleNode* aRuleNode);

#ifdef MOZ_PERF_METRICS
  NS_DECL_NSITIMERECORDER
#endif

  NS_IMETHOD AttributeAffectsStyle(nsIAtom *aAttribute, nsIContent *aContent,
                                   PRBool &aAffects);

  void WalkRuleProcessors(nsISupportsArrayEnumFunc aFunc, void* aData,
                          nsIContent* aContent);

private:
  static nsrefcnt gInstances;
  static nsIURI *gQuirkURI;

  // These are not supported and are not implemented!
  StyleSetImpl(const StyleSetImpl& aCopy);
  StyleSetImpl& operator=(const StyleSetImpl& aCopy);

protected:
  virtual ~StyleSetImpl();
  PRBool EnsureArray(nsISupportsArray** aArray);
  void RecycleArray(nsISupportsArray** aArray);
  
  void EnsureRuleWalker(nsIPresContext* aPresContext);

  void ClearRuleProcessors(void);
  void ClearOverrideRuleProcessors(void);
  void ClearBackstopRuleProcessors(void);
  void ClearDocRuleProcessors(void);

  nsresult  GatherRuleProcessors(void);

  nsIStyleContext* GetContext(nsIPresContext* aPresContext, 
                              nsIStyleContext* aParentContext,
                              nsIAtom* aPseudoTag, 
                              PRBool aForceUnique);
#ifdef DEBUG
  void  List(FILE* out, PRInt32 aIndent, nsISupportsArray* aSheets);
  void  ListContexts(nsIStyleContext* aRootContext, FILE* out, PRInt32 aIndent);
#endif

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

  nsRuleNode* mRuleTree; // This is the root of our rule tree.  It is a lexicographic tree of
                         // matched rules that style contexts use to look up properties.
  nsRuleWalker* mRuleWalker;   // This is an instance of a rule walker that can be used
                               // to navigate through our tree.

  MOZ_TIMER_DECLARE(mStyleResolutionWatch)

#ifdef MOZ_PERF_METRICS
  PRBool            mTimerEnabled;   // true if timing is enabled, false if disabled
#endif

};

nsrefcnt StyleSetImpl::gInstances = 0;
nsIURI *StyleSetImpl::gQuirkURI = 0;

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
    mStyleRuleSupplier(nsnull),
    mRuleTree(nsnull),
    mRuleWalker(nsnull)
#ifdef MOZ_PERF_METRICS
    ,mTimerEnabled(PR_FALSE)
#endif
{
  NS_INIT_REFCNT();
  if (gInstances++ == 0)
  {
    const char kQuirk_href[] = "resource:/res/quirk.css";
    NS_NewURI (&gQuirkURI, kQuirk_href);
    NS_ASSERTION (gQuirkURI != 0, "Cannot allocate nsStyleSetImpl::gQuirkURI");
  }
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
  if (--gInstances == 0)
  {
    NS_IF_RELEASE (gQuirkURI);
  }
}

#ifndef MOZ_PERF_METRICS
NS_IMPL_ISUPPORTS1(StyleSetImpl, nsIStyleSet)
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
    PRInt32 newDocIndex = 0;
    aDocument->GetIndexOfStyleSheet(aSheet, &newDocIndex);
    PRUint32 count;
    nsresult rv = mDocSheets->Count(&count);
    if (NS_FAILED(rv)) return;  // XXX error?
    PRUint32 index;
    for (index = 0; index < count; index++) {
      nsIStyleSheet* sheet = (nsIStyleSheet*)mDocSheets->ElementAt(index);
      PRInt32 sheetDocIndex = 0;
      aDocument->GetIndexOfStyleSheet(sheet, &sheetDocIndex);
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
        sheet->QueryInterface(NS_GET_IID(nsICSSStyleSheet), getter_AddRefs(cssSheet));
        if (cssSheet) {
          nsCOMPtr<nsIStyleSheet> quirkSheet;
          PRBool bHasSheet = PR_FALSE;
          NS_ASSERTION(gQuirkURI != nsnull, "StyleSetImpl::gQuirkStyleSet is not initialized!");
          if (gQuirkURI != nsnull
              && NS_SUCCEEDED(cssSheet->ContainsStyleSheet(gQuirkURI, bHasSheet, 
                                                           getter_AddRefs(quirkSheet))) 
              && bHasSheet) {
            NS_ASSERTION(quirkSheet, "QuirkSheet must be set: ContainsStyleSheet is hosed");
            // cache the sheet for faster lookup next time
            mQuirkStyleSheet = quirkSheet.get();
            // addref for our cached reference
            NS_ADDREF(mQuirkStyleSheet);
            // only one quirk style sheet can exist, so stop looking
            break;
          }
        }
      }
    }
  }
  if (mQuirkStyleSheet) {
#if defined(DEBUG_warren) || defined(DEBUG_attinasi)
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
                    nsRuleWalker* aRuleWalker)
    : mPresContext(aPresContext),
      mMedium(aMedium),
      mContent(aContent),
      mParentContext(aParentContext),
      mRuleWalker(aRuleWalker)
  {
  }
  nsIPresContext*   mPresContext;
  nsIAtom*          mMedium;
  nsIContent*       mContent;
  nsIStyleContext*  mParentContext;
  nsRuleWalker*     mRuleWalker;
};

static PRBool
EnumRulesMatching(nsISupports* aProcessor, void* aData)
{
  nsIStyleRuleProcessor*  processor = (nsIStyleRuleProcessor*)aProcessor;
  RulesMatchingData* data = (RulesMatchingData*)aData;

  processor->RulesMatching(data->mPresContext, data->mMedium, data->mContent, 
                           data->mParentContext, data->mRuleWalker);
  return PR_TRUE;
}

nsIStyleContext* StyleSetImpl::GetContext(nsIPresContext* aPresContext, 
                                          nsIStyleContext* aParentContext, 
                                          nsIAtom* aPseudoTag, 
                                          PRBool aForceUnique)
{
  nsIStyleContext* result = nsnull;
  
  nsRuleNode* ruleNode = mRuleWalker->GetCurrentNode();
      
  if ((PR_FALSE == aForceUnique) && (nsnull != aParentContext)) {
    aParentContext->FindChildWithRules(aPseudoTag, ruleNode, result);
  }

  if (nsnull == result) {
    if (NS_SUCCEEDED(NS_NewStyleContext(&result, aParentContext, aPseudoTag, ruleNode, aPresContext))) {
      if (PR_TRUE == aForceUnique)
        result->ForceUnique();
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

void
StyleSetImpl::AddImportantRules(nsRuleNode* aCurrNode)
{
  // XXX Note: this is still incorrect from a cascade standpoint, but
  // it preserves the existing incorrect cascade behavior.
  nsRuleNode* parent = aCurrNode->GetParent();
  if (parent)
    AddImportantRules(parent);

  nsCOMPtr<nsIStyleRule> rule;;
  aCurrNode->GetRule(getter_AddRefs(rule));
  nsCOMPtr<nsICSSStyleRule> cssRule(do_QueryInterface(rule));
  if (cssRule) {
    nsCOMPtr<nsIStyleRule> impRule = getter_AddRefs(cssRule->GetImportantRule());
    if (impRule)
      mRuleWalker->Forward(impRule);
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

void StyleSetImpl::EnsureRuleWalker(nsIPresContext* aPresContext)
{ 
  if (mRuleWalker)
    return;

  nsRuleNode::CreateRootNode(aPresContext, &mRuleTree);
  mRuleWalker = new nsRuleWalker(mRuleTree);
}

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
      EnsureRuleWalker(aPresContext);
      nsCOMPtr<nsIAtom> medium;
      aPresContext->GetMedium(getter_AddRefs(medium));
      RulesMatchingData data(aPresContext, medium, aContent, aParentContext, mRuleWalker);
      WalkRuleProcessors(EnumRulesMatching, &data, aContent);
      
      // Walk all of the rules and add in the !important counterparts.
      nsRuleNode* ruleNode = mRuleWalker->GetCurrentNode();
      AddImportantRules(ruleNode);
      result = GetContext(aPresContext, aParentContext, nsnull, aForceUnique);
     
      // Now reset the walker back to the root of the tree.
      mRuleWalker->Reset();
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
                          nsICSSPseudoComparator* aComparator,
                          nsRuleWalker* aWalker)
    : mPresContext(aPresContext),
      mMedium(aMedium),
      mParentContent(aParentContent),
      mPseudoTag(aPseudoTag),
      mParentContext(aParentContext),
      mComparator(aComparator),
      mRuleWalker(aWalker)
  {
  }
  nsIPresContext*         mPresContext;
  nsIAtom*                mMedium;
  nsIContent*             mParentContent;
  nsIAtom*                mPseudoTag;
  nsIStyleContext*        mParentContext;
  nsICSSPseudoComparator* mComparator;
  nsRuleWalker*          mRuleWalker;
};

static PRBool
EnumPseudoRulesMatching(nsISupports* aProcessor, void* aData)
{
  nsIStyleRuleProcessor*  processor = (nsIStyleRuleProcessor*)aProcessor;
  PseudoRulesMatchingData* data = (PseudoRulesMatchingData*)aData;

  processor->RulesMatching(data->mPresContext, data->mMedium,
                           data->mParentContent, data->mPseudoTag, 
                           data->mParentContext, data->mComparator, data->mRuleWalker);
  return PR_TRUE;
}

nsIStyleContext* StyleSetImpl::ResolvePseudoStyleFor(nsIPresContext* aPresContext,
                                                     nsIContent* aParentContent,
                                                     nsIAtom* aPseudoTag,
                                                     nsIStyleContext* aParentContext,
                                                     PRBool aForceUnique,
                                                     nsICSSPseudoComparator* aComparator)
{
  MOZ_TIMER_DEBUGLOG(("Start: StyleSetImpl::ResolvePseudoStyleFor(), this=%p\n", this));
  STYLESET_START_TIMER(NS_TIMER_STYLE_RESOLUTION);

  nsIStyleContext*  result = nsnull;

  NS_ASSERTION(aPseudoTag, "must have pseudo tag");
  NS_ASSERTION(aPresContext, "must have pres context");

  if (aPseudoTag && aPresContext) {
    GatherRuleProcessors();
    if (mBackstopRuleProcessors || mDocRuleProcessors || mOverrideRuleProcessors) {
      nsCOMPtr<nsIAtom> medium;
      aPresContext->GetMedium(getter_AddRefs(medium));
      EnsureRuleWalker(aPresContext);
      PseudoRulesMatchingData data(aPresContext, medium, aParentContent, 
                                   aPseudoTag, aParentContext, aComparator, mRuleWalker);
      WalkRuleProcessors(EnumPseudoRulesMatching, &data, aParentContent);
      
      // Walk all of the rules and add in the !important counterparts.
      nsRuleNode* ruleNode = mRuleWalker->GetCurrentNode();
      AddImportantRules(ruleNode);
      result = GetContext(aPresContext, aParentContext, aPseudoTag, aForceUnique);
     
      // Now reset the walker back to the root of the tree.
      mRuleWalker->Reset();
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
      nsCOMPtr<nsIAtom> medium;
      aPresContext->GetMedium(getter_AddRefs(medium));
      EnsureRuleWalker(aPresContext);
      PseudoRulesMatchingData data(aPresContext, medium, aParentContent, 
                                   aPseudoTag, aParentContext, nsnull, mRuleWalker);
      WalkRuleProcessors(EnumPseudoRulesMatching, &data, aParentContent);
    
      // Walk all of the rules and add in the !important counterparts.
      nsRuleNode* ruleNode = mRuleWalker->GetCurrentNode();
      AddImportantRules(ruleNode);
      if (!mRuleWalker->AtRoot())
        result = GetContext(aPresContext, aParentContext, aPseudoTag, aForceUnique);
 
      // Now reset the walker back to the root of the tree.
      mRuleWalker->Reset();
    }
  }
  
  MOZ_TIMER_DEBUGLOG(("Stop: StyleSetImpl::ProbePseudoStyleFor(), this=%p\n", this));
  STYLESET_STOP_TIMER(NS_TIMER_STYLE_RESOLUTION);
  return result;
}

NS_IMETHODIMP
StyleSetImpl::Shutdown()
{
  delete mRuleWalker;
  if (mRuleTree)
  {
    mRuleTree->Destroy();
    mRuleTree = nsnull;
  }
  return NS_OK;
}

nsresult
StyleSetImpl::GetRuleTree(nsRuleNode** aResult)
{
  *aResult = mRuleTree;
  return NS_OK;
}

nsresult
StyleSetImpl::ClearCachedDataInRuleTree(nsIStyleRule* aInlineStyleRule)
{
  if (mRuleTree)
    mRuleTree->ClearCachedDataInSubtree(aInlineStyleRule);
  return NS_OK;
}

nsresult
StyleSetImpl::ClearStyleData(nsIPresContext* aPresContext, nsIStyleRule* aRule, nsIStyleContext* aContext)
{
  // XXXdwh.  If we're willing to *really* optimize this
  // invalidation, we could only invalidate the struct data
  // that actually changed.  For example, if someone changes
  // style.left, we really only need to blow away cached
  // data in the position struct.
  if (aContext) {
    nsRuleNode* ruleNode;
    aContext->GetRuleNode(&ruleNode);
    ruleNode->ClearCachedData(aRule); 

    // We don't need to mess with the style tree in this case, since the act of 
    // changing inline style attributes automatically causes re-resolution to a new style context
    // (with new descendant style contexts as well).
  }
  else {
    // XXXdwh This is not terribly fast, but fortunately this case is rare (and often a full tree
    // invalidation anyway).  Improving performance here would involve a footprint
    // increase.  Mappings from rule nodes to their associated style contexts as well as
    // mappings from rules to their associated rule nodes would enable us to avoid the two
    // tree walks that occur here.

    // Crawl the entire rule tree and blow away all data for rule nodes (and their descendants)
    // that have the given rule.
    if (mRuleTree)
      mRuleTree->ClearCachedDataInSubtree(aRule);

    // We need to crawl the entire style context tree, and for each style context we need 
    // to see if the specified rule is matched.  If so, that context and all its descendant
    // contexts must have their data wiped.
    nsCOMPtr<nsIPresShell> shell;
    aPresContext->GetShell(getter_AddRefs(shell));
    nsIFrame* rootFrame;
    shell->GetRootFrame(&rootFrame);
    if (rootFrame) {
      nsCOMPtr<nsIStyleContext> rootContext;
      rootFrame->GetStyleContext(getter_AddRefs(rootContext));
      if (rootContext)
        rootContext->ClearStyleData(aPresContext, aRule);
    }
  }

  return NS_OK;
}

nsresult
StyleSetImpl::RemoveBodyFixupRule(nsIDocument *aDocument)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIHTMLDocument> htmlDoc = do_QueryInterface(aDocument);
  if (htmlDoc) {
    nsCOMPtr<nsIDOMHTMLBodyElement> node;
    htmlDoc->GetBodyElement(getter_AddRefs(node));
    if (node) {
      nsCOMPtr<nsIBodySuper> bodyElement = do_QueryInterface(node);
      bodyElement->RemoveBodyFixupRule();
      return NS_OK;
    }
  }
  return rv;
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

      nsRuleNode* ruleNode;
      aStyleContext->GetRuleNode(&ruleNode);
      if (aNewParentContext) {
        result = aNewParentContext->FindChildWithRules(pseudoTag, ruleNode, newChild);
      }
      if (newChild) { // new parent already has one
        *aNewStyleContext = newChild;
      }
      else {  // need to make one in the new parent
        result = NS_NewStyleContext(aNewStyleContext, aNewParentContext, pseudoTag,
                                    ruleNode, aPresContext);
      }

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
                               PRInt32 aModType, 
                               PRInt32 aHint)
{
  return mFrameConstructor->AttributeChanged(aPresContext, aContent, 
                                             aNameSpaceID, aAttribute, aModType, aHint);
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
                                  nsIFrame**       aFrame,
                                  nsFindFrameHint* aHint)
{
  return mFrameConstructor->FindPrimaryFrameFor(aPresContext, aFrameManager,
                                                aContent, aFrame, aHint);
}

NS_IMETHODIMP
StyleSetImpl::GetInsertionPoint(nsIPresShell* aPresShell,
                                nsIFrame*     aParentFrame,
                                nsIContent*   aChildContent,
                                nsIFrame**    aInsertionPoint)
{
  return mFrameConstructor->GetInsertionPoint(aPresShell, aParentFrame,
                                              aChildContent, aInsertionPoint);
}

#ifdef DEBUG
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
#endif

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


#ifdef DEBUG
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
#endif


NS_EXPORT nsresult
NS_NewStyleSet(nsIStyleSet** aInstancePtrResult)
{
  if (aInstancePtrResult == nsnull) {
    return NS_ERROR_NULL_POINTER;
  }

  StyleSetImpl  *it = new StyleSetImpl();

  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return it->QueryInterface(NS_GET_IID(nsIStyleSet), (void **) aInstancePtrResult);
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

  /* scoped sheets should be checked first, since - if present - they will contain
     the bulk of the applicable rules for the content node. */
  if (mStyleRuleSupplier)
    mStyleRuleSupplier->AttributeAffectsStyle(EnumAffectsStyle, &pair, aContent, &aAffects);

  if (!aAffects) {
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
  }

  return NS_OK;
}

#ifdef DEBUG
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
#endif

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
