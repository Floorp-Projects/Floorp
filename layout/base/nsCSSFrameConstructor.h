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
 * The Original Code is mozilla.org code.
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

/*
 * construction of a frame tree that is nearly isomorphic to the content
 * tree and updating of that tree in response to dynamic changes
 */

#ifndef nsCSSFrameConstructor_h___
#define nsCSSFrameConstructor_h___

#include "nsCOMPtr.h"
#include "nsILayoutHistoryState.h"
#include "nsIXBLService.h"
#include "nsQuoteList.h"
#include "nsCounterManager.h"
#include "nsDataHashtable.h"
#include "nsHashKeys.h"
#include "nsThreadUtils.h"
#include "nsPageContentFrame.h"
#include "nsIViewManager.h"

class nsIDocument;
struct nsFrameItems;
struct nsAbsoluteItems;
class nsStyleContext;
struct nsStyleContent;
struct nsStyleDisplay;
class nsIPresShell;
class nsVoidArray;
class nsFrameManager;
class nsIDOMHTMLSelectElement;
class nsPresContext;
class nsStyleChangeList;
class nsIFrame;
struct nsGenConInitializer;

struct nsFindFrameHint
{
  nsIFrame *mPrimaryFrameForPrevSibling;  // weak ref to the primary frame for the content for which we need a frame
  nsFindFrameHint() : mPrimaryFrameForPrevSibling(nsnull) { }
};

typedef void (nsLazyFrameConstructionCallback)
             (nsIContent* aContent, nsIFrame* aFrame, void* aArg);

class nsFrameConstructorState;
class nsFrameConstructorSaveState;
  
class nsCSSFrameConstructor
{
public:
  nsCSSFrameConstructor(nsIDocument *aDocument, nsIPresShell* aPresShell);
  ~nsCSSFrameConstructor(void) {
    NS_ASSERTION(mUpdateCount == 0, "Dying in the middle of our own update?");
    NS_ASSERTION(mFocusSuppressCount == 0, "Focus suppression will be wrong");
  }

  // Maintain global objects - gXBLService
  static nsIXBLService * GetXBLService();
  static void ReleaseGlobals() { NS_IF_RELEASE(gXBLService); }

  // get the alternate text for a content node
  static void GetAlternateTextFor(nsIContent*    aContent,
                                  nsIAtom*       aTag,  // content object's tag
                                  nsXPIDLString& aAltText);
private: 
  // These are not supported and are not implemented! 
  nsCSSFrameConstructor(const nsCSSFrameConstructor& aCopy); 
  nsCSSFrameConstructor& operator=(const nsCSSFrameConstructor& aCopy); 

public:
  // XXXbz this method needs to actually return errors!
  nsresult ConstructRootFrame(nsIContent*     aDocElement,
                              nsIFrame**      aNewFrame);

  nsresult ReconstructDocElementHierarchy();

  nsresult ContentAppended(nsIContent*     aContainer,
                           PRInt32         aNewIndexInContainer);

  nsresult ContentInserted(nsIContent*            aContainer,
                           nsIContent*            aChild,
                           PRInt32                aIndexInContainer,
                           nsILayoutHistoryState* aFrameState);

  nsresult ContentRemoved(nsIContent* aContainer,
                          nsIContent* aChild,
                          PRInt32     aIndexInContainer,
                          PRBool*     aDidReconstruct);

  nsresult CharacterDataChanged(nsIContent*     aContent,
                                PRBool          aAppend);

  nsresult ContentStatesChanged(nsIContent*     aContent1,
                                nsIContent*     aContent2,
                                PRInt32         aStateMask);

  // Process the children of aContent and indicate that frames should be
  // created for them. This is used for lazily built content such as that
  // inside popups so that it is only created when the popup is opened.
  // If aIsSynch is true, this method constructs the frames synchronously.
  // aCallback will be called with three arguments, the first is the value
  // of aContent, the second is aContent's primary frame, and the third is
  // the value of aArg.
  // aCallback will always be called even if the children of aContent had
  // been generated earlier.
  nsresult AddLazyChildren(nsIContent* aContent,
                           nsLazyFrameConstructionCallback* aCallback,
                           void* aArg, PRBool aIsSynch = PR_FALSE);

  // Should be called when a frame is going to be destroyed and
  // WillDestroyFrameTree hasn't been called yet.
  void NotifyDestroyingFrame(nsIFrame* aFrame);

  nsresult AttributeChanged(nsIContent* aContent,
                            PRInt32     aNameSpaceID,
                            nsIAtom*    aAttribute,
                            PRInt32     aModType,
                            PRUint32    aStateMask);

  void BeginUpdate();
  void EndUpdate();
  void RecalcQuotesAndCounters();

  void WillDestroyFrameTree(PRBool aDestroyingPresShell);

  // Get an integer that increments every time there is a style change
  // as a result of a change to the :hover content state.
  PRUint32 GetHoverGeneration() const { return mHoverGeneration; }

  // Note: It's the caller's responsibility to make sure to wrap a
  // ProcessRestyledFrames call in a view update batch.
  // This function does not call ProcessAttachedQueue() on the binding manager.
  // If the caller wants that to happen synchronously, it needs to handle that
  // itself.
  nsresult ProcessRestyledFrames(nsStyleChangeList& aRestyleArray);

private:

  // Note: It's the caller's responsibility to make sure to wrap a
  // ProcessOneRestyle call in a view update batch.
  // This function does not call ProcessAttachedQueue() on the binding manager.
  // If the caller wants that to happen synchronously, it needs to handle that
  // itself.
  void ProcessOneRestyle(nsIContent* aContent, nsReStyleHint aRestyleHint,
                         nsChangeHint aChangeHint);

public:
  // Restyling for a ContentInserted (notification after insertion) or
  // for a CharacterDataChanged.  |aContainer| must be non-null; when
  // the container is null, no work is needed.
  void RestyleForInsertOrChange(nsIContent* aContainer,
                                nsIContent* aChild);
  // This would be the same as RestyleForInsertOrChange if we got the
  // notification before the removal.  However, we get it after, so we
  // have to use the index.  |aContainer| must be non-null; when the
  // container is null, no work is needed.
  void RestyleForRemove(nsIContent* aContainer, nsIContent* aOldChild,
                        PRInt32 aIndexInContainer);
  // Same for a ContentAppended.  |aContainer| must be non-null; when
  // the container is null, no work is needed.
  void RestyleForAppend(nsIContent* aContainer,
                        PRInt32 aNewIndexInContainer);

  // Note: It's the caller's responsibility to make sure to wrap a
  // ProcessPendingRestyles call in a view update batch.
  // This function does not call ProcessAttachedQueue() on the binding manager.
  // If the caller wants that to happen synchronously, it needs to handle that
  // itself.
  void ProcessPendingRestyles();
  
  // Rebuilds all style data by throwing out the old rule tree and
  // building a new one, and additionally applying aExtraHint (which
  // must not contain nsChangeHint_ReconstructFrame) to the root frame.
  void RebuildAllStyleData(nsChangeHint aExtraHint);

  void PostRestyleEvent(nsIContent* aContent, nsReStyleHint aRestyleHint,
                        nsChangeHint aMinChangeHint);
private:
  void PostRestyleEventInternal();
public:

  /**
   * Asynchronously clear style data from the root frame downwards and ensure
   * it will all be rebuilt. This is safe to call anytime; it will schedule
   * a restyle and take effect next time style changes are flushed.
   * This method is used to recompute the style data when some change happens
   * outside of any style rules, like a color preference change or a change
   * in a system font size, or to fix things up when an optimization in the
   * style data has become invalid. We assume that the root frame will not
   * need to be reframed.
   */
  void PostRebuildAllStyleDataEvent(nsChangeHint aExtraHint);

  // Request to create a continuing frame
  nsresult CreateContinuingFrame(nsPresContext* aPresContext,
                                 nsIFrame*       aFrame,
                                 nsIFrame*       aParentFrame,
                                 nsIFrame**      aContinuingFrame,
                                 PRBool          aIsFluid = PR_TRUE);

  // Copy over fixed frames from aParentFrame's prev-in-flow
  nsresult ReplicateFixedFrames(nsPageContentFrame* aParentFrame);

  // Request to find the primary frame associated with a given content object.
  // This is typically called by the pres shell when there is no mapping in
  // the pres shell hash table
  nsresult FindPrimaryFrameFor(nsFrameManager*  aFrameManager,
                               nsIContent*      aContent,
                               nsIFrame**       aFrame,
                               nsFindFrameHint* aHint);

  // Get the XBL insertion point for a child
  nsresult GetInsertionPoint(nsIFrame*     aParentFrame,
                             nsIContent*   aChildContent,
                             nsIFrame**    aInsertionPoint,
                             PRBool*       aMultiple = nsnull);

  nsresult CreateListBoxContent(nsPresContext* aPresContext,
                                nsIFrame*       aParentFrame,
                                nsIFrame*       aPrevFrame,
                                nsIContent*     aChild,
                                nsIFrame**      aResult,
                                PRBool          aIsAppend,
                                PRBool          aIsScrollbar,
                                nsILayoutHistoryState* aFrameState);

  nsresult RemoveMappingsForFrameSubtree(nsIFrame* aRemovedFrame);

  // This is misnamed! This returns the outermost frame for the root element
  nsIFrame* GetInitialContainingBlock() { return mInitialContainingBlock; }
  // This returns the outermost frame for the root element
  nsIFrame* GetRootElementFrame() { return mInitialContainingBlock; }
  // This returns the frame for the root element that does not
  // have a psuedo-element style
  nsIFrame* GetRootElementStyleFrame() { return mRootElementStyleFrame; }
  nsIFrame* GetPageSequenceFrame() { return mPageSequenceFrame; }

private:

  nsresult ReconstructDocElementHierarchyInternal();

  nsresult ReinsertContent(nsIContent*    aContainer,
                           nsIContent*    aChild);

  nsresult ConstructPageFrame(nsIPresShell*  aPresShell, 
                              nsPresContext* aPresContext,
                              nsIFrame*      aParentFrame,
                              nsIFrame*      aPrevPageFrame,
                              nsIFrame*&     aPageFrame,
                              nsIFrame*&     aCanvasFrame);

  void DoContentStateChanged(nsIContent*     aContent,
                             PRInt32         aStateMask);

  /* aMinHint is the minimal change that should be made to the element */
  void RestyleElement(nsIContent*     aContent,
                      nsIFrame*       aPrimaryFrame,
                      nsChangeHint    aMinHint);

  void RestyleLaterSiblings(nsIContent*     aContent);

  nsresult InitAndRestoreFrame (const nsFrameConstructorState& aState,
                                nsIContent*                    aContent,
                                nsIFrame*                      aParentFrame,
                                nsIFrame*                      aPrevInFlow,
                                nsIFrame*                      aNewFrame,
                                PRBool                         aAllowCounters = PR_TRUE);

  already_AddRefed<nsStyleContext>
  ResolveStyleContext(nsIFrame*         aParentFrame,
                      nsIContent*       aContent);

  nsresult ConstructFrame(nsFrameConstructorState& aState,
                          nsIContent*              aContent,
                          nsIFrame*                aParentFrame,
                          nsFrameItems&            aFrameItems);

  nsresult ConstructDocElementFrame(nsFrameConstructorState& aState,
                                    nsIContent*              aDocElement,
                                    nsIFrame*                aParentFrame,
                                    nsIFrame**               aNewFrame);

  nsresult ConstructDocElementTableFrame(nsIContent*            aDocElement,
                                         nsIFrame*              aParentFrame,
                                         nsIFrame**             aNewTableFrame,
                                         nsFrameConstructorState& aState);

  /**
   * CreateAttributeContent creates a single content/frame combination for an
   * |attr(foo)| generated content.
   *
   * @param aParentContent the parent content for the generated content
   * @param aParentFrame the parent frame for the generated frame
   * @param aAttrNamespace the namespace of the attribute in question
   * @param aAttrName the localname of the attribute
   * @param aStyleContext the style context to use
   * @param aGeneratedContent the array of generated content to append the
   *                          created content to.
   * @param [out] aNewContent the content node we create
   * @param [out] aNewFrame the new frame we create
   */
  nsresult CreateAttributeContent(nsIContent* aParentContent,
                                  nsIFrame* aParentFrame,
                                  PRInt32 aAttrNamespace,
                                  nsIAtom* aAttrName,
                                  nsStyleContext* aStyleContext,
                                  nsCOMArray<nsIContent>& aGeneratedContent,
                                  nsIContent** aNewContent,
                                  nsIFrame** aNewFrame);

  /**
   * Create a text node containing the given string. If aText is non-null
   * then we also set aText to the returned node.
   */
  already_AddRefed<nsIContent> CreateGenConTextNode(const nsString& aString,  
                                                    nsCOMPtr<nsIDOMCharacterData>* aText,
                                                    nsGenConInitializer* aInitializer);

  /**
   * Create a content node for the given generated content style.
   * The caller takes care of making it SetNativeAnonymous, binding it
   * to the document, and creating frames for it.
   * @param aParentContent is the node that has the before/after style
   * @param aStyleContext is the 'before' or 'after' pseudo-element
   * style context
   * @param aContentIndex is the index of the content item to create
   */
  already_AddRefed<nsIContent> CreateGeneratedContent(nsIContent*     aParentContent,
                                                      nsStyleContext* aStyleContext,
                                                      PRUint32        aContentIndex);

  void CreateGeneratedContentFrame(nsFrameConstructorState& aState,
                                   nsIFrame*                aFrame,
                                   nsIContent*              aContent,
                                   nsStyleContext*          aStyleContext,
                                   nsIAtom*                 aPseudoElement,
                                   nsFrameItems&            aFrameItems);

  // This method can change aFrameList: it can chop off the end and
  // put it in a special sibling of aParentFrame.  It can also change
  // aState by moving some floats out of it.
  nsresult AppendFrames(nsFrameConstructorState&       aState,
                        nsIContent*                    aContainer,
                        nsIFrame*                      aParentFrame,
                        nsFrameItems&                  aFrameList,
                        nsIFrame*                      aAfterFrame);

  // BEGIN TABLE SECTION
  /**
   * ConstructTableFrame will construct the outer and inner table frames and
   * return them.  Unless aIsPseudo is PR_TRUE, it will put the inner frame in
   * the child list of the outer frame, and will put any pseudo frames it had
   * to create into aChildItems.  The newly-created outer frame will either be
   * in aChildItems or a descendant of a pseudo in aChildItems (unless it's
   * positioned or floated, in which case its placeholder will be in
   * aChildItems).
   */ 
  nsresult ConstructTableFrame(nsFrameConstructorState& aState,
                               nsIContent*              aContent,
                               nsIFrame*                aContentParent,
                               nsStyleContext*          aStyleContext,
                               PRInt32                  aNameSpaceID,
                               PRBool                   aIsPseudo,
                               nsFrameItems&            aChildItems,
                               nsIFrame*&               aNewOuterFrame,
                               nsIFrame*&               aNewInnerFrame);

  nsresult ConstructTableCaptionFrame(nsFrameConstructorState& aState,
                                      nsIContent*              aContent,
                                      nsIFrame*                aParent,
                                      nsStyleContext*          aStyleContext,
                                      PRInt32                  aNameSpaceID,
                                      nsFrameItems&            aChildItems,
                                      nsIFrame*&               aNewFrame,
                                      PRBool&                  aIsPseudoParent);

  nsresult ConstructTableRowGroupFrame(nsFrameConstructorState& aState,
                                       nsIContent*              aContent,
                                       nsIFrame*                aParent,
                                       nsStyleContext*          aStyleContext,
                                       PRInt32                  aNameSpaceID,
                                       PRBool                   aIsPseudo,
                                       nsFrameItems&            aChildItems,
                                       nsIFrame*&               aNewFrame,
                                       PRBool&                  aIsPseudoParent);

  nsresult ConstructTableColGroupFrame(nsFrameConstructorState& aState,
                                       nsIContent*              aContent,
                                       nsIFrame*                aParent,
                                       nsStyleContext*          aStyleContext,
                                       PRInt32                  aNameSpaceID,
                                       PRBool                   aIsPseudo,
                                       nsFrameItems&            aChildItems,
                                       nsIFrame*&               aNewFrame,
                                       PRBool&                  aIsPseudoParent);

  nsresult ConstructTableRowFrame(nsFrameConstructorState& aState,
                                  nsIContent*              aContent,
                                  nsIFrame*                aParent,
                                  nsStyleContext*          aStyleContext,
                                  PRInt32                  aNameSpaceID,
                                  PRBool                   aIsPseudo,
                                  nsFrameItems&            aChildItems,
                                  nsIFrame*&               aNewFrame,
                                  PRBool&                  aIsPseudoParent);

  nsresult ConstructTableColFrame(nsFrameConstructorState& aState,
                                  nsIContent*              aContent,
                                  nsIFrame*                aParent,
                                  nsStyleContext*          aStyleContext,
                                  PRInt32                  aNameSpaceID,
                                  PRBool                   aIsPseudo,
                                  nsFrameItems&            aChildItems,
                                  nsIFrame*&               aNewFrame,
                                  PRBool&                  aIsPseudoParent);

  nsresult ConstructTableCellFrame(nsFrameConstructorState& aState,
                                   nsIContent*              aContent,
                                   nsIFrame*                aParentFrame,
                                   nsStyleContext*          aStyleContext,
                                   PRInt32                  aNameSpaceID,
                                   PRBool                   aIsPseudo,
                                   nsFrameItems&            aChildItems,
                                   nsIFrame*&               aNewCellOuterFrame,
                                   nsIFrame*&               aNewCellInnerFrame,
                                   PRBool&                  aIsPseudoParent);

  nsresult CreatePseudoTableFrame(PRInt32                  aNameSpaceID,
                                  nsFrameConstructorState& aState, 
                                  nsIFrame*                aParentFrameIn = nsnull);

  nsresult CreatePseudoRowGroupFrame(PRInt32                  aNameSpaceID,
                                     nsFrameConstructorState& aState, 
                                     nsIFrame*                aParentFrameIn = nsnull);

  nsresult CreatePseudoColGroupFrame(PRInt32                  aNameSpaceID,
                                     nsFrameConstructorState& aState, 
                                     nsIFrame*                aParentFrameIn = nsnull);

  nsresult CreatePseudoRowFrame(PRInt32                  aNameSpaceID,
                                nsFrameConstructorState& aState, 
                                nsIFrame*                aParentFrameIn = nsnull);

  nsresult CreatePseudoCellFrame(PRInt32                  aNameSpaceID,
                                 nsFrameConstructorState& aState, 
                                 nsIFrame*                aParentFrameIn = nsnull);

  nsresult GetPseudoTableFrame(PRInt32                  aNameSpaceID,
                               nsFrameConstructorState& aState, 
                               nsIFrame&                aParentFrameIn);

  nsresult GetPseudoColGroupFrame(PRInt32                  aNameSpaceID,
                                  nsFrameConstructorState& aState, 
                                  nsIFrame&                aParentFrameIn);

  nsresult GetPseudoRowGroupFrame(PRInt32                  aNameSpaceID,
                                  nsFrameConstructorState& aState, 
                                  nsIFrame&                aParentFrameIn);

  nsresult GetPseudoRowFrame(PRInt32                  aNameSpaceID,
                             nsFrameConstructorState& aState, 
                             nsIFrame&                aParentFrameIn);

  nsresult GetPseudoCellFrame(PRInt32                  aNameSpaceID,
                              nsFrameConstructorState& aState, 
                              nsIFrame&                aParentFrameIn);

  nsresult GetParentFrame(PRInt32                  aNameSpaceID,
                          nsIFrame&                aParentFrameIn, 
                          nsIAtom*                 aChildFrameType, 
                          nsFrameConstructorState& aState, 
                          nsIFrame*&               aParentFrame,
                          PRBool&                  aIsPseudoParent);

  /**
   * Function to adjust aParentFrame and aFrameItems to deal with table
   * pseudo-frames that may have to be inserted.
   * @param aState the nsFrameConstructorState we're using.
   * @param aChildContent the content node we want to construct a frame for
   * @param aParentFrame the frame we think should be the parent.  This will be
   *        adjusted to point to a pseudo-frame if needed.
   * @param aTag tag that would be used for frame construction
   * @param aNameSpaceID namespace that will be used for frame construction
   * @param aChildStyle the style context for aChildContent
   * @param aFrameItems the framelist we think we need to put the child frame
   *        into.  If we have to construct pseudo-frames, we'll modify the
   *        pointer to point to the list the child frame should go into.
   * @param aSaveState the nsFrameConstructorSaveState we can use for pushing a
   *        float containing block if we have to do it.
   * @param aSuppressFrame whether we should not create a frame below this
   *        parent
   * @param aCreatedPseudo whether we had to create a pseudo-parent
   * @return NS_OK on success, NS_ERROR_OUT_OF_MEMORY and such as needed.
   */
  // XXXbz this function should really go away once we rework pseudo-frame
  // handling to be better. This should simply be part of the job of
  // GetGeometricParent, and stuff like the frameitems and parent frame should
  // be kept track of in the state...
  nsresult AdjustParentFrame(nsFrameConstructorState&     aState,
                             nsIContent*                  aChildContent,
                             nsIFrame* &                  aParentFrame,
                             nsIAtom*                     aTag,
                             PRInt32                      aNameSpaceID,
                             nsStyleContext*              aChildStyle,
                             nsFrameItems* &              aFrameItems,
                             nsFrameConstructorSaveState& aSaveState,
                             PRBool&                      aSuppressFrame,
                             PRBool&                      aCreatedPseudo);

  const nsStyleDisplay* GetDisplay(nsIFrame* aFrame);

  // END TABLE SECTION

protected:
  static nsresult CreatePlaceholderFrameFor(nsIPresShell*    aPresShell, 
                                            nsIContent*      aContent,
                                            nsIFrame*        aFrame,
                                            nsStyleContext*  aStyleContext,
                                            nsIFrame*        aParentFrame,
                                            nsIFrame*        aPrevInFlow,
                                            nsIFrame**       aPlaceholderFrame);

private:
  // @param OUT aNewFrame the new radio control frame
  nsresult ConstructRadioControlFrame(nsIFrame**         aNewFrame,
                                      nsIContent*        aContent,
                                      nsStyleContext*    aStyleContext);

  // @param OUT aNewFrame the new checkbox control frame
  nsresult ConstructCheckboxControlFrame(nsIFrame**       aNewFrame,
                                         nsIContent*      aContent,
                                         nsStyleContext*  aStyleContext);
  // ConstructButtonFrame puts the new frame in aFrameItems and
  // handles the kids of the button.
  nsresult ConstructButtonFrame(nsFrameConstructorState& aState,
                                nsIContent*              aContent,
                                nsIFrame*                aParentFrame,
                                nsIAtom*                 aTag,
                                nsStyleContext*          aStyleContext,
                                nsIFrame**               aNewFrame,
                                const nsStyleDisplay*    aStyleDisplay,
                                nsFrameItems&            aFrameItems,
                                PRBool                   aHasPseudoParent);

  // ConstructSelectFrame puts the new frame in aFrameItems and
  // handles the kids of the select.
  nsresult ConstructSelectFrame(nsFrameConstructorState& aState,
                                nsIContent*              aContent,
                                nsIFrame*                aParentFrame,
                                nsIAtom*                 aTag,
                                nsStyleContext*          aStyleContext,
                                nsIFrame*&               aNewFrame,
                                const nsStyleDisplay*    aStyleDisplay,
                                PRBool&                  aFrameHasBeenInitialized,
                                nsFrameItems&            aFrameItems);

  // ConstructFieldSetFrame puts the new frame in aFrameItems and
  // handles the kids of the fieldset
  nsresult ConstructFieldSetFrame(nsFrameConstructorState& aState,
                                  nsIContent*              aContent,
                                  nsIFrame*                aParentFrame,
                                  nsIAtom*                 aTag,
                                  nsStyleContext*          aStyleContext,
                                  nsIFrame*&               aNewFrame,
                                  nsFrameItems&            aFrameItems,
                                  const nsStyleDisplay*    aStyleDisplay,
                                  PRBool&                  aFrameHasBeenInitialized);

  nsresult ConstructTextFrame(nsFrameConstructorState& aState,
                              nsIContent*              aContent,
                              nsIFrame*                aParentFrame,
                              nsStyleContext*          aStyleContext,
                              nsFrameItems&            aFrameItems,
                              PRBool                   aPseudoParent);

  nsresult ConstructPageBreakFrame(nsFrameConstructorState& aState,
                                   nsIContent*              aContent,
                                   nsIFrame*                aParentFrame,
                                   nsStyleContext*          aStyleContext,
                                   nsFrameItems&            aFrameItems);

  // Construct a page break frame if page-break-before:always is set in aStyleContext
  // and add it to aFrameItems. Return true if page-break-after:always is set on aStyleContext.
  // Don't do this for row groups, rows or cell, because tables handle those internally.
  PRBool PageBreakBefore(nsFrameConstructorState& aState,
                         nsIContent*              aContent,
                         nsIFrame*                aParentFrame,
                         nsStyleContext*          aStyleContext,
                         nsFrameItems&            aFrameItems);

  nsresult ConstructHTMLFrame(nsFrameConstructorState& aState,
                              nsIContent*              aContent,
                              nsIFrame*                aParentFrame,
                              nsIAtom*                 aTag,
                              PRInt32                  aNameSpaceID,
                              nsStyleContext*          aStyleContext,
                              nsFrameItems&            aFrameItems,
                              PRBool                   aHasPseudoParent);

  nsresult ConstructFrameInternal( nsFrameConstructorState& aState,
                                   nsIContent*              aContent,
                                   nsIFrame*                aParentFrame,
                                   nsIAtom*                 aTag,
                                   PRInt32                  aNameSpaceID,
                                   nsStyleContext*          aStyleContext,
                                   nsFrameItems&            aFrameItems,
                                   PRBool                   aXBLBaseTag);

  nsresult CreateAnonymousFrames(nsIAtom*                 aTag,
                                 nsFrameConstructorState& aState,
                                 nsIContent*              aParent,
                                 nsIFrame*                aNewFrame,
                                 PRBool                   aAppendToExisting,
                                 nsFrameItems&            aChildItems,
                                 PRBool                   aIsRoot = PR_FALSE);

  nsresult CreateAnonymousFrames(nsFrameConstructorState& aState,
                                 nsIContent*              aParent,
                                 nsIDocument*             aDocument,
                                 nsIFrame*                aNewFrame,
                                 PRBool                   aAppendToExisting,
                                 nsFrameItems&            aChildItems);

//MathML Mod - RBS
#ifdef MOZ_MATHML
  /**
   * Takes the frames in aBlockItems and wraps them in a new anonymous block
   * frame whose content is aContent and whose parent will be aParentFrame.
   * The anonymous block is added to aNewItems and aBlockItems is cleared.
   */
  nsresult FlushAccumulatedBlock(nsFrameConstructorState& aState,
                                 nsIContent* aContent,
                                 nsIFrame* aParentFrame,
                                 nsFrameItems* aBlockItems,
                                 nsFrameItems* aNewItems);

  nsresult ConstructMathMLFrame(nsFrameConstructorState& aState,
                                nsIContent*              aContent,
                                nsIFrame*                aParentFrame,
                                nsIAtom*                 aTag,
                                PRInt32                  aNameSpaceID,
                                nsStyleContext*          aStyleContext,
                                nsFrameItems&            aFrameItems,
                                PRBool                   aHasPseudoParent);
#endif

  nsresult ConstructXULFrame(nsFrameConstructorState& aState,
                             nsIContent*              aContent,
                             nsIFrame*                aParentFrame,
                             nsIAtom*                 aTag,
                             PRInt32                  aNameSpaceID,
                             nsStyleContext*          aStyleContext,
                             nsFrameItems&            aFrameItems,
                             PRBool                   aXBLBaseTag,
                             PRBool                   aHasPseudoParent,
                             PRBool*                  aHaltProcessing);


// XTF
#ifdef MOZ_XTF
  nsresult ConstructXTFFrame(nsFrameConstructorState& aState,
                             nsIContent*              aContent,
                             nsIFrame*                aParentFrame,
                             nsIAtom*                 aTag,
                             PRInt32                  aNameSpaceID,
                             nsStyleContext*          aStyleContext,
                             nsFrameItems&            aFrameItems,
                             PRBool                   aHasPseudoParent);
#endif

// SVG - rods
#ifdef MOZ_SVG
  nsresult ConstructSVGFrame(nsFrameConstructorState& aState,
                             nsIContent*              aContent,
                             nsIFrame*                aParentFrame,
                             nsIAtom*                 aTag,
                             PRInt32                  aNameSpaceID,
                             nsStyleContext*          aStyleContext,
                             nsFrameItems&            aFrameItems,
                             PRBool                   aHasPseudoParent,
                             PRBool*                  aHaltProcessing);
#endif

  nsresult ConstructFrameByDisplayType(nsFrameConstructorState& aState,
                                       const nsStyleDisplay*    aDisplay,
                                       nsIContent*              aContent,
                                       PRInt32                  aNameSpaceID,
                                       nsIAtom*                 aTag,
                                       nsIFrame*                aParentFrame,
                                       nsStyleContext*          aStyleContext,
                                       nsFrameItems&            aFrameItems,
                                       PRBool                   aHasPseudoParent);

  nsresult ProcessChildren(nsFrameConstructorState& aState,
                           nsIContent*              aContent,
                           nsIFrame*                aFrame,
                           PRBool                   aCanHaveGeneratedContent,
                           nsFrameItems&            aFrameItems,
                           PRBool                   aParentIsBlock);

  // @param OUT aFrame the newly created frame
  nsresult CreateInputFrame(nsFrameConstructorState& aState,
                            nsIContent*              aContent,
                            nsIFrame*                aParentFrame,
                            nsIAtom*                 aTag,
                            nsStyleContext*          aStyleContext,
                            nsIFrame**               aFrame,
                            const nsStyleDisplay*    aStyleDisplay,
                            PRBool&                  aFrameHasBeenInitialized,
                            PRBool&                  aAddedToFrameList,
                            nsFrameItems&            aFrameItems,
                            PRBool                   aHasPseudoParent);

  // A function that can be invoked to create some sort of image frame.
  typedef nsIFrame* (* ImageFrameCreatorFunc)(nsIPresShell*, nsStyleContext*);

  /**
   * CreateHTMLImageFrame will do some tests on aContent, and if it determines
   * that the content should get an image frame it'll create one via aFunc and
   * return it in *aFrame.  Note that if this content node isn't supposed to
   * have an image frame this method will return NS_OK and set *aFrame to null.
   */
  nsresult CreateHTMLImageFrame(nsIContent*           aContent,
                                nsStyleContext*       aStyleContext,
                                ImageFrameCreatorFunc aFunc,
                                nsIFrame**            aFrame);

  nsIFrame* GetFrameFor(nsIContent* aContent);

  /**
   * These two functions are used when we start frame creation from a non-root
   * element. They should recreate the same state that we would have
   * arrived at if we had built frames from the root frame to aFrame.
   * Therefore, any calls to PushFloatContainingBlock and
   * PushAbsoluteContainingBlock during frame construction should get
   * corresponding logic in these functions.
   */
public:
  nsIFrame* GetAbsoluteContainingBlock(nsIFrame* aFrame);
private:
  nsIFrame* GetFloatContainingBlock(nsIFrame* aFrame);

  nsIContent* PropagateScrollToViewport();

  // Build a scroll frame: 
  //  Calls BeginBuildingScrollFrame, InitAndRestoreFrame, and then FinishBuildingScrollFrame.
  //  Sets the primary frame for the content to the output aNewFrame.
  // @param aNewFrame the created scrollframe --- output only
  nsresult
  BuildScrollFrame(nsFrameConstructorState& aState,
                   nsIContent*              aContent,
                   nsStyleContext*          aContentStyle,
                   nsIFrame*                aScrolledFrame,
                   nsIFrame*                aParentFrame,
                   nsIFrame*                aContentParentFrame,
                   nsIFrame*&               aNewFrame,
                   nsStyleContext*&         aScrolledChildStyle);

  // Builds the initial ScrollFrame
  already_AddRefed<nsStyleContext>
  BeginBuildingScrollFrame(nsFrameConstructorState& aState,
                           nsIContent*              aContent,
                           nsStyleContext*          aContentStyle,
                           nsIFrame*                aParentFrame,
                           nsIFrame*                aContentParentFrame,
                           nsIAtom*                 aScrolledPseudo,
                           PRBool                   aIsRoot,
                           nsIFrame*&               aNewFrame);

  // Completes the building of the scrollframe:
  // Creates a view for the scrolledframe and makes it the child of the scrollframe.
  void
  FinishBuildingScrollFrame(nsIFrame* aScrollFrame,
                            nsIFrame* aScrolledFrame);

  // InitializeSelectFrame puts scrollFrame in aFrameItems if aBuildCombobox is false
  nsresult
  InitializeSelectFrame(nsFrameConstructorState& aState,
                        nsIFrame*                scrollFrame,
                        nsIFrame*                scrolledFrame,
                        nsIContent*              aContent,
                        nsIFrame*                aParentFrame,
                        nsStyleContext*          aStyleContext,
                        PRBool                   aBuildCombobox,
                        nsFrameItems&            aFrameItems);

  nsresult MaybeRecreateFramesForContent(nsIContent*      aContent);

  nsresult RecreateFramesForContent(nsIContent*      aContent);

  // If removal of aFrame from the frame tree requires reconstruction of some
  // containing block (either of aFrame or of its parent) due to {ib} splits,
  // recreate the relevant containing block.  The return value indicates
  // whether this happened.  If this method returns true, *aResult is the
  // return value of ReframeContainingBlock.  If this method returns false, the
  // value of *aResult is no affected.  aFrame and aResult must not be null.
  // aFrame must be the result of a GetPrimaryFrameFor() call (which means its
  // parent is also not null).
  PRBool MaybeRecreateContainerForIBSplitterFrame(nsIFrame* aFrame,
                                                  nsresult* aResult);

  nsresult CreateContinuingOuterTableFrame(nsIPresShell*    aPresShell, 
                                           nsPresContext*  aPresContext,
                                           nsIFrame*        aFrame,
                                           nsIFrame*        aParentFrame,
                                           nsIContent*      aContent,
                                           nsStyleContext*  aStyleContext,
                                           nsIFrame**       aContinuingFrame);

  nsresult CreateContinuingTableFrame(nsIPresShell*    aPresShell, 
                                      nsPresContext*  aPresContext,
                                      nsIFrame*        aFrame,
                                      nsIFrame*        aParentFrame,
                                      nsIContent*      aContent,
                                      nsStyleContext*  aStyleContext,
                                      nsIFrame**       aContinuingFrame);

  //----------------------------------------

  // Methods support creating block frames and their children

  already_AddRefed<nsStyleContext>
  GetFirstLetterStyle(nsIContent*      aContent,
                      nsStyleContext*  aStyleContext);

  already_AddRefed<nsStyleContext>
  GetFirstLineStyle(nsIContent*      aContent,
                    nsStyleContext*  aStyleContext);

  PRBool ShouldHaveFirstLetterStyle(nsIContent*      aContent,
                                    nsStyleContext*  aStyleContext);

  // Check whether a given block has first-letter style.  Make sure to
  // only pass in blocks!  And don't pass in null either.
  PRBool HasFirstLetterStyle(nsIFrame* aBlockFrame);

  PRBool ShouldHaveFirstLineStyle(nsIContent*      aContent,
                                  nsStyleContext*  aStyleContext);

  void ShouldHaveSpecialBlockStyle(nsIContent*      aContent,
                                   nsStyleContext*  aStyleContext,
                                   PRBool*          aHaveFirstLetterStyle,
                                   PRBool*          aHaveFirstLineStyle);

  // |aContentParentFrame| should be null if it's really the same as
  // |aParentFrame|.
  // @param aFrameItems where we want to put the block in case it's in-flow.
  // @param aNewFrame an in/out parameter. On input it is the block to be
  // constructed. On output it is reset to the outermost
  // frame constructed (e.g. if we need to wrap the block in an
  // nsColumnSetFrame.
  // @param aParentFrame is the desired parent for the (possibly wrapped)
  // block
  // @param aContentParent is the parent the block would have if it
  // were in-flow
  nsresult ConstructBlock(nsFrameConstructorState& aState,
                          const nsStyleDisplay*    aDisplay,
                          nsIContent*              aContent,
                          nsIFrame*                aParentFrame,
                          nsIFrame*                aContentParentFrame,
                          nsStyleContext*          aStyleContext,
                          nsIFrame**               aNewFrame,
                          nsFrameItems&            aFrameItems,
                          PRBool                   aAbsPosContainer);

  nsresult ConstructInline(nsFrameConstructorState& aState,
                           const nsStyleDisplay*    aDisplay,
                           nsIContent*              aContent,
                           nsIFrame*                aParentFrame,
                           nsStyleContext*          aStyleContext,
                           PRBool                   aIsPositioned,
                           nsIFrame*                aNewFrame);

  /**
   * Move an already-constructed framelist into the inline frame at
   * the tail end of an {ib} split.  Creates said inline if it doesn't
   * already exist.
   *
   * @param aState the frame construction state we're using right now.
   * @param aExistingEndFrame if non-null, the already-existing end frame.
   * @param aIsPositioned Whether the end frame should be positioned.
   * @param aContent the content node for this {ib} split.
   * @param aStyleContext the style context to use for the new frame
   * @param aFramesToMove The frame list to move over
   * @param aBlockPart the block part of the {ib} split.
   * @param aTargetState if non-null, the target state to pass to
   *        MoveChildrenTo for float reparenting.
   * XXXbz test float reparenting?
   *
   * @note aIsPositioned, aContent, aStyleContext, are
   *       only used if aExistingEndFrame is null.
   */
  nsIFrame* MoveFramesToEndOfIBSplit(nsFrameConstructorState& aState,
                                     nsIFrame* aExistingEndFrame,
                                     PRBool aIsPositioned,
                                     nsIContent* aContent,
                                     nsStyleContext* aStyleContext,
                                     nsIFrame* aFramesToMove,
                                     nsIFrame* aBlockPart,
                                     nsFrameConstructorState* aTargetState);

  nsresult ProcessInlineChildren(nsFrameConstructorState& aState,
                                 nsIContent*              aContent,
                                 nsIFrame*                aFrame,
                                 PRBool                   aCanHaveGeneratedContent,
                                 nsFrameItems&            aFrameItems,
                                 PRBool*                  aKidsAllInline);

  // Determine whether we need to wipe out what we just did and start over
  // because we're doing something like adding block kids to an inline frame
  // (and therefore need an {ib} split).  If aIsAppend is true, aPrevSibling is
  // ignored.  Otherwise it may be used to determine whether to reframe when
  // inserting into the block of an {ib} split.
  // @return PR_TRUE if we reconstructed the containing block, PR_FALSE
  // otherwise
  PRBool WipeContainingBlock(nsFrameConstructorState& aState,
                             nsIFrame*                aContainingBlock,
                             nsIFrame*                aFrame,
                             const nsFrameItems&      aFrameList,
                             PRBool                   aIsAppend,
                             nsIFrame*                aPrevSibling);

  nsresult ReframeContainingBlock(nsIFrame* aFrame);

  nsresult StyleChangeReflow(nsIFrame* aFrame);

  /** Helper function that searches the immediate child frames 
    * (and their children if the frames are "special")
    * for a frame that maps the specified content object
    *
    * @param aParentFrame   the primary frame for aParentContent
    * @param aContent       the content node for which we seek a frame
    * @param aParentContent the parent for aContent 
    * @param aHint          an optional hint used to make the search for aFrame faster
    */
  nsIFrame* FindFrameWithContent(nsFrameManager*  aFrameManager,
                                 nsIFrame*        aParentFrame,
                                 nsIContent*      aParentContent,
                                 nsIContent*      aContent,
                                 nsFindFrameHint* aHint);

  //----------------------------------------

  // Methods support :first-letter style

  void CreateFloatingLetterFrame(nsFrameConstructorState& aState,
                                 nsIFrame*                aBlockFrame,
                                 nsIContent*              aTextContent,
                                 nsIFrame*                aTextFrame,
                                 nsIContent*              aBlockContent,
                                 nsIFrame*                aParentFrame,
                                 nsStyleContext*          aStyleContext,
                                 nsFrameItems&            aResult);

  nsresult CreateLetterFrame(nsFrameConstructorState& aState,
                             nsIFrame*                aBlockFrame,
                             nsIContent*              aTextContent,
                             nsIFrame*                aParentFrame,
                             nsFrameItems&            aResult);

  nsresult WrapFramesInFirstLetterFrame(nsFrameConstructorState& aState,
                                        nsIContent*              aBlockContent,
                                        nsIFrame*                aBlockFrame,
                                        nsFrameItems&            aBlockFrames);

  nsresult WrapFramesInFirstLetterFrame(nsFrameConstructorState& aState,
                                        nsIFrame*                aBlockFrame,
                                        nsIFrame*                aParentFrame,
                                        nsIFrame*                aParentFrameList,
                                        nsIFrame**               aModifiedParent,
                                        nsIFrame**               aTextFrame,
                                        nsIFrame**               aPrevFrame,
                                        nsFrameItems&            aLetterFrame,
                                        PRBool*                  aStopLooking);

  nsresult RecoverLetterFrames(nsFrameConstructorState& aState,
                               nsIFrame*                aBlockFrame);

  // 
  nsresult RemoveLetterFrames(nsPresContext*  aPresContext,
                              nsIPresShell*    aPresShell,
                              nsFrameManager*  aFrameManager,
                              nsIFrame*        aBlockFrame);

  // Recursive helper for RemoveLetterFrames
  nsresult RemoveFirstLetterFrames(nsPresContext*  aPresContext,
                                   nsIPresShell*    aPresShell,
                                   nsFrameManager*  aFrameManager,
                                   nsIFrame*        aFrame,
                                   PRBool*          aStopLooking);

  // Special remove method for those pesky floating first-letter frames
  nsresult RemoveFloatingFirstLetterFrames(nsPresContext*  aPresContext,
                                           nsIPresShell*    aPresShell,
                                           nsFrameManager*  aFrameManager,
                                           nsIFrame*        aBlockFrame,
                                           PRBool*          aStopLooking);

  // Capture state for the frame tree rooted at the frame associated with the
  // content object, aContent
  nsresult CaptureStateForFramesOf(nsIContent* aContent,
                                   nsILayoutHistoryState* aHistoryState);

  // Capture state for the frame tree rooted at aFrame.
  nsresult CaptureStateFor(nsIFrame*              aFrame,
                           nsILayoutHistoryState* aHistoryState);

  //----------------------------------------

  // Methods support :first-line style

  nsresult WrapFramesInFirstLineFrame(nsFrameConstructorState& aState,
                                      nsIContent*              aBlockContent,
                                      nsIFrame*                aBlockFrame,
                                      nsFrameItems&            aFrameItems);

  nsresult AppendFirstLineFrames(nsFrameConstructorState& aState,
                                 nsIContent*              aContent,
                                 nsIFrame*                aBlockFrame,
                                 nsFrameItems&            aFrameItems);

  nsresult InsertFirstLineFrames(nsFrameConstructorState& aState,
                                 nsIContent*              aContent,
                                 nsIFrame*                aBlockFrame,
                                 nsIFrame**               aParentFrame,
                                 nsIFrame*                aPrevSibling,
                                 nsFrameItems&            aFrameItems);

  nsresult RemoveFixedItems(const nsFrameConstructorState& aState,
                            nsIFrame*                      aRootElementFrame);

  // Find the right frame to use for aContent when looking for sibling
  // frames for aTargetContent.  If aPrevSibling is true, this
  // will look for last continuations, etc, as necessary.  This calls
  // IsValidSibling as needed; if that returns false it returns null.
  //
  // @param aTargetContentDisplay the CSS display enum for aTargetContent if
  // already known, UNSET_DISPLAY otherwise.
  nsIFrame* FindFrameForContentSibling(nsIContent* aContent,
                                       nsIContent* aTargetContent,
                                       PRUint8& aTargetContentDisplay,
                                       PRBool aPrevSibling);

  // Find the ``rightmost'' frame for the content immediately preceding
  // aIndexInContainer, following continuations if necessary.
  nsIFrame* FindPreviousSibling(nsIContent* aContainer,
                                PRInt32     aIndexInContainer,
                                nsIContent* aChild);

  // Find the frame for the content node immediately following aIndexInContainer.
  nsIFrame* FindNextSibling(nsIContent* aContainer,
                            PRInt32     aIndexInContainer,
                            nsIContent* aChild);

  // see if aContent and aSibling are legitimate siblings due to restrictions
  // imposed by table columns
  // XXXbz this code is generally wrong, since the frame for aContent
  // may be constructed based on tag, not based on aDisplay!
  PRBool IsValidSibling(nsIFrame*              aSibling,
                        nsIContent*            aContent,
                        PRUint8&               aDisplay);
  
  /**
   * Find the ``rightmost'' frame for the anonymous content immediately
   * preceding aChild, following continuation if necessary.
   */
  nsIFrame*
  FindPreviousAnonymousSibling(nsIContent*   aContainer,
                               nsIContent*   aChild);

  /**
   * Find the frame for the anonymous content immediately following
   * aChild.
   */
  nsIFrame*
  FindNextAnonymousSibling(nsIContent*   aContainer,
                           nsIContent*   aChild);

  void QuotesDirty() {
    NS_PRECONDITION(mUpdateCount != 0, "Instant quote updates are bad news");
    mQuotesDirty = PR_TRUE;
  }

  void CountersDirty() {
    NS_PRECONDITION(mUpdateCount != 0, "Instant counter updates are bad news");
    mCountersDirty = PR_TRUE;
  }

public:
  struct RestyleData;
  friend struct RestyleData;

  struct RestyleData {
    nsReStyleHint mRestyleHint;  // What we want to restyle
    nsChangeHint  mChangeHint;   // The minimal change hint for "self"
  };

  struct RestyleEnumerateData : public RestyleData {
    nsCOMPtr<nsIContent> mContent;
  };

  class RestyleEvent;
  friend class RestyleEvent;

  class RestyleEvent : public nsRunnable {
  public:
    NS_DECL_NSIRUNNABLE
    RestyleEvent(nsCSSFrameConstructor *aConstructor)
      : mConstructor(aConstructor) {
      NS_PRECONDITION(aConstructor, "Must have a constructor!");
    }
    void Revoke() { mConstructor = nsnull; }
  private:
    nsCSSFrameConstructor *mConstructor;
  };

  friend class nsFrameConstructorState;

private:

  class LazyGenerateChildrenEvent;
  friend class LazyGenerateChildrenEvent;

  // See comments of nsCSSFrameConstructor::AddLazyChildren()
  class LazyGenerateChildrenEvent : public nsRunnable {
  public:
    NS_DECL_NSIRUNNABLE
    LazyGenerateChildrenEvent(nsIContent *aContent,
                              nsIPresShell *aPresShell,
                              nsLazyFrameConstructionCallback* aCallback,
                              void* aArg)
      : mContent(aContent), mPresShell(aPresShell), mCallback(aCallback), mArg(aArg)
    {}

  private:
    nsCOMPtr<nsIContent> mContent;
    nsCOMPtr<nsIPresShell> mPresShell;
    nsLazyFrameConstructionCallback* mCallback;
    void* mArg;
  };

  nsIDocument*        mDocument;  // Weak ref
  nsIPresShell*       mPresShell; // Weak ref

  // See the comment at the start of ConstructRootFrame for more details
  // about the following frames.
  
  // This is not the real CSS 2.1 "initial containing block"! It is just
  // the outermost frame for the root element.
  nsIFrame*           mInitialContainingBlock;
  // This is the frame for the root element that has no pseudo-element style.
  nsIFrame*           mRootElementStyleFrame;
  // This is the containing block for fixed-pos frames --- the viewport
  nsIFrame*           mFixedContainingBlock;
  // This is the containing block that contains the root element ---
  // the real "initial containing block" according to CSS 2.1.
  nsIFrame*           mDocElementContainingBlock;
  nsIFrame*           mGfxScrollFrame;
  nsIFrame*           mPageSequenceFrame;
  nsQuoteList         mQuoteList;
  nsCounterManager    mCounterManager;
  PRUint16            mUpdateCount;
  PRUint32            mFocusSuppressCount;
  PRPackedBool        mQuotesDirty : 1;
  PRPackedBool        mCountersDirty : 1;
  PRPackedBool        mIsDestroyingFrameTree : 1;
  PRPackedBool        mRebuildAllStyleData : 1;
  // This is true if mDocElementContainingBlock supports absolute positioning
  PRPackedBool        mHasRootAbsPosContainingBlock : 1;
  PRUint32            mHoverGeneration;
  nsChangeHint        mRebuildAllExtraHint;

  nsRevocableEventPtr<RestyleEvent> mRestyleEvent;

  nsCOMPtr<nsILayoutHistoryState> mTempFrameTreeState;

  nsDataHashtable<nsISupportsHashKey, RestyleData> mPendingRestyles;

  static nsIXBLService * gXBLService;
};

#endif /* nsCSSFrameConstructor_h___ */
