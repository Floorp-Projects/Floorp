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
#ifndef nsStyleSet_h___
#define nsStyleSet_h___

#include <stdio.h>
#include "nsISupports.h"

class nsIAtom;
class nsIStyleRule;
class nsIStyleSheet;
class nsIStyleContext;
class nsIStyleRuleSupplier;
class nsIPresContext;
class nsIPresShell;
class nsIContent;
class nsIFrame;
class nsIDocument;
class nsIFrameManager;
class nsISupportsArray;
class nsRuleNode;
struct nsFindFrameHint;

#include "nsVoidArray.h"
class nsISizeOfHandler;

class nsICSSPseudoComparator;

// IID for the nsIStyleSet interface {e59396b0-b244-11d1-8031-006008159b5a}
#define NS_ISTYLE_SET_IID     \
{0xe59396b0, 0xb244, 0x11d1, {0x80, 0x31, 0x00, 0x60, 0x08, 0x15, 0x9b, 0x5a}}

class nsIStyleSet : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_ISTYLE_SET_IID; return iid; }

  // Style sheets are ordered, most significant first
  // NOTE: this is the reverse of the way documents store the sheets
  virtual void AppendOverrideStyleSheet(nsIStyleSheet* aSheet) = 0;
  virtual void InsertOverrideStyleSheetAfter(nsIStyleSheet* aSheet,
                                             nsIStyleSheet* aAfterSheet) = 0;
  virtual void InsertOverrideStyleSheetBefore(nsIStyleSheet* aSheet,
                                              nsIStyleSheet* aBeforeSheet) = 0;
  virtual void RemoveOverrideStyleSheet(nsIStyleSheet* aSheet) = 0;
  virtual PRInt32 GetNumberOfOverrideStyleSheets() = 0;
  virtual nsIStyleSheet* GetOverrideStyleSheetAt(PRInt32 aIndex) = 0;

  // the ordering of document style sheets is given by the document
  virtual void AddDocStyleSheet(nsIStyleSheet* aSheet, nsIDocument* aDocument) = 0;
  virtual void RemoveDocStyleSheet(nsIStyleSheet* aSheet) = 0;
  virtual PRInt32 GetNumberOfDocStyleSheets() = 0;
  virtual nsIStyleSheet* GetDocStyleSheetAt(PRInt32 aIndex) = 0;

  virtual void AppendBackstopStyleSheet(nsIStyleSheet* aSheet) = 0;
  virtual void InsertBackstopStyleSheetAfter(nsIStyleSheet* aSheet,
                                             nsIStyleSheet* aAfterSheet) = 0;
  virtual void InsertBackstopStyleSheetBefore(nsIStyleSheet* aSheet,
                                              nsIStyleSheet* aBeforeSheet) = 0;
  virtual void RemoveBackstopStyleSheet(nsIStyleSheet* aSheet) = 0;
  virtual PRInt32 GetNumberOfBackstopStyleSheets() = 0;
  virtual nsIStyleSheet* GetBackstopStyleSheetAt(PRInt32 aIndex) = 0;
  virtual void ReplaceBackstopStyleSheets(nsISupportsArray* aNewSheets) = 0;
  
  virtual nsresult GetRuleTree(nsRuleNode** aResult) = 0;
  virtual nsresult ClearCachedDataInRuleTree(nsIStyleRule* aRule) = 0;

  virtual nsresult RemoveBodyFixupRule(nsIDocument *aDocument) = 0;

  // ClearCachedStyleData is used to invalidate portions of both the style context tree
  // and rule tree without destroying the actual nodes in the two trees.  |aRule| provides
  // a hint as to which rule has changed, and all subtree data pruning will occur rooted
  // only on style contexts and rule nodes that use that rule.  If the rule is null, then
  // it is assumed that both trees are to be entirely wiped.
  //
  // |aContext| provides an additional hint that a specific style context has changed, and
  // that the entire rule tree need not be searched for occurrences of |aRule|.  It is
  // only specified in the inline style case, i.e., when the inline style attribute changes.
  virtual nsresult ClearStyleData(nsIPresContext* aPresContext, nsIStyleRule* aRule, nsIStyleContext* aContext) = 0;

  // enable / disable the Quirk style sheet: 
  // returns NS_FAILURE if none is found, otherwise NS_OK
  NS_IMETHOD EnableQuirkStyleSheet(PRBool aEnable) = 0;

  
  NS_IMETHOD NotifyStyleSheetStateChanged(PRBool aDisabled) = 0;

  // get a style context for a non-pseudo frame
  virtual nsIStyleContext* ResolveStyleFor(nsIPresContext* aPresContext,
                                           nsIContent* aContent,
                                           nsIStyleContext* aParentContext,
                                           PRBool aForceUnique = PR_FALSE) = 0;

  // get a style context for a pseudo-frame (ie: tag = NS_NewAtom(":first-line");
  virtual nsIStyleContext* ResolvePseudoStyleFor(nsIPresContext* aPresContext,
                                                 nsIContent* aParentContent,
                                                 nsIAtom* aPseudoTag,
                                                 nsIStyleContext* aParentContext,
                                                 PRBool aForceUnique = PR_FALSE,
                                                 nsICSSPseudoComparator* aComparator = nsnull) = 0;

  // This funtions just like ResolvePseudoStyleFor except that it will
  // return nsnull if there are no explicit style rules for that
  // pseudo element
  virtual nsIStyleContext* ProbePseudoStyleFor(nsIPresContext* aPresContext,
                                               nsIContent* aParentContent,
                                               nsIAtom* aPseudoTag,
                                               nsIStyleContext* aParentContext,
                                               PRBool aForceUnique = PR_FALSE) = 0;

  NS_IMETHOD Shutdown()=0;

  // Get a new style context that lives in a different parent
  // The new context will be the same as the old if the new parent == the old parent
  NS_IMETHOD  ReParentStyleContext(nsIPresContext* aPresContext,
                                   nsIStyleContext* aStyleContext, 
                                   nsIStyleContext* aNewParentContext,
                                   nsIStyleContext** aNewStyleContext) = 0;

  // Test if style is dependent on content state
  NS_IMETHOD  HasStateDependentStyle(nsIPresContext* aPresContext,
                                     nsIContent*     aContent) = 0;

  // Create frames for the root content element and its child content
  NS_IMETHOD  ConstructRootFrame(nsIPresContext* aPresContext,
                                 nsIContent*     aDocElement,
                                 nsIFrame*&      aFrameSubTree) = 0;

  // Causes reconstruction of a frame hierarchy rooted by the
  // frame document element frame. This is often called when radical style
  // change precludes incremental reflow.
  NS_IMETHOD ReconstructDocElementHierarchy(nsIPresContext* aPresContext) = 0;

  // Notifications of changes to the content mpodel
  NS_IMETHOD ContentAppended(nsIPresContext* aPresContext,
                             nsIContent*     aContainer,
                             PRInt32         aNewIndexInContainer) = 0;
  NS_IMETHOD ContentInserted(nsIPresContext* aPresContext,
                             nsIContent*     aContainer,
                             nsIContent*     aChild,
                             PRInt32         aIndexInContainer) = 0;
  NS_IMETHOD ContentReplaced(nsIPresContext* aPresContext,
                             nsIContent*     aContainer,
                             nsIContent*     aOldChild,
                             nsIContent*     aNewChild,
                             PRInt32         aIndexInContainer) = 0;
  NS_IMETHOD ContentRemoved(nsIPresContext*  aPresContext,
                            nsIContent*      aContainer,
                            nsIContent*      aChild,
                            PRInt32          aIndexInContainer) = 0;

  NS_IMETHOD ContentChanged(nsIPresContext*  aPresContext,
                            nsIContent* aContent,
                            nsISupports* aSubContent) = 0;
  NS_IMETHOD ContentStatesChanged(nsIPresContext* aPresContext, 
                                  nsIContent* aContent1,
                                  nsIContent* aContent2) = 0;
  NS_IMETHOD AttributeChanged(nsIPresContext*  aPresContext,
                              nsIContent* aChild,
                              PRInt32 aNameSpaceID,
                              nsIAtom* aAttribute,
                              PRInt32 aModType, 
                              PRInt32 aHint) = 0; // See nsStyleConsts fot hint values

  // Style change notifications
  NS_IMETHOD StyleRuleChanged(nsIPresContext* aPresContext,
                              nsIStyleSheet* aStyleSheet,
                              nsIStyleRule* aStyleRule,
                              PRInt32 aHint) = 0; // See nsStyleConsts fot hint values
  NS_IMETHOD StyleRuleAdded(nsIPresContext* aPresContext,
                            nsIStyleSheet* aStyleSheet,
                            nsIStyleRule* aStyleRule) = 0;
  NS_IMETHOD StyleRuleRemoved(nsIPresContext* aPresContext,
                              nsIStyleSheet* aStyleSheet,
                              nsIStyleRule* aStyleRule) = 0;

  // Notification that we were unable to render a replaced element.
  // Called when the replaced element can not be rendered, and we should
  // instead render the element's contents.
  // The content object associated with aFrame should either be a IMG
  // element or an OBJECT element.
  NS_IMETHOD CantRenderReplacedElement(nsIPresContext* aPresContext,
                                       nsIFrame*       aFrame) = 0;

  // Request to create a continuing frame
  NS_IMETHOD CreateContinuingFrame(nsIPresContext* aPresContext,
                                   nsIFrame*       aFrame,
                                   nsIFrame*       aParentFrame,
                                   nsIFrame**      aContinuingFrame) = 0;

  /** Request to find the primary frame associated with a given content object.
    * This is typically called by the pres shell when there is no mapping in
    * the pres shell hash table.
    * @param aPresContext   the pres context
    * @param aFrameManager  the frame manager
    * @param aContent       the content we need to find a frame for
    * @param aFrame         [OUT] the resulting frame
    * @param aHint          optional performance hint, may be null
    *
    * @return NS_OK.  aFrame will be null if no frame could be found
    */
  NS_IMETHOD FindPrimaryFrameFor(nsIPresContext*  aPresContext,
                                 nsIFrameManager* aFrameManager,
                                 nsIContent*      aContent,
                                 nsIFrame**       aFrame,
                                 nsFindFrameHint* aHint=0) = 0;

  /**
   * Return the point in the frame hierarchy where the frame that
   * will be constructed for |aChildContent| ought be inserted.
   *
   * @param aPresShell      the presentation shell
   * @param aParentFrame    the frame that will parent the frame that is
   *                        created for aChildContent
   * @param aChildContent   the child content for which a frame is to be
   *                        created
   * @param aInsertionPoint [OUT] the frame that should parent the frame
   *                              for |aChildContent|.
   */
  NS_IMETHOD GetInsertionPoint(nsIPresShell* aPresShell,
                               nsIFrame*     aParentFrame,
                               nsIContent*   aChildContent,
                               nsIFrame**    aInsertionPoint) = 0;

  // APIs for registering objects that can supply additional
  // rules during processing.
  NS_IMETHOD SetStyleRuleSupplier(nsIStyleRuleSupplier* aSupplier)=0;
  NS_IMETHOD GetStyleRuleSupplier(nsIStyleRuleSupplier** aSupplier)=0;

#ifdef DEBUG
  virtual void List(FILE* out = stdout, PRInt32 aIndent = 0) = 0;
  virtual void ListContexts(nsIStyleContext* aRootContext, FILE* out = stdout, PRInt32 aIndent = 0) = 0;
  virtual void SizeOf(nsISizeOfHandler *aSizeofHandler, PRUint32 &aSize) = 0;
#endif

  virtual void ResetUniqueStyleItems(void) = 0;

  // If changing the given attribute cannot affect style context, aAffects
  // will be PR_FALSE on return.
  NS_IMETHOD AttributeAffectsStyle(nsIAtom *aAttribute, nsIContent *aContent,
                                   PRBool &aAffects) = 0;
};

extern NS_EXPORT nsresult
  NS_NewStyleSet(nsIStyleSet** aInstancePtrResult);


class nsUniqueStyleItems : private nsVoidArray
{
public :
  // return a singleton instance of the nsUniqueStyleItems object
  static nsUniqueStyleItems *GetUniqueStyleItems( void ){
    if(mInstance == nsnull){
#ifdef DEBUG
      nsUniqueStyleItems *pInstance = 
#endif
      new nsUniqueStyleItems;

      NS_ASSERTION(pInstance == mInstance, "Singleton?");

      // the ctor sets the mInstance static member variable...
      //  if it is null, then we just end up returning null...
    }
    return mInstance;
  }
  
  void *GetItem(void *aPtr){
    PRInt32 index = nsVoidArray::IndexOf(aPtr);
    if( index != -1){
      return nsVoidArray::ElementAt(index);
    } else {
      return nsnull;
    }
  }
  
  PRBool AddItem(void *aPtr){
    if(nsVoidArray::IndexOf(aPtr) == -1){
      return nsVoidArray::AppendElement(aPtr);
    } else {
      return PR_FALSE;
    }
  }
  
  PRBool RemoveItem(void *aPtr){
    return nsVoidArray::RemoveElement(aPtr);
  }

  PRInt32 Count(void){ 
    return nsVoidArray::Count(); 
  }

  void Clear(void){
    nsVoidArray::Clear();
  }
protected:
  // disallow these:
  nsUniqueStyleItems( const nsUniqueStyleItems& src);
  nsUniqueStyleItems& operator =(const nsUniqueStyleItems& src);

  // make this accessable to factory only
  nsUniqueStyleItems(void) : nsVoidArray(){
    NS_ASSERTION(mInstance == nsnull, "singleton?");
    mInstance=this;
  }

  static nsUniqueStyleItems *mInstance;
};

#define UNIQUE_STYLE_ITEMS(__ptr) \
  nsUniqueStyleItems* __ptr = nsUniqueStyleItems::GetUniqueStyleItems(); \
  NS_ASSERTION(__ptr != nsnull, "UniqueItems cannot be null: error in nsUniqueStyleItems factory");

/** a simple struct (that may someday be expanded) 
  * that contains data supplied by the caller to help
  * the style set find a frame for a content node
  */
struct nsFindFrameHint
{
  nsIFrame *mPrimaryFrameForPrevSibling;  // weak ref to the primary frame for the content for which we need a frame
  nsFindFrameHint() { 
    mPrimaryFrameForPrevSibling = nsnull;
  };
};

#endif /* nsIStyleSet_h___ */
