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

struct nsFindFrameHint
{
  nsIFrame *mPrimaryFrameForPrevSibling;  // weak ref to the primary frame for the content for which we need a frame
  nsFindFrameHint() : mPrimaryFrameForPrevSibling(nsnull) { }
};

class nsFrameConstructorState;
class nsFrameConstructorSaveState;
  
class nsCSSFrameConstructor
{
public:
  nsCSSFrameConstructor(nsIDocument *aDocument, nsIPresShell* aPresShell);
  ~nsCSSFrameConstructor(void) { }

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
                           nsILayoutHistoryState* aFrameState,
                           PRBool                 aInReinsertContent);

  nsresult ContentRemoved(nsIContent*     aContainer,
                          nsIContent*     aChild,
                          PRInt32         aIndexInContainer,
                          PRBool          aInReinsertContent);

  nsresult CharacterDataChanged(nsIContent*     aContent,
                                PRBool          aAppend);

  nsresult ContentStatesChanged(nsIContent*     aContent1,
                                nsIContent*     aContent2,
                                PRInt32         aStateMask);

  // Should be called when a frame is going to be destroyed and
  // WillDestroyFrameTree hasn't been called yet.
  void NotifyDestroyingFrame(nsIFrame* aFrame);

  nsresult AttributeChanged(nsIContent* aContent,
                            PRInt32     aNameSpaceID,
                            nsIAtom*    aAttribute,
                            PRInt32     aModType);

  void BeginUpdate() { ++mUpdateCount; }
  void EndUpdate();
  void RecalcQuotesAndCounters();

  void WillDestroyFrameTree();

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
  // Note: It's the caller's responsibility to make sure to wrap a
  // ProcessPendingRestyles call in a view update batch.
  // ProcessPendingRestyles will handle calling ProcessAttachedQueue() on the
  // binding manager.
  void ProcessPendingRestyles();

  void PostRestyleEvent(nsIContent* aContent, nsReStyleHint aRestyleHint,
                        nsChangeHint aMinChangeHint);

  // Request to create a continuing frame
  nsresult CreateContinuingFrame(nsPresContext* aPresContext,
                                 nsIFrame*       aFrame,
                                 nsIFrame*       aParentFrame,
                                 nsIFrame**      aContinuingFrame,
                                 PRBool          aIsFluid = PR_TRUE);

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

  nsIFrame* GetInitialContainingBlock() { return mInitialContainingBlock; }
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
                              nsIFrame*&     aPageContentFrame);

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
   * @param [out] aNewContent the content node we create
   * @param [out] aNewFrame the new frame we create
   */
  nsresult CreateAttributeContent(nsIContent* aParentContent,
                                  nsIFrame* aParentFrame,
                                  PRInt32 aAttrNamespace,
                                  nsIAtom* aAttrName,
                                  nsStyleContext* aStyleContext,
                                  nsIContent** aNewContent,
                                  nsIFrame** aNewFrame);
  
  nsresult CreateGeneratedFrameFor(nsIFrame*             aParentFrame,
                                   nsIContent*           aContent,
                                   nsStyleContext*       aStyleContext,
                                   const nsStyleContent* aStyleContent,
                                   PRUint32              aContentIndex,
                                   nsIFrame**            aFrame);

  PRBool CreateGeneratedContentFrame(nsFrameConstructorState& aState,
                                     nsIFrame*                aFrame,
                                     nsIContent*              aContent,
                                     nsStyleContext*          aStyleContext,
                                     nsIAtom*                 aPseudoElement,
                                     nsIFrame**               aResult);

  nsresult AppendFrames(const nsFrameConstructorState& aState,
                        nsIContent*                    aContainer,
                        nsIFrame*                      aParentFrame,
                        nsIFrame*                      aFrameList,
                        nsIFrame*                      aAfterFrame);

  // BEGIN TABLE SECTION
  /**
   * ConstructTableFrame will construct the outer and inner table frames and
   * return them.  Unless aIsPseudo is PR_TRUE, it will put the inner frame in
   * the child list of the outer frame, and will put any pseudo frames it had
   * to create into aChildItems.  The newly-created outer frame will either be
   * in aChildItems or a descendant of a pseudo in aChildItems (unless it's
   * positioned or floated, in which case its placeholder will be in
   * aChildItems).  If aAllowOutOfFlow is false, the table frame will be forced
   * to be in-flow no matter what its float or position values are.
   */ 
  nsresult ConstructTableFrame(nsFrameConstructorState& aState,
                               nsIContent*              aContent,
                               nsIFrame*                aContentParent,
                               nsStyleContext*          aStyleContext,
                               PRInt32                  aNameSpaceID,
                               PRBool                   aIsPseudo,
                               nsFrameItems&            aChildItems,
                               PRBool                   aAllowOutOfFlow,
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
                                            nsPresContext*  aPresContext,
                                            nsFrameManager*  aFrameManager,
                                            nsIContent*      aContent,
                                            nsIFrame*        aFrame,
                                            nsStyleContext*  aStyleContext,
                                            nsIFrame*        aParentFrame,
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
                                nsFrameItems&            aFrameItems);

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
  nsresult TestSVGConditions(nsIContent* aContent,
                             PRBool&     aHasRequiredExtensions,
                             PRBool&     aHasRequiredFeatures,
                             PRBool&     aHasSystemLanguage);
 
  nsresult SVGSwitchProcessChildren(nsFrameConstructorState& aState,
                                    nsIContent*              aContent,
                                    nsIFrame*                aFrame,
                                    nsFrameItems&            aFrameItems);

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
                            nsFrameItems&            aFrameItems);

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
   * These functions are used when we start frame creation from a non-root
   * element. They should recreate the same state that we would have
   * arrived at if we had built frames from the root frame to aFrame.
   * Therefore, any calls to PushFloatContainingBlock and
   * PushAbsoluteContainingBlock during frame construction should get
   * corresponding logic in these functions.
   */
  nsIFrame* GetAbsoluteContainingBlock(nsIFrame* aFrame);
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

  PRBool MaybeRecreateContainerForIBSplitterFrame(nsIFrame* aFrame, nsresult* aResult);

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

  PRBool HaveFirstLetterStyle(nsIContent*      aContent,
                              nsStyleContext*  aStyleContext);

  PRBool HaveFirstLineStyle(nsIContent*      aContent,
                            nsStyleContext*  aStyleContext);

  void HaveSpecialBlockStyle(nsIContent*      aContent,
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

  nsresult ProcessInlineChildren(nsFrameConstructorState& aState,
                                 nsIContent*              aContent,
                                 nsIFrame*                aFrame,
                                 PRBool                   aCanHaveGeneratedContent,
                                 nsFrameItems&            aFrameItems,
                                 PRBool*                  aKidsAllInline);

  PRBool AreAllKidsInline(nsIFrame* aFrameList);

  PRBool WipeContainingBlock(nsFrameConstructorState& aState,
                             nsIFrame*                blockContent,
                             nsIFrame*                aFrame,
                             nsIFrame*                aFrameList);

  PRBool NeedSpecialFrameReframe(nsIContent*      aParent1,
                                 nsIContent*      aParent2,
                                 nsIFrame*&       aParentFrame,
                                 nsIContent*      aChild,
                                 PRInt32          aIndexInContainer,
                                 nsIFrame*&       aPrevSibling,
                                 nsIFrame*        aNextSibling);

  nsresult ReframeContainingBlock(nsIFrame* aFrame);

  nsresult StyleChangeReflow(nsIFrame* aFrame, nsIAtom* aAttribute);

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

  nsresult RemoveFixedItems(const nsFrameConstructorState& aState);

  // Find the ``rightmost'' frame for the content immediately preceding
  // aIndexInContainer, following continuations if necessary. If aChild is
  // not null, make sure it passes the call to IsValidSibling
  nsIFrame* FindPreviousSibling(nsIContent*       aContainer,
                                nsIFrame*         aContainerFrame,
                                PRInt32           aIndexInContainer,
                                const nsIContent* aChild = nsnull);

  // Find the frame for the content node immediately following aIndexInContainer.
  // If aChild is not null, make sure it passes the call to IsValidSibling
  nsIFrame* FindNextSibling(nsIContent*       aContainer,
                            nsIFrame*         aContainerFrame,
                            PRInt32           aIndexInContainer,
                            const nsIContent* aChild = nsnull);

  // see if aContent and aSibling are legitimate siblings due to restrictions
  // imposed by table columns
  PRBool IsValidSibling(nsIFrame*              aParentFrame,
                        const nsIFrame&        aSibling,
                        PRUint8                aSiblingDisplay,
                        nsIContent&            aContent,
                        PRUint8&               aDisplay);

  void QuotesDirty() {
    if (mUpdateCount != 0)
      mQuotesDirty = PR_TRUE;
    else
      mQuoteList.RecalcAll();
  }

  void CountersDirty() {
    if (mUpdateCount != 0)
      mCountersDirty = PR_TRUE;
    else
      mCounterManager.RecalcAll();
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
  nsIDocument*        mDocument;  // Weak ref
  nsIPresShell*       mPresShell; // Weak ref

  nsIFrame*           mInitialContainingBlock;
  nsIFrame*           mFixedContainingBlock;
  nsIFrame*           mDocElementContainingBlock;
  nsIFrame*           mGfxScrollFrame;
  nsIFrame*           mPageSequenceFrame;
  nsQuoteList         mQuoteList;
  nsCounterManager    mCounterManager;
  PRUint16            mUpdateCount;
  PRPackedBool        mQuotesDirty : 1;
  PRPackedBool        mCountersDirty : 1;
  PRPackedBool        mInitialContainingBlockIsAbsPosContainer : 1;
  PRPackedBool        mIsDestroyingFrameTree : 1;

  nsRevocableEventPtr<RestyleEvent> mRestyleEvent;

  nsCOMPtr<nsILayoutHistoryState> mTempFrameTreeState;

  nsDataHashtable<nsISupportsHashKey, RestyleData> mPendingRestyles;

  static nsIXBLService * gXBLService;
};

#endif /* nsCSSFrameConstructor_h___ */
