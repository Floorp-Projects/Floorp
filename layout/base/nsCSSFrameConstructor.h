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

class nsIDocument;
struct nsFrameItems;
struct nsAbsoluteItems;
class nsStyleContext;
struct nsStyleContent;
struct nsStyleDisplay;
class nsIPresShell;
class nsFrameManager;
class nsIDOMHTMLSelectElement;
class nsPresContext;
class nsStyleChangeList;
class nsIFrame;
struct nsGenConInitializer;
class ChildIterator;
class nsICSSAnonBoxPseudo;

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
  }

  struct RestyleData;
  friend struct RestyleData;

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
  nsresult ConstructRootFrame(nsIFrame** aNewFrame);

  nsresult ReconstructDocElementHierarchy();

  nsresult ContentAppended(nsIContent*     aContainer,
                           PRInt32         aNewIndexInContainer);

  nsresult ContentInserted(nsIContent*            aContainer,
                           nsIContent*            aChild,
                           PRInt32                aIndexInContainer,
                           nsILayoutHistoryState* aFrameState);

  enum RemoveFlags { REMOVE_CONTENT, REMOVE_FOR_RECONSTRUCTION };
  nsresult ContentRemoved(nsIContent* aContainer,
                          nsIContent* aChild,
                          PRInt32     aIndexInContainer,
                          RemoveFlags aFlags,
                          PRBool*     aDidReconstruct);

  nsresult CharacterDataChanged(nsIContent* aContent,
                                CharacterDataChangeInfo* aInfo);

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

  // Gets called when the presshell is destroying itself and also
  // when we tear down our frame tree to reconstruct it
  void WillDestroyFrameTree();

  // Get an integer that increments every time there is a style change
  // as a result of a change to the :hover content state.
  PRUint32 GetHoverGeneration() const { return mHoverGeneration; }

  // Note: It's the caller's responsibility to make sure to wrap a
  // ProcessRestyledFrames call in a view update batch and a script blocker.
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

  void ProcessPendingRestyleTable(
           nsDataHashtable<nsISupportsHashKey, RestyleData>& aRestyles);

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
  // ProcessPendingRestyles call in a view update batch and a script blocker.
  // This function does not call ProcessAttachedQueue() on the binding manager.
  // If the caller wants that to happen synchronously, it needs to handle that
  // itself.
  void ProcessPendingRestyles();
  
  // Rebuilds all style data by throwing out the old rule tree and
  // building a new one, and additionally applying aExtraHint (which
  // must not contain nsChangeHint_ReconstructFrame) to the root frame.
  void RebuildAllStyleData(nsChangeHint aExtraHint);

  // See PostRestyleEventCommon below.
  void PostRestyleEvent(nsIContent* aContent, nsReStyleHint aRestyleHint,
                        nsChangeHint aMinChangeHint)
  {
    nsPresContext *presContext = mPresShell->GetPresContext();
    if (presContext) {
      PostRestyleEventCommon(aContent, aRestyleHint, aMinChangeHint,
                             presContext->IsProcessingAnimationStyleChange());
    }
  }

  // See PostRestyleEventCommon below.
  void PostAnimationRestyleEvent(nsIContent* aContent,
                                 nsReStyleHint aRestyleHint,
                                 nsChangeHint aMinChangeHint)
  {
    PostRestyleEventCommon(aContent, aRestyleHint, aMinChangeHint, PR_TRUE);
  }
private:
  /**
   * Notify the frame constructor that a content node needs to have its
   * style recomputed.
   * @param aContent: The content node to be restyled.
   * @param aRestyleHint: Which nodes need to have selector matching run
   *                      on them.
   * @param aMinChangeHint: A minimum change hint for aContent and its
   *                        descendants.
   * @param aForAnimation: Whether the style should be computed with or
   *                       without animation data.  Animation code
   *                       sometimes needs to pass true; other code
   *                       should generally pass the the pres context's
   *                       IsProcessingAnimationStyleChange() value
   *                       (which is the default value).
   */
  void PostRestyleEventCommon(nsIContent* aContent, nsReStyleHint aRestyleHint,
                              nsChangeHint aMinChangeHint,
                              PRBool aForAnimation);
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

  // GetInitialContainingBlock() is deprecated in favor of GetRootElementFrame();
  // nsIFrame* GetInitialContainingBlock() { return mRootElementFrame; }
  // This returns the outermost frame for the root element
  nsIFrame* GetRootElementFrame() { return mRootElementFrame; }
  // This returns the frame for the root element that does not
  // have a psuedo-element style
  nsIFrame* GetRootElementStyleFrame() { return mRootElementStyleFrame; }
  nsIFrame* GetPageSequenceFrame() { return mPageSequenceFrame; }

  // Get the frame that is the parent of the root element.
  nsIFrame* GetDocElementContainingBlock()
    { return mDocElementContainingBlock; }

private:
  struct FrameConstructionItem;
  class FrameConstructionItemList;

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
  already_AddRefed<nsStyleContext>
  ResolveStyleContext(nsStyleContext* aParentStyleContext,
                      nsIContent* aContent);

  // Construct a frame for aContent and put it in aFrameItems.  This should
  // only be used in cases when it's known that the frame won't need table
  // pseudo-frame construction and the like.
  nsresult ConstructFrame(nsFrameConstructorState& aState,
                          nsIContent*              aContent,
                          nsIFrame*                aParentFrame,
                          nsFrameItems&            aFrameItems);

  // Add the frame construction items for the given aContent and aParentFrame
  // to the list.  This might add more than one item in some rare cases.
  // aContentIndex is the index of aContent in its parent's child list,
  // or -1 if it's not in its parent's child list, or the index is
  // not known. If the index is not known, optimizations that
  // may suppress the construction of white-space-only text frames
  // may not be performed.
  void AddFrameConstructionItems(nsFrameConstructorState& aState,
                                 nsIContent*              aContent,
                                 PRInt32                  aContentIndex,
                                 nsIFrame*                aParentFrame,
                                 FrameConstructionItemList& aItems);

  // Construct the frames for the document element.  This must always return a
  // singe new frame (which may, of course, have a bunch of kids).
  // XXXbz no need to return a frame here, imo.
  nsresult ConstructDocElementFrame(nsIContent*              aDocElement,
                                    nsILayoutHistoryState*   aFrameState,
                                    nsIFrame**               aNewFrame);

  // Set up our mDocElementContainingBlock correctly for the given root
  // content.
  nsresult SetUpDocElementContainingBlock(nsIContent* aDocElement);

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
  already_AddRefed<nsIContent> CreateGenConTextNode(nsFrameConstructorState& aState,
                                                    const nsString& aString,
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
  already_AddRefed<nsIContent> CreateGeneratedContent(nsFrameConstructorState& aState,
                                                      nsIContent*     aParentContent,
                                                      nsStyleContext* aStyleContext,
                                                      PRUint32        aContentIndex);

  // aFrame may be null; this method doesn't use it directly in any case.
  void CreateGeneratedContentItem(nsFrameConstructorState& aState,
                                  nsIFrame*                aFrame,
                                  nsIContent*              aContent,
                                  nsStyleContext*          aStyleContext,
                                  nsIAtom*                 aPseudoElement,
                                  FrameConstructionItemList& aItems);

  // This method can change aFrameList: it can chop off the end and put it in a
  // special sibling of aParentFrame.  It can also change aState by moving some
  // floats out of it.  aPrevSibling must be the frame after which aFrameList
  // is to be placed on aParentFrame's principal child list.  It may be null if
  // aFrameList is being added at the beginning of the child list.
  nsresult AppendFrames(nsFrameConstructorState&       aState,
                        nsIFrame*                      aParentFrame,
                        nsFrameItems&                  aFrameList,
                        nsIFrame*                      aPrevSibling);

  // BEGIN TABLE SECTION
  /**
   * Construct an outer table frame.  This is the FrameConstructionData
   * callback used for the job.
   */
  nsresult ConstructTable(nsFrameConstructorState& aState,
                          FrameConstructionItem&   aItem,
                          nsIFrame*                aParentFrame,
                          const nsStyleDisplay*    aDisplay,
                          nsFrameItems&            aFrameItems,
                          nsIFrame**               aNewFrame);

  /**
   * FrameConstructionData callback used for constructing table rows.
   */
  nsresult ConstructTableRow(nsFrameConstructorState& aState,
                             FrameConstructionItem&   aItem,
                             nsIFrame*                aParentFrame,
                             const nsStyleDisplay*    aStyleDisplay,
                             nsFrameItems&            aFrameItems,
                             nsIFrame**               aNewFrame);

  /**
   * FrameConstructionData callback used for constructing table columns.
   */
  nsresult ConstructTableCol(nsFrameConstructorState& aState,
                             FrameConstructionItem&   aItem,
                             nsIFrame*                aParentFrame,
                             const nsStyleDisplay*    aStyleDisplay,
                             nsFrameItems&            aFrameItems,
                             nsIFrame**               aNewFrame);

  /**
   * FrameConstructionData callback used for constructing table cells.
   */
  nsresult ConstructTableCell(nsFrameConstructorState& aState,
                              FrameConstructionItem&   aItem,
                              nsIFrame*                aParentFrame,
                              const nsStyleDisplay*    aStyleDisplay,
                              nsFrameItems&            aFrameItems,
                              nsIFrame**               aNewFrame);

private:
  /* An enum of possible parent types for anonymous table object construction */
  enum ParentType {
    eTypeBlock = 0, /* This includes all non-table-related frames */
    eTypeRow,
    eTypeRowGroup,
    eTypeColGroup,
    eTypeTable,
    eParentTypeCount
  };

  /* 3 bits is enough to handle our ParentType values */
#define FCDATA_PARENT_TYPE_OFFSET 29
  /* Macro to get the desired parent type out of an mBits member of
     FrameConstructionData */
#define FCDATA_DESIRED_PARENT_TYPE(_bits)           \
  ParentType((_bits) >> FCDATA_PARENT_TYPE_OFFSET)
  /* Macro to create FrameConstructionData bits out of a desired parent type */
#define FCDATA_DESIRED_PARENT_TYPE_TO_BITS(_type)     \
  (((PRUint32)(_type)) << FCDATA_PARENT_TYPE_OFFSET)

  /* Get the parent type that aParentFrame has. */
  static ParentType GetParentType(nsIFrame* aParentFrame) {
    return GetParentType(aParentFrame->GetType());
  }

  /* Get the parent type for the given nsIFrame type atom */
  static ParentType GetParentType(nsIAtom* aFrameType);

  /* A constructor function that just creates an nsIFrame object.  The caller
     is responsible for initializing the object, adding it to frame lists,
     constructing frames for the children, etc.

     @param nsIPresShell the presshell whose arena should be used to allocate
                         the frame.
     @param nsStyleContext the style context to use for the frame. */
  typedef nsIFrame* (* FrameCreationFunc)(nsIPresShell*, nsStyleContext*);

  /* A function that can be used to get a FrameConstructionData.  Such
     a function is allowed to return null.

     @param nsIContent the node for which the frame is being constructed.
     @param nsStyleContext the style context to be used for the frame.
  */
  struct FrameConstructionData;
  typedef const FrameConstructionData*
    (* FrameConstructionDataGetter)(nsIContent*, nsStyleContext*);

  /* A constructor function that's used for complicated construction tasks.
     This is expected to create the new frame, initialize it, add whatever
     needs to be added to aFrameItems (XXXbz is that really necessary?  Could
     caller add?  Might there be cases when *aNewFrame or its placeholder is
     not the thing that ends up in aFrameItems?  If not, would it be safe to do
     the add into the frame construction state after processing kids?  Look
     into this as a followup!), process children as needed, etc.  It is NOT
     expected to deal with the primary frame map.

     @param aState the frame construction state to use.
     @param aItem the frame construction item to use
     @param aParentFrame the frame to set as the parent of the
                         newly-constructed frame.
     @param aStyleDisplay the display struct from aItem's mStyleContext
     @param aFrameItems the frame list to add the new frame (or its
                        placeholder) to.
     @param aFrame out param handing out the frame that was constructed.  This
                   frame is what the caller will add to the primary frame map.
  */
  typedef nsresult
    (nsCSSFrameConstructor::* FrameFullConstructor)(nsFrameConstructorState& aState,
                                                    FrameConstructionItem& aItem,
                                                    nsIFrame* aParentFrame,
                                                    const nsStyleDisplay* aStyleDisplay,
                                                    nsFrameItems& aFrameItems,
                                                    nsIFrame** aFrame);

  /* Bits that modify the way a FrameConstructionData is handled */

  /* If the FCDATA_SKIP_FRAMEMAP bit is set, then the frame created should not
     be added to the primary frame map.  This flag might get ignored when used
     with FCDATA_MAY_NEED_SCROLLFRAME, since scrollframe construction will add
     to the frame map. */
#define FCDATA_SKIP_FRAMEMAP 0x1
  /* If the FCDATA_FUNC_IS_DATA_GETTER bit is set, then the mFunc of the
     FrameConstructionData is a getter function that can be used to get the
     actual FrameConstructionData to use. */
#define FCDATA_FUNC_IS_DATA_GETTER 0x2
  /* If the FCDATA_FUNC_IS_FULL_CTOR bit is set, then the FrameConstructionData
     has an mFullConstructor.  In this case, there is no relevant mData or
     mFunc */
#define FCDATA_FUNC_IS_FULL_CTOR 0x4
  /* If FCDATA_DISALLOW_OUT_OF_FLOW is set, do not allow the frame to
     float or be absolutely positioned.  This can also be used with
     FCDATA_FUNC_IS_FULL_CTOR to indicate what the full-constructor
     function will do. */
#define FCDATA_DISALLOW_OUT_OF_FLOW 0x8
  /* If FCDATA_FORCE_NULL_ABSPOS_CONTAINER is set, make sure to push a
     null absolute containing block before processing children for this
     frame.  If this is not set, the frame will be pushed as the
     absolute containing block as needed, based on its style */
#define FCDATA_FORCE_NULL_ABSPOS_CONTAINER 0x10
#ifdef MOZ_MATHML
  /* If FCDATA_WRAP_KIDS_IN_BLOCKS is set, the inline kids of the frame
     will be wrapped in blocks.  This is only usable for MathML at the
     moment. */
#define FCDATA_WRAP_KIDS_IN_BLOCKS 0x20
#endif /* MOZ_MATHML */
  /* If FCDATA_SUPPRESS_FRAME is set, no frame should be created for the
     content.  If this bit is set, nothing else in the struct needs to be
     set. */
#define FCDATA_SUPPRESS_FRAME 0x40
  /* If FCDATA_MAY_NEED_SCROLLFRAME is set, the new frame should be wrapped in
     a scrollframe if its overflow type so requires.  This flag might override
     FCDATA_SKIP_FRAMEMAP, since scrollframe construction will add to the frame
     map. */
#define FCDATA_MAY_NEED_SCROLLFRAME 0x80
#ifdef MOZ_XUL
  /* If FCDATA_IS_POPUP is set, the new frame is a XUL popup frame.  These need
     some really weird special handling.  */
#define FCDATA_IS_POPUP 0x100
#endif /* MOZ_XUL */
  /* If FCDATA_SKIP_ABSPOS_PUSH is set, don't push this frame as an
     absolute containing block, no matter what its style says. */
#define FCDATA_SKIP_ABSPOS_PUSH 0x200
  /* If FCDATA_FORCE_VIEW is set, then force creation of a view for the frame.
     this is only used if a scrollframe is not created and a full constructor
     isn't used, so this flag shouldn't be used with
     FCDATA_MAY_NEED_SCROLLFRAME or FCDATA_FUNC_IS_FULL_CTOR.  */
#define FCDATA_FORCE_VIEW 0x400
  /* If FCDATA_DISALLOW_GENERATED_CONTENT is set, then don't allow generated
     content when processing kids of this frame.  This should not be used with
     FCDATA_FUNC_IS_FULL_CTOR */
#define FCDATA_DISALLOW_GENERATED_CONTENT 0x800
  /* If FCDATA_IS_TABLE_PART is set, then the frame is some sort of
     table-related thing and we should not attempt to fetch a table-cell parent
     for it if it's inside another table-related frame. */
#define FCDATA_IS_TABLE_PART 0x1000
  /* If FCDATA_IS_INLINE is set, then the frame is a non-replaced CSS
     inline box. */
#define FCDATA_IS_INLINE 0x2000
  /* If FCDATA_IS_LINE_PARTICIPANT is set, the frame is something that will
     return true for IsFrameOfType(nsIFrame::eLineParticipant) */
#define FCDATA_IS_LINE_PARTICIPANT 0x4000
  /* If FCDATA_IS_LINE_BREAK is set, the frame is something that will
     induce a line break boundary before and after itself. */
#define FCDATA_IS_LINE_BREAK 0x8000
  /* If FCDATA_ALLOW_BLOCK_STYLES is set, allow block styles when processing
     children.  This should not be used with FCDATA_FUNC_IS_FULL_CTOR. */
#define FCDATA_ALLOW_BLOCK_STYLES 0x10000
  /* If FCDATA_USE_CHILD_ITEMS is set, then use the mChildItems in the relevant
     FrameConstructionItem instead of trying to process the content's children.
     This can be used with or without FCDATA_FUNC_IS_FULL_CTOR.
     The child items might still need table pseudo processing. */
#define FCDATA_USE_CHILD_ITEMS 0x20000

  /* Structure representing information about how a frame should be
     constructed.  */
  struct FrameConstructionData {
    // Flag bits that can modify the way the construction happens
    PRUint32 mBits;
    // We have exactly one of three types of functions, so use a union for
    // better cache locality for the ones that aren't pointer-to-member.  That
    // one needs to be separate, because we can't cast between it and the
    // others and hence wouldn't be able to initialize the union without a
    // constructor and all the resulting generated code.  See documentation
    // above for FrameCreationFunc, FrameConstructionDataGetter, and
    // FrameFullConstructor to see what the functions would do.
    union Func {
      FrameCreationFunc mCreationFunc;
      FrameConstructionDataGetter mDataGetter;
    } mFunc;
    FrameFullConstructor mFullConstructor;
  };

  /* Structure representing a mapping of an atom to a FrameConstructionData.
     This can be used with non-static atoms, assuming that the nsIAtom* is
     stored somewhere that this struct can point to (that is, a static
     nsIAtom*) and that it's allocated before the struct is ever used. */
  struct FrameConstructionDataByTag {
    // Pointer to nsIAtom* is used because we want to initialize this
    // statically, so before our atom tables are set up.
    const nsIAtom * const * const mTag;
    const FrameConstructionData mData;
  };

  /* Structure representing a mapping of an integer to a
     FrameConstructionData. There are no magic integer values here. */
  struct FrameConstructionDataByInt {
    /* Could be used for display or whatever else */
    const PRInt32 mInt;
    const FrameConstructionData mData;
  };

  /* Structure that has a FrameConstructionData and style context pseudo-type
     for a table pseudo-frame */
  struct PseudoParentData {
    const FrameConstructionData mFCData;
    nsICSSAnonBoxPseudo * const * const mPseudoType;
  };
  /* Array of such structures that we use to properly construct table
     pseudo-frames as needed */
  static const PseudoParentData sPseudoParentData[eParentTypeCount];

  /* A function that takes an integer, content, style context, and array of
     FrameConstructionDataByInts and finds the appropriate frame construction
     data to use and returns it.  This can return null if none of the integers
     match or if the matching integer has a FrameConstructionDataGetter that
     returns null. */
  static const FrameConstructionData*
    FindDataByInt(PRInt32 aInt, nsIContent* aContent,
                  nsStyleContext* aStyleContext,
                  const FrameConstructionDataByInt* aDataPtr,
                  PRUint32 aDataLength);

  /* A function that takes a tag, content, style context, and array of
     FrameConstructionDataByTags and finds the appropriate frame construction
     data to use and returns it.  This can return null if none of the tags
     match or if the matching tag has a FrameConstructionDataGetter that
     returns null. */
  static const FrameConstructionData*
    FindDataByTag(nsIAtom* aTag, nsIContent* aContent,
                  nsStyleContext* aStyleContext,
                  const FrameConstructionDataByTag* aDataPtr,
                  PRUint32 aDataLength);

  /* A class representing a list of FrameConstructionItems */
  class FrameConstructionItemList {
  public:
    FrameConstructionItemList() :
      mInlineCount(0),
      mLineParticipantCount(0),
      mItemCount(0),
      mLineBoundaryAtStart(PR_FALSE),
      mLineBoundaryAtEnd(PR_FALSE),
      mParentHasNoXBLChildren(PR_FALSE)
    {
      PR_INIT_CLIST(&mItems);
      memset(mDesiredParentCounts, 0, sizeof(mDesiredParentCounts));
    }

    ~FrameConstructionItemList() {
      PRCList* cur = PR_NEXT_LINK(&mItems);
      while (cur != &mItems) {
        PRCList* next = PR_NEXT_LINK(cur);
        delete ToItem(cur);
        cur = next;
      }

      // Leaves our mItems pointing to deleted memory in both directions,
      // but that's OK at this point.
    }

    void SetLineBoundaryAtStart(PRBool aBoundary) { mLineBoundaryAtStart = aBoundary; }
    void SetLineBoundaryAtEnd(PRBool aBoundary) { mLineBoundaryAtEnd = aBoundary; }
    void SetParentHasNoXBLChildren(PRBool aHasNoXBLChildren) {
      mParentHasNoXBLChildren = aHasNoXBLChildren;
    }
    PRBool HasLineBoundaryAtStart() { return mLineBoundaryAtStart; }
    PRBool HasLineBoundaryAtEnd() { return mLineBoundaryAtEnd; }
    PRBool ParentHasNoXBLChildren() { return mParentHasNoXBLChildren; }
    PRBool IsEmpty() const { return PR_CLIST_IS_EMPTY(&mItems); }
    PRBool AnyItemsNeedBlockParent() const { return mLineParticipantCount != 0; }
    PRBool AreAllItemsInline() const { return mInlineCount == mItemCount; }
    PRBool IsStartInline() const {
      NS_ASSERTION(!IsEmpty(), "Someone forgot to check IsEmpty()");
      return ToItem(PR_LIST_HEAD(&mItems))->mHasInlineEnds;
    }
    PRBool IsEndInline() const {
      NS_ASSERTION(!IsEmpty(), "Someone forgot to check IsEmpty()");
      return ToItem(PR_LIST_TAIL(&mItems))->mHasInlineEnds;
    }
    PRBool AllWantParentType(ParentType aDesiredParentType) const {
      return mDesiredParentCounts[aDesiredParentType] == mItemCount;
    }

    // aContentIndex is the index of aContent in its parent's child list,
    // or -1 if aContent is not in its parent's child list, or the index
    // is not known.
    FrameConstructionItem* AppendItem(const FrameConstructionData* aFCData,
                                      nsIContent* aContent,
                                      nsIAtom* aTag,
                                      PRInt32 aNameSpaceID,
                                      PRInt32 aContentIndex,
                                      already_AddRefed<nsStyleContext> aStyleContext)
    {
      FrameConstructionItem* item =
        new FrameConstructionItem(aFCData, aContent, aTag, aNameSpaceID,
                                  aContentIndex, aStyleContext);
      if (item) {
        PR_APPEND_LINK(item, &mItems);
        ++mItemCount;
        ++mDesiredParentCounts[item->DesiredParentType()];
      } else {
        // Clean up the style context
        nsRefPtr<nsStyleContext> sc(aStyleContext);
      }
      return item;
    }

    void InlineItemAdded() { ++mInlineCount; }
    void LineParticipantItemAdded() { ++mLineParticipantCount; }

    class Iterator;
    friend class Iterator;

    class Iterator {
    public:
      Iterator(FrameConstructionItemList& list) :
        mCurrent(PR_NEXT_LINK(&list.mItems)),
        mEnd(&list.mItems),
        mList(list)
      {}
      Iterator(const Iterator& aOther) :
        mCurrent(aOther.mCurrent),
        mEnd(aOther.mEnd),
        mList(aOther.mList)
      {}

      PRBool operator==(const Iterator& aOther) const {
        NS_ASSERTION(mEnd == aOther.mEnd, "Iterators for different lists?");
        return mCurrent == aOther.mCurrent;
      }
      PRBool operator!=(const Iterator& aOther) const {
        return !(*this == aOther);
      }
      Iterator& operator=(const Iterator& aOther) {
        NS_ASSERTION(mEnd == aOther.mEnd, "Iterators for different lists?");
        mCurrent = aOther.mCurrent;
        return *this;
      }

      FrameConstructionItemList* List() {
        return &mList;
      }

      operator FrameConstructionItem& () {
        return item();
      }

      FrameConstructionItem& item() {
        return *FrameConstructionItemList::ToItem(mCurrent);
      }
      PRBool IsDone() const { return mCurrent == mEnd; }
      PRBool AtStart() const { return mCurrent == PR_NEXT_LINK(mEnd); }
      void Next() {
        NS_ASSERTION(!IsDone(), "Should have checked IsDone()!");
        mCurrent = PR_NEXT_LINK(mCurrent);
      }
      void Prev() {
        NS_ASSERTION(!AtStart(), "Should have checked AtStart()!");
        mCurrent = PR_PREV_LINK(mCurrent);
      }
      void SetToEnd() { mCurrent = mEnd; }

      // Skip over all items that want a parent type different from the given
      // one.  Return whether the iterator is done after doing that.  The
      // iterator must not be done when this is called.
      inline PRBool SkipItemsWantingParentType(ParentType aParentType);

      // Skip over whitespace.  Return whether the iterator is done after doing
      // that.  The iterator must not be done, and must be pointing to a
      // whitespace item when this is called.
      inline PRBool SkipWhitespace();

      // Remove the item pointed to by this iterator from its current list and
      // Append it to aTargetList.  This iterator is advanced to point to the
      // next item in its list.  aIter must not be done.  aOther must not be
      // the list this iterator is iterating over..
      void AppendItemToList(FrameConstructionItemList& aTargetList);

      // As above, but moves all items starting with this iterator until we
      // get to aEnd; the item pointed to by aEnd is not stolen.  This method
      // might have optimizations over just looping and doing StealItem for
      // some special cases.  After this method returns, this iterator will
      // point to the item aEnd points to now; aEnd is not modified.
      // aTargetList must not be the list this iterator is iterating over.
      void AppendItemsToList(const Iterator& aEnd,
                             FrameConstructionItemList& aTargetList);

      // Insert aItem in this iterator's list right before the item pointed to
      // by this iterator.  After the insertion, this iterator will continue to
      // point to the item it now points to (the one just after the
      // newly-inserted item).  This iterator is allowed to be done; in that
      // case this call just appends the given item to the list.
      void InsertItem(FrameConstructionItem* aItem);

      // Delete the items between this iterator and aEnd, including the item
      // this iterator currently points to but not including the item pointed
      // to by aEnd.  When this returns, this iterator will point to the same
      // item as aEnd.  This iterator must not equal aEnd when this method is
      // called.
      void DeleteItemsTo(const Iterator& aEnd);

    private:
      PRCList* mCurrent;
      PRCList* mEnd;
      FrameConstructionItemList& mList;
    };

  private:
    static FrameConstructionItem* ToItem(PRCList* item) {
      return static_cast<FrameConstructionItem*>(item);
    }

    // Adjust our various counts for aItem being added or removed.  aDelta
    // should be either +1 or -1 depending on which is happening.
    void AdjustCountsForItem(FrameConstructionItem* aItem, PRInt32 aDelta);

    PRCList mItems;
    PRUint32 mInlineCount;
    PRUint32 mLineParticipantCount;
    PRUint32 mItemCount;
    PRUint32 mDesiredParentCounts[eParentTypeCount];
    // True if there is guaranteed to be a line boundary before the
    // frames created by these items
    PRPackedBool mLineBoundaryAtStart;
    // True if there is guaranteed to be a line boundary after the
    // frames created by these items
    PRPackedBool mLineBoundaryAtEnd;
    // True if the parent is guaranteed to have no XBL anonymous children
    PRPackedBool mParentHasNoXBLChildren;
  };

  typedef FrameConstructionItemList::Iterator FCItemIterator;

  /* A struct representing an item for which frames might need to be
   * constructed.  This contains all the information needed to construct the
   * frame other than the parent frame and whatever would be stored in the
   * frame constructor state. */
  struct FrameConstructionItem : public PRCList {
    // No need to PR_INIT_CLIST in the constructor because the only
    // place that creates us immediately appends us.
    FrameConstructionItem(const FrameConstructionData* aFCData,
                          nsIContent* aContent,
                          nsIAtom* aTag,
                          PRInt32 aNameSpaceID,
                          PRInt32 aContentIndex,
                          already_AddRefed<nsStyleContext> aStyleContext) :
      mFCData(aFCData), mContent(aContent), mTag(aTag),
      mNameSpaceID(aNameSpaceID), mContentIndex(aContentIndex),
      mStyleContext(aStyleContext),
      mIsText(PR_FALSE), mIsGeneratedContent(PR_FALSE),
      mIsRootPopupgroup(PR_FALSE), mIsAllInline(PR_FALSE),
      mHasInlineEnds(PR_FALSE), mIsPopup(PR_FALSE),
      mIsLineParticipant(PR_FALSE)
    {}
    ~FrameConstructionItem() {
      if (mIsGeneratedContent) {
        mContent->UnbindFromTree();
        NS_RELEASE(mContent);
      }
    }

    ParentType DesiredParentType() {
      return FCDATA_DESIRED_PARENT_TYPE(mFCData->mBits);
    }

    // Don't call this unless the frametree really depends on the answer!
    // Especially so for generated content, where we don't want to reframe
    // things.
    PRBool IsWhitespace() const;

    PRBool IsLineBoundary() const {
      return !mHasInlineEnds || (mFCData->mBits & FCDATA_IS_LINE_BREAK);
    }

    // The FrameConstructionData to use.
    const FrameConstructionData* mFCData;
    // The nsIContent node to use when initializing the new frame.
    nsIContent* mContent;
    // The XBL-resolved tag name to use for frame construction.
    nsIAtom* mTag;
    // The XBL-resolved namespace to use for frame construction.
    PRInt32 mNameSpaceID;
    // The index of mContent in its parent's child list, or -1 if it's
    // not in the parent's child list or not known.
    PRInt32 mContentIndex;
    // The style context to use for creating the new frame.
    nsRefPtr<nsStyleContext> mStyleContext;
    // Whether this is a text content item.
    PRPackedBool mIsText;
    // Whether this is a generated content container.
    // If it is, mContent is a strong pointer.
    PRPackedBool mIsGeneratedContent;
    // Whether this is an item for the root popupgroup.
    PRPackedBool mIsRootPopupgroup;
    // Whether construction from this item will create only frames that are
    // IsInlineOutside() in the principal child list.
    PRPackedBool mIsAllInline;
    // Whether construction from this item will give leading and trailing
    // inline frames.  This is equal to mIsAllInline, except for inline frame
    // items, where it's always true, whereas mIsAllInline might be false due
    // to {ib} splits.
    PRPackedBool mHasInlineEnds;
    // Whether construction from this item will create a popup that needs to
    // go into the global popup items.
    PRPackedBool mIsPopup;
    // Whether this item should be treated as a line participant
    PRPackedBool mIsLineParticipant;

    // Child frame construction items.
    FrameConstructionItemList mChildItems;

  private:
    FrameConstructionItem(const FrameConstructionItem& aOther); /* not implemented */
  };

  /**
   * Function to create the table pseudo items we need.
   * @param aItems the child frame construction items before pseudo creation
   * @param aParentFrame the parent frame we're creating pseudos for
   */
  nsresult CreateNeededTablePseudos(FrameConstructionItemList& aItems,
                                    nsIFrame* aParentFrame);

  /**
   * Function to adjust aParentFrame to deal with captions.
   * @param aParentFrame the frame we think should be the parent.  This will be
   *        adjusted to point to the right parent frame.
   * @param aFCData the FrameConstructionData that would be used for frame
   *        construction.
   * @param aStyleContext the style context for aChildContent
   */
  // XXXbz this function should really go away once we rework pseudo-frame
  // handling to be better. This should simply be part of the job of
  // GetGeometricParent, and stuff like the frameitems and parent frame should
  // be kept track of in the state...
  void AdjustParentFrame(nsIFrame* &                  aParentFrame,
                         const FrameConstructionData* aFCData,
                         nsStyleContext*              aStyleContext);

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
  // ConstructButtonFrame puts the new frame in aFrameItems and
  // handles the kids of the button.
  nsresult ConstructButtonFrame(nsFrameConstructorState& aState,
                                FrameConstructionItem&    aItem,
                                nsIFrame*                aParentFrame,
                                const nsStyleDisplay*    aStyleDisplay,
                                nsFrameItems&            aFrameItems,
                                nsIFrame**               aNewFrame);

  // ConstructSelectFrame puts the new frame in aFrameItems and
  // handles the kids of the select.
  nsresult ConstructSelectFrame(nsFrameConstructorState& aState,
                                FrameConstructionItem&   aItem,
                                nsIFrame*                aParentFrame,
                                const nsStyleDisplay*    aStyleDisplay,
                                nsFrameItems&            aFrameItems,
                                nsIFrame**               aNewFrame);

  // ConstructFieldSetFrame puts the new frame in aFrameItems and
  // handles the kids of the fieldset
  nsresult ConstructFieldSetFrame(nsFrameConstructorState& aState,
                                  FrameConstructionItem&   aItem,
                                  nsIFrame*                aParentFrame,
                                  const nsStyleDisplay*    aStyleDisplay,
                                  nsFrameItems&            aFrameItems,
                                  nsIFrame**               aNewFrame);

  // aParentFrame might be null.  If it is, that means it was an
  // inline frame.
  static const FrameConstructionData* FindTextData(nsIFrame* aParentFrame);

  nsresult ConstructTextFrame(const FrameConstructionData* aData,
                              nsFrameConstructorState& aState,
                              nsIContent*              aContent,
                              nsIFrame*                aParentFrame,
                              nsStyleContext*          aStyleContext,
                              nsFrameItems&            aFrameItems);

  // If aParentContent's child at aContentIndex is a text node and
  // doesn't have a frame, append a frame construction item for it to aItems.
  void AddTextItemIfNeeded(nsFrameConstructorState& aState,
                           nsIFrame* aParentFrame,
                           nsIContent* aParentContent,
                           PRInt32 aContentIndex,
                           FrameConstructionItemList& aItems);

  // If aParentContent's child at aContentIndex is a text node and
  // doesn't have a frame, try to create a frame for it.
  void ReframeTextIfNeeded(nsIContent* aParentContent,
                           PRInt32 aContentIndex);

  void AddPageBreakItem(nsIContent* aContent,
                        nsStyleContext* aMainStyleContext,
                        FrameConstructionItemList& aItems);

  // Function to find FrameConstructionData for aContent.  Will return
  // null if aContent is not HTML.
  // aParentFrame might be null.  If it is, that means it was an
  // inline frame.
  static const FrameConstructionData* FindHTMLData(nsIContent* aContent,
                                                   nsIAtom* aTag,
                                                   PRInt32 aNameSpaceID,
                                                   nsIFrame* aParentFrame,
                                                   nsStyleContext* aStyleContext);
  // HTML data-finding helper functions
  static const FrameConstructionData*
    FindImgData(nsIContent* aContent, nsStyleContext* aStyleContext);
  static const FrameConstructionData*
    FindImgControlData(nsIContent* aContent, nsStyleContext* aStyleContext);
  static const FrameConstructionData*
    FindInputData(nsIContent* aContent, nsStyleContext* aStyleContext);
  static const FrameConstructionData*
    FindObjectData(nsIContent* aContent, nsStyleContext* aStyleContext);

  /* Construct a frame from the given FrameConstructionItem.  This function
     will handle adding the frame to frame lists, processing children, adding
     it to the primary frame map, and so forth.

     @param aItem the FrameConstructionItem to use.
     @param aState the frame construction state to use.
     @param aParentFrame the frame to set as the parent of the
                         newly-constructed frame.
     @param aFrameItems the frame list to add the new frame (or its
                        placeholder) to.
  */
  nsresult ConstructFrameFromItemInternal(FrameConstructionItem& aItem,
                                          nsFrameConstructorState& aState,
                                          nsIFrame* aParentFrame,
                                          nsFrameItems& aFrameItems);

  // possible flags for AddFrameConstructionItemInternal's aFlags argument
  /* Allow xbl:base to affect the tag/namespace used. */
#define ITEM_ALLOW_XBL_BASE 0x1
  /* Allow page-break before and after items to be created if the
     style asks for them. */
#define ITEM_ALLOW_PAGE_BREAK 0x2
  /* The item is a generated content item. */
#define ITEM_IS_GENERATED_CONTENT 0x4
  // The guts of AddFrameConstructionItems
  // aParentFrame might be null.  If it is, that means it was an
  // inline frame.
  void AddFrameConstructionItemsInternal(nsFrameConstructorState& aState,
                                         nsIContent*              aContent,
                                         nsIFrame*                aParentFrame,
                                         nsIAtom*                 aTag,
                                         PRInt32                  aNameSpaceID,
                                         PRInt32                  aContentIndex,
                                         nsStyleContext*          aStyleContext,
                                         PRUint32                 aFlags,
                                         FrameConstructionItemList& aItems);

  /**
   * Construct frames for the given item list and parent frame, and put the
   * resulting frames in aFrameItems.
   */
  nsresult ConstructFramesFromItemList(nsFrameConstructorState& aState,
                                       FrameConstructionItemList& aItems,
                                       nsIFrame* aParentFrame,
                                       nsFrameItems& aFrameItems);
  nsresult ConstructFramesFromItem(nsFrameConstructorState& aState,
                                   FCItemIterator& aItem,
                                   nsIFrame* aParentFrame,
                                   nsFrameItems& aFrameItems);
  static PRBool AtLineBoundary(FCItemIterator& aIter);

  nsresult CreateAnonymousFrames(nsFrameConstructorState& aState,
                                 nsIContent*              aParent,
                                 nsIFrame*                aParentFrame,
                                 nsFrameItems&            aChildItems);

  nsresult GetAnonymousContent(nsIContent* aParent,
                               nsIFrame* aParentFrame,
                               nsTArray<nsIContent*>& aAnonContent);

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

  // Function to find FrameConstructionData for aContent.  Will return
  // null if aContent is not MathML.
  static const FrameConstructionData* FindMathMLData(nsIContent* aContent,
                                                     nsIAtom* aTag,
                                                     PRInt32 aNameSpaceID,
                                                     nsStyleContext* aStyleContext);
#endif

  // Function to find FrameConstructionData for aContent.  Will return
  // null if aContent is not XUL.
  static const FrameConstructionData* FindXULTagData(nsIContent* aContent,
                                                     nsIAtom* aTag,
                                                     PRInt32 aNameSpaceID,
                                                     nsStyleContext* aStyleContext);
  // XUL data-finding helper functions and structures
#ifdef MOZ_XUL
  static const FrameConstructionData*
    FindPopupGroupData(nsIContent* aContent, nsStyleContext* aStyleContext);
  // sXULTextBoxData used for both labels and descriptions
  static const FrameConstructionData sXULTextBoxData;
  static const FrameConstructionData*
    FindXULLabelData(nsIContent* aContent, nsStyleContext* aStyleContext);
  static const FrameConstructionData*
    FindXULDescriptionData(nsIContent* aContent, nsStyleContext* aStyleContext);
#ifdef XP_MACOSX
  static const FrameConstructionData*
    FindXULMenubarData(nsIContent* aContent, nsStyleContext* aStyleContext);
#endif /* XP_MACOSX */
  static const FrameConstructionData*
    FindXULListBoxBodyData(nsIContent* aContent, nsStyleContext* aStyleContext);
  static const FrameConstructionData*
    FindXULListItemData(nsIContent* aContent, nsStyleContext* aStyleContext);
#endif /* MOZ_XUL */

  // Function to find FrameConstructionData for aContent using one of the XUL
  // display types.  Will return null if aDisplay doesn't have a XUL display
  // type.  This function performs no other checks, so should only be called if
  // we know for sure that the content is not something that should get a frame
  // constructed by tag.
  static const FrameConstructionData*
    FindXULDisplayData(const nsStyleDisplay* aDisplay,
                       nsIContent* aContent,
                       nsStyleContext* aStyleContext);

// SVG - rods
#ifdef MOZ_SVG
  static const FrameConstructionData* FindSVGData(nsIContent* aContent,
                                                  nsIAtom* aTag,
                                                  PRInt32 aNameSpaceID,
                                                  nsIFrame* aParentFrame,
                                                  nsStyleContext* aStyleContext);

  nsresult ConstructSVGForeignObjectFrame(nsFrameConstructorState& aState,
                                          FrameConstructionItem&   aItem,
                                          nsIFrame* aParentFrame,
                                          const nsStyleDisplay* aStyleDisplay,
                                          nsFrameItems& aFrameItems,
                                          nsIFrame** aNewFrame);
#endif

  /* Not static because it does PropagateScrollToViewport.  If this
     changes, make this static */
  const FrameConstructionData*
    FindDisplayData(const nsStyleDisplay* aDisplay, nsIContent* aContent,
                    nsStyleContext* aStyleContext);

  /**
   * Construct a scrollable block frame
   */
  nsresult ConstructScrollableBlock(nsFrameConstructorState& aState,
                                    FrameConstructionItem&   aItem,
                                    nsIFrame*                aParentFrame,
                                    const nsStyleDisplay*    aDisplay,
                                    nsFrameItems&            aFrameItems,
                                    nsIFrame**               aNewFrame);

  /**
   * Construct a non-scrollable block frame
   */
  nsresult ConstructNonScrollableBlock(nsFrameConstructorState& aState,
                                       FrameConstructionItem&   aItem,
                                       nsIFrame*                aParentFrame,
                                       const nsStyleDisplay*    aDisplay,
                                       nsFrameItems&            aFrameItems,
                                       nsIFrame**               aNewFrame);

  /**
   * Construct the frames for the children of aContent.  "children" is defined
   * as "whatever ChildIterator returns for aContent".  This means we're
   * basically operating on children in the "flattened tree" per sXBL/XBL2.
   * This method will also handle constructing ::before, ::after,
   * ::first-letter, and ::first-line frames, as needed and if allowed.
   *
   * If the parent is a float containing block, this method will handle pushing
   * it as the float containing block in aState (so there's no need for callers
   * to push it themselves).
   *
   * @param aState the frame construction state
   * @param aContent the content node whose children need frames
   * @param aStyleContext the style context for aContent
   * @param aFrame the frame to use as the parent frame for the new in-flow
   *        kids. Note that this must be its own content insertion frame, but
   *        need not be be the primary frame for aContent.  This frame will be
   *        pushed as the float containing block, as needed.  aFrame is also
   *        used to find the parent style context for the kids' style contexts
   *        (not necessary aFrame's style context).
   * @param aCanHaveGeneratedContent Whether to allow :before and
   *        :after styles on the parent.
   * @param aFrameItems the list in which we should place the in-flow children
   * @param aAllowBlockStyles Whether to allow first-letter and first-line
   *        styles on the parent.
   * @param aTableCreator if non-null, will just make this method call
   *        TableProcessChildren between constructing the ::before and ::after
   *        content instead of doing whatever it would normally do.
   */
  nsresult ProcessChildren(nsFrameConstructorState& aState,
                           nsIContent*              aContent,
                           nsStyleContext*          aStyleContext,
                           nsIFrame*                aFrame,
                           const PRBool             aCanHaveGeneratedContent,
                           nsFrameItems&            aFrameItems,
                           const PRBool             aAllowBlockStyles);

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
  // @param aParentFrame the geometric parent that the scrollframe will have.
  nsresult
  BuildScrollFrame(nsFrameConstructorState& aState,
                   nsIContent*              aContent,
                   nsStyleContext*          aContentStyle,
                   nsIFrame*                aScrolledFrame,
                   nsIFrame*                aParentFrame,
                   nsIFrame*&               aNewFrame);

  // Builds the initial ScrollFrame
  already_AddRefed<nsStyleContext>
  BeginBuildingScrollFrame(nsFrameConstructorState& aState,
                           nsIContent*              aContent,
                           nsStyleContext*          aContentStyle,
                           nsIFrame*                aParentFrame,
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

  nsresult MaybeRecreateFramesForContent(nsIContent* aContent);

  // If aAsyncInsert is true then a restyle event will be posted to handle the
  // required ContentInserted call instead of doing it immediately.
  nsresult RecreateFramesForContent(nsIContent* aContent, PRBool aAsyncInsert);

  // If removal of aFrame from the frame tree requires reconstruction of some
  // containing block (either of aFrame or of its parent) due to {ib} splits or
  // table pseudo-frames, recreate the relevant frame subtree.  The return value
  // indicates whether this happened.  If this method returns true, *aResult is
  // the return value of ReframeContainingBlock or RecreateFramesForContent.
  // If this method returns false, the value of *aResult is not affected.
  // aFrame and aResult must not be null.  aFrame must be the result of a
  // GetPrimaryFrameFor() call (which means its parent is also not null).
  PRBool MaybeRecreateContainerForFrameRemoval(nsIFrame* aFrame,
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
                           FrameConstructionItem&   aItem,
                           nsIFrame*                aParentFrame,
                           const nsStyleDisplay*    aDisplay,
                           nsFrameItems&            aFrameItems,
                           nsIFrame**               aNewFrame);

  /**
   * Move an already-constructed framelist into the inline frame at
   * the tail end of an {ib} split.
   *
   * @param aState the frame construction state we're using right now.
   * @param aExistingEndFrame the already-existing end frame.
   * @param aFramesToMove The frame list to move over.  Must be nonempty.
   * @param aBlockPart the block part of the {ib} split.
   * @param aTargetState if non-null, the target state to pass to
   *        MoveChildrenTo for float reparenting.
   * XXXbz test float reparenting?
   */
  void MoveFramesToEndOfIBSplit(nsFrameConstructorState& aState,
                                nsIFrame* aExistingEndFrame,
                                nsFrameList& aFramesToMove,
                                nsIFrame* aBlockPart,
                                nsFrameConstructorState* aTargetState);

  /**
   * For an inline aParentItem, construct its list of child
   * FrameConstructionItems and set its mIsAllInline flag appropriately.
   */
  void BuildInlineChildItems(nsFrameConstructorState& aState,
                             FrameConstructionItem& aParentItem);

  // Determine whether we need to wipe out what we just did and start over
  // because we're doing something like adding block kids to an inline frame
  // (and therefore need an {ib} split).  aPrevSibling must be correct, even in
  // aIsAppend cases.  Passing aIsAppend false even when an append is happening
  // is ok in terms of correctness, but can lead to unnecessary reframing.  If
  // aIsAppend is true, then the caller MUST call
  // nsCSSFrameConstructor::AppendFrames (as opposed to
  // nsFrameManager::InsertFrames directly) to add the new frames.
  // @return PR_TRUE if we reconstructed the containing block, PR_FALSE
  // otherwise
  PRBool WipeContainingBlock(nsFrameConstructorState& aState,
                             nsIFrame*                aContainingBlock,
                             nsIFrame*                aFrame,
                             FrameConstructionItemList& aItems,
                             PRBool                   aIsAppend,
                             nsIFrame*                aPrevSibling);

  nsresult ReframeContainingBlock(nsIFrame* aFrame);

  nsresult StyleChangeReflow(nsIFrame* aFrame, nsChangeHint aHint);

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

  nsresult CreateLetterFrame(nsIFrame*                aBlockFrame,
                             nsIContent*              aTextContent,
                             nsIFrame*                aParentFrame,
                             nsFrameItems&            aResult);

  nsresult WrapFramesInFirstLetterFrame(nsIContent*   aBlockContent,
                                        nsIFrame*     aBlockFrame,
                                        nsFrameItems& aBlockFrames);

  nsresult WrapFramesInFirstLetterFrame(nsIFrame*     aBlockFrame,
                                        nsIFrame*     aParentFrame,
                                        nsIFrame*     aParentFrameList,
                                        nsIFrame**    aModifiedParent,
                                        nsIFrame**    aTextFrame,
                                        nsIFrame**    aPrevFrame,
                                        nsFrameItems& aLetterFrame,
                                        PRBool*       aStopLooking);

  nsresult RecoverLetterFrames(nsIFrame* aBlockFrame);

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

  // This method chops the initial inline-outside frames out of aFrameItems.
  // If aLineFrame is non-null, it appends them to that frame.  Otherwise, it
  // creates a new line frame, sets the inline frames as its initial child
  // list, and inserts that line frame at the front of what's left of
  // aFrameItems.  In both cases, the kids are reparented to the line frame.
  // After this call, aFrameItems holds the frames that need to become kids of
  // the block (possibly including line frames).
  nsresult WrapFramesInFirstLineFrame(nsFrameConstructorState& aState,
                                      nsIContent*              aBlockContent,
                                      nsIFrame*                aBlockFrame,
                                      nsIFrame*                aLineFrame,
                                      nsFrameItems&            aFrameItems);

  // Handle the case when a block with first-line style is appended to (by
  // possibly calling WrapFramesInFirstLineFrame as needed).
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

  // Find the ``rightmost'' frame for the content immediately preceding the one
  // aIter points to, following continuations if necessary.  aIter is passed by
  // value on purpose, so as not to modify the callee's iterator.
  nsIFrame* FindPreviousSibling(const ChildIterator& aFirst,
                                ChildIterator aIter);

  // Find the frame for the content node immediately following the one aIter
  // points to, following continuations if necessary.  aIter is passed by value
  // on purpose, so as not to modify the callee's iterator.
  nsIFrame* FindNextSibling(ChildIterator aIter,
                            const ChildIterator& aLast);

  // Find the right previous sibling for an insertion.  This also updates the
  // parent frame to point to the correct continuation of the parent frame to
  // use, and returns whether this insertion is to be treated as an append.
  // aChild is the child being inserted and aIndexInContainer its index in
  // aContainer (which is aChild's DOM parent).
  nsIFrame* GetInsertionPrevSibling(nsIFrame*& aParentFrame, /* inout */
                                    nsIContent* aContainer,
                                    nsIContent* aChild,
                                    PRInt32 aIndexInContainer,
                                    PRBool* aIsAppend);

  // see if aContent and aSibling are legitimate siblings due to restrictions
  // imposed by table columns
  // XXXbz this code is generally wrong, since the frame for aContent
  // may be constructed based on tag, not based on aDisplay!
  PRBool IsValidSibling(nsIFrame*              aSibling,
                        nsIContent*            aContent,
                        PRUint8&               aDisplay);
  
  void QuotesDirty() {
    NS_PRECONDITION(mUpdateCount != 0, "Instant quote updates are bad news");
    mQuotesDirty = PR_TRUE;
  }

  void CountersDirty() {
    NS_PRECONDITION(mUpdateCount != 0, "Instant counter updates are bad news");
    mCountersDirty = PR_TRUE;
  }

public:

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
  
  // This is just the outermost frame for the root element.
  nsIFrame*           mRootElementFrame;
  // This is the frame for the root element that has no pseudo-element style.
  nsIFrame*           mRootElementStyleFrame;
  // This is the containing block for fixed-pos frames --- the
  // viewport or page frame
  nsIFrame*           mFixedContainingBlock;
  // This is the containing block that contains the root element ---
  // the real "initial containing block" according to CSS 2.1.
  nsIFrame*           mDocElementContainingBlock;
  nsIFrame*           mGfxScrollFrame;
  nsIFrame*           mPageSequenceFrame;
  nsQuoteList         mQuoteList;
  nsCounterManager    mCounterManager;
  PRUint16            mUpdateCount;
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
  nsDataHashtable<nsISupportsHashKey, RestyleData> mPendingAnimationRestyles;

  static nsIXBLService * gXBLService;
};

#endif /* nsCSSFrameConstructor_h___ */
