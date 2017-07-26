/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * construction of a frame tree that is nearly isomorphic to the content
 * tree and updating of that tree in response to dynamic changes
 */

#ifndef nsCSSFrameConstructor_h___
#define nsCSSFrameConstructor_h___

#include "mozilla/Attributes.h"
#include "mozilla/LinkedList.h"
#include "mozilla/RestyleManager.h"
#include "mozilla/RestyleManager.h"

#include "nsCOMPtr.h"
#include "nsILayoutHistoryState.h"
#include "nsQuoteList.h"
#include "nsCounterManager.h"
#include "nsIAnonymousContentCreator.h"
#include "nsFrameManager.h"
#include "ScrollbarStyles.h"

struct nsFrameItems;
class nsStyleContext;
struct nsStyleDisplay;
struct nsGenConInitializer;

class nsContainerFrame;
class nsFirstLineFrame;
class nsFirstLetterFrame;
class nsICSSAnonBoxPseudo;
class nsIDocument;
class nsPageContentFrame;
struct PendingBinding;
class nsGenericDOMDataNode;

class nsFrameConstructorState;

namespace mozilla {

namespace dom {

class FlattenedChildIterator;

} // namespace dom
} // namespace mozilla

class nsCSSFrameConstructor final : public nsFrameManager
{
public:
  typedef mozilla::CSSPseudoElementType CSSPseudoElementType;
  typedef mozilla::dom::Element Element;

  friend class mozilla::RestyleManager;
  friend class mozilla::GeckoRestyleManager;
  friend class mozilla::ServoRestyleManager;

  nsCSSFrameConstructor(nsIDocument* aDocument, nsIPresShell* aPresShell);
  ~nsCSSFrameConstructor() {
    MOZ_ASSERT(mUpdateCount == 0, "Dying in the middle of our own update?");
  }

  // get the alternate text for a content node
  static void GetAlternateTextFor(nsIContent*    aContent,
                                  nsIAtom*       aTag,  // content object's tag
                                  nsXPIDLString& aAltText);

private:
  nsCSSFrameConstructor(const nsCSSFrameConstructor& aCopy) = delete;
  nsCSSFrameConstructor& operator=(const nsCSSFrameConstructor& aCopy) = delete;

public:
  mozilla::RestyleManager* RestyleManager() const
    { return mPresShell->GetPresContext()->RestyleManager(); }

  nsIFrame* ConstructRootFrame();

  void ReconstructDocElementHierarchy();

  // Create frames for content nodes that are marked as needing frames. This
  // should be called before ProcessPendingRestyles.
  // Note: It's the caller's responsibility to make sure to wrap a
  // CreateNeededFrames call in a view update batch and a script blocker.
  void CreateNeededFrames();

private:
  void CreateNeededFrames(nsIContent* aContent,
                          TreeMatchContext& aTreeMatchContext);

  enum Operation {
    CONTENTAPPEND,
    CONTENTINSERT
  };

  // aChild is the child being inserted for inserts, and the first
  // child being appended for appends.
  bool MaybeConstructLazily(Operation aOperation,
                              nsIContent* aContainer,
                              nsIContent* aChild);

  // Issues a single ContentInserted for each child of aContainer in the range
  // [aStartChild, aEndChild).
  void IssueSingleInsertNofications(nsIContent* aContainer,
                                    nsIContent* aStartChild,
                                    nsIContent* aEndChild,
                                    bool aAllowLazyConstruction,
                                    bool aForReconstruction);

  /**
   * Data that represents an insertion point for some child content.
   */
  struct InsertionPoint
  {
    InsertionPoint()
      : mParentFrame(nullptr), mContainer(nullptr), mMultiple(false) {}
    InsertionPoint(nsContainerFrame* aParentFrame, nsIContent* aContainer,
                   bool aMultiple = false)
      : mParentFrame(aParentFrame), mContainer(aContainer),
        mMultiple(aMultiple) {}
    /**
     * The parent frame to use if the inserted children needs to create
     * frame(s).  May be null, which signals that  we shouldn't try to
     * create frames for the inserted children; either because there are
     * no parent frame or because there are multiple insertion points and
     * we will call IssueSingleInsertNofications for each child instead.
     * mContainer should not be used when mParentFrame is null.
     */
    nsContainerFrame* mParentFrame;
    /**
     * The flattened tree parent for the inserted children.
     * It's undefined if mParentFrame is null.
     */
    nsIContent* mContainer;
    /**
     * If true then there are multiple insertion points, which means consumers
     * should insert children individually into the node's flattened tree parent.
     */
    bool mMultiple;
  };
  /**
   * Checks if the children of aContainer in the range [aStartChild, aEndChild)
   * can be inserted/appended to one insertion point together. If so, returns
   * that insertion point. If not, returns with InsertionPoint.mFrame == nullptr
   * and issues single ContentInserted calls for each child.
   * aEndChild = nullptr indicates that we are dealing with an append.
   */
  InsertionPoint GetRangeInsertionPoint(nsIContent* aContainer,
                                        nsIContent* aStartChild,
                                        nsIContent* aEndChild,
                                        bool aAllowLazyConstruction,
                                        bool aForReconstruction);

  // Returns true if parent was recreated due to frameset child, false otherwise.
  bool MaybeRecreateForFrameset(nsIFrame* aParentFrame,
                                  nsIContent* aStartChild,
                                  nsIContent* aEndChild);

  /**
   * For each child in the aStartChild/aEndChild range, calls
   * NoteDirtyDescendantsForServo on their flattened tree parents.  This is
   * used when content is inserted into the document and we decide that
   * we can do lazy frame construction.  It handles children being rebound to
   * different insertion points by calling NoteDirtyDescendantsForServo on each
   * child's flattened tree parent.  Only used when we are styled by Servo.
   */
  void LazilyStyleNewChildRange(nsIContent* aStartChild, nsIContent* aEndChild);

  /**
   * For each child in the aStartChild/aEndChild range, calls StyleNewChildren
   * on their flattened tree parents.  This is used when content is inserted
   * into the document and we decide that we cannot do lazy frame construction.
   * It handles children being rebound to different insertion points by calling
   * StyleNewChildren on each child's flattened tree parent.  Only used when we
   * are styled by Servo.
   */
  void StyleNewChildRange(nsIContent* aStartChild, nsIContent* aEndChild);

  /**
   * Calls StyleSubtreeForReconstruct on each child in the aStartChild/aEndChild
   * range. Only used when we are styled by Servo.
   */
  void StyleChildRangeForReconstruct(nsIContent* aStartChild,
                                     nsIContent* aEndChild);

public:
  /**
   * Lazy frame construction is controlled by the aAllowLazyConstruction bool
   * parameter of nsCSSFrameConstructor::ContentAppended/Inserted. It is true
   * for all inserts/appends as passed from the presshell, except for the
   * insert of the root element, which is always non-lazy. Even if the
   * aAllowLazyConstruction passed to ContentAppended/Inserted is true we still
   * may not be able to construct lazily, so we call MaybeConstructLazily.
   * MaybeConstructLazily does not allow lazy construction if any of the
   * following are true:
   *  -we are in chrome
   *  -the container is in a native anonymous subtree
   *  -the container is XUL
   *  -is any of the appended/inserted nodes are XUL or editable
   *  -(for inserts) the child is anonymous.  In the append case this function
   *   must not be called with anonymous children.
   * The XUL and chrome checks are because XBL bindings only get applied at
   * frame construction time and some things depend on the bindings getting
   * attached synchronously. The editable checks are because the editor seems
   * to expect frames to be constructed synchronously.
   *
   * If MaybeConstructLazily returns false we construct as usual, but if it
   * returns true then it adds NODE_NEEDS_FRAME bits to the newly
   * inserted/appended nodes and adds NODE_DESCENDANTS_NEED_FRAMES bits to the
   * container and up along the parent chain until it hits the root or another
   * node with that bit set. Then it posts a restyle event to ensure that a
   * flush happens to construct those frames.
   *
   * When the flush happens the presshell calls
   * nsCSSFrameConstructor::CreateNeededFrames. CreateNeededFrames follows any
   * nodes with NODE_DESCENDANTS_NEED_FRAMES set down the content tree looking
   * for nodes with NODE_NEEDS_FRAME set. It calls ContentAppended for any runs
   * of nodes with NODE_NEEDS_FRAME set that are at the end of their childlist,
   * and ContentRangeInserted for any other runs that aren't.
   *
   * If a node is removed from the document then we don't bother unsetting any
   * of the lazy bits that might be set on it, its descendants, or any of its
   * ancestor nodes because that is a slow operation, the work might be wasted
   * if another node gets inserted in its place, and we can clear the bits
   * quicker by processing the content tree from top down the next time we call
   * CreateNeededFrames. (We do clear the bits when BindToTree is called on any
   * nsIContent; so any nodes added to the document will not have any lazy bits
   * set.)
   */

  // If aAllowLazyConstruction is true then frame construction of the new
  // children can be done lazily.
  //
  // When constructing frames lazily, we can keep the tree match context in a
  // much easier way than nsFrameConstructorState, and thus, we're allowed to
  // provide a TreeMatchContext to avoid calling InitAncestors repeatedly deep
  // in the DOM.
  void ContentAppended(nsIContent* aContainer,
                       nsIContent* aFirstNewContent,
                       bool aAllowLazyConstruction,
                       TreeMatchContext* aProvidedTreeMatchContext = nullptr)
  {
    ContentAppended(aContainer, aFirstNewContent, aAllowLazyConstruction, false,
                    aProvidedTreeMatchContext);
  }

  // If aAllowLazyConstruction is true then frame construction of the new child
  // can be done lazily.
  void ContentInserted(nsIContent* aContainer,
                       nsIContent* aChild,
                       nsILayoutHistoryState* aFrameState,
                       bool aAllowLazyConstruction);

  // Like ContentInserted but handles inserting the children of aContainer in
  // the range [aStartChild, aEndChild).  aStartChild must be non-null.
  // aEndChild may be null to indicate the range includes all kids after
  // aStartChild.
  //
  // If aAllowLazyConstruction is true then frame construction of
  // the new children can be done lazily. It is only allowed to be true when
  // inserting a single node.
  //
  // See ContentAppended to see why we allow passing an already initialized
  // TreeMatchContext.
  void ContentRangeInserted(nsIContent* aContainer,
                            nsIContent* aStartChild,
                            nsIContent* aEndChild,
                            nsILayoutHistoryState* aFrameState,
                            bool aAllowLazyConstruction,
                            TreeMatchContext* aProvidedTreeMatchContext = nullptr)
  {
    ContentRangeInserted(aContainer, aStartChild, aEndChild, aFrameState,
                         aAllowLazyConstruction, false,
                         aProvidedTreeMatchContext);
  }

private:
  // Helpers for the public ContentAppended, ContentInserted and
  // ContentRangeInserted functions above.
  //
  // aForReconstruction indicates whether this call is for frame reconstruction
  // via RecreateFramesFor or lazy frame construction via CreateNeededFrames.
  // (This latter case admittedly isn't always for "reconstruction" per se, but
  // the important thing is that aForReconstruction is false for real content
  // insertions, and true for other cases.)
  void ContentAppended(nsIContent* aContainer,
                       nsIContent* aFirstNewContent,
                       bool aAllowLazyConstruction,
                       bool aForReconstruction,
                       TreeMatchContext* aProvidedTreeMatchContext);
  void ContentRangeInserted(nsIContent* aContainer,
                            nsIContent* aStartChild,
                            nsIContent* aEndChild,
                            nsILayoutHistoryState* aFrameState,
                            bool aAllowLazyConstruction,
                            bool aForReconstruction,
                            TreeMatchContext* aProvidedTreeMatchContext);

public:
  enum RemoveFlags {
    REMOVE_CONTENT, REMOVE_FOR_RECONSTRUCTION, REMOVE_DESTROY_FRAMES };
  /**
   * Recreate or destroy frames for aChild in aContainer.
   * aFlags == REMOVE_CONTENT means aChild has been removed from the document.
   * aFlags == REMOVE_FOR_RECONSTRUCTION means the caller will reconstruct the
   *   frames later.
   * In both the above cases, this method will in some cases try to reconstruct
   * the frames (aDidReconstruct is then set to true), it's just that in the
   * former case aChild isn't in the document so no frames will be created for
   * it.  Ancestors may have been reframed though.
   * aFlags == REMOVE_DESTROY_FRAMES is the same as REMOVE_FOR_RECONSTRUCTION
   * except it will never try to reconstruct frames.  Instead, the caller is
   * responsible for doing that, on the content returned in aDestroyedFramesFor.
   * The layout frame state is guarranted to be captured for the removed frames
   * only when aFlags == REMOVE_DESTROY_FRAMES, otherwise it will only be
   * captured if we reconstructed frames for an ancestor.
   */
  void ContentRemoved(nsIContent*  aContainer,
                      nsIContent*  aChild,
                      nsIContent*  aOldNextSibling,
                      RemoveFlags  aFlags,
                      bool*        aDidReconstruct,
                      nsIContent** aDestroyedFramesFor = nullptr);

  void CharacterDataChanged(nsIContent* aContent,
                            CharacterDataChangeInfo* aInfo);

  // If aContent is a text node that has been optimized away due to being
  // whitespace next to a block boundary (or for some other reason), stop
  // doing that and create a frame for it if it should have one. This recreates
  // frames so be careful (although this should not change actual layout).
  // Returns the frame for aContent if there is one.
  nsIFrame* EnsureFrameForTextNode(nsGenericDOMDataNode* aContent);

  // Generate the child frames and process bindings
  void GenerateChildFrames(nsContainerFrame* aFrame);

  // Should be called when a frame is going to be destroyed and
  // WillDestroyFrameTree hasn't been called yet.
  void NotifyDestroyingFrame(nsIFrame* aFrame);

  void BeginUpdate();
  void EndUpdate();
  void RecalcQuotesAndCounters();

  // Called when any counter style is changed.
  void NotifyCounterStylesAreDirty();

  // Gets called when the presshell is destroying itself and also
  // when we tear down our frame tree to reconstruct it
  void WillDestroyFrameTree();

  /**
   * Destroy the frames for aContent.  Note that this may destroy frames
   * for an ancestor instead - aDestroyedFramesFor contains the content node
   * where frames were actually destroyed (which should be used in the
   * ContentInserted call to recreate frames).  The frame tree state
   * is captured before the frames are destroyed and can be retrieved using
   * GetLastCapturedLayoutHistoryState().
   */
  void DestroyFramesFor(nsIContent*  aContent,
                        nsIContent** aDestroyedFramesFor);

  // Request to create a continuing frame.  This method never returns null.
  nsIFrame* CreateContinuingFrame(nsPresContext*    aPresContext,
                                  nsIFrame*         aFrame,
                                  nsContainerFrame* aParentFrame,
                                  bool              aIsFluid = true);

  // Copy over fixed frames from aParentFrame's prev-in-flow
  nsresult ReplicateFixedFrames(nsPageContentFrame* aParentFrame);

  /**
   * Get the XBL insertion point for aChild in aContainer.
   */
  InsertionPoint GetInsertionPoint(nsIContent* aContainer, nsIContent* aChild);

  void CreateListBoxContent(nsContainerFrame* aParentFrame,
                            nsIFrame*         aPrevFrame,
                            nsIContent*       aChild,
                            nsIFrame**        aResult,
                            bool              aIsAppend);

  // GetInitialContainingBlock() is deprecated in favor of GetRootElementFrame();
  // nsIFrame* GetInitialContainingBlock() { return mRootElementFrame; }
  // This returns the outermost frame for the root element
  nsContainerFrame* GetRootElementFrame() { return mRootElementFrame; }
  // This returns the frame for the root element that does not
  // have a psuedo-element style
  nsIFrame* GetRootElementStyleFrame() { return mRootElementStyleFrame; }
  nsIFrame* GetPageSequenceFrame() { return mPageSequenceFrame; }

  // Get the frame that is the parent of the root element.
  nsContainerFrame* GetDocElementContainingBlock()
    { return mDocElementContainingBlock; }

  /**
   * Return the layout history state that was captured in the last
   * ContentRemoved / RecreateFramesForContent call.
   */
  nsILayoutHistoryState* GetLastCapturedLayoutHistoryState()
  {
    return mTempFrameTreeState;
  }

private:
  struct FrameConstructionItem;
  class FrameConstructionItemList;

  nsContainerFrame* ConstructPageFrame(nsIPresShell*      aPresShell,
                                       nsContainerFrame*  aParentFrame,
                                       nsIFrame*          aPrevPageFrame,
                                       nsContainerFrame*& aCanvasFrame);

  void InitAndRestoreFrame (const nsFrameConstructorState& aState,
                            nsIContent*                    aContent,
                            nsContainerFrame*              aParentFrame,
                            nsIFrame*                      aNewFrame,
                            bool                           aAllowCounters = true);

  // aState can be null if not available; it's used as an optimization.
  // XXXbz IsValidSibling is the only caller that doesn't pass a state here!
  already_AddRefed<nsStyleContext>
  ResolveStyleContext(nsIFrame*                aParentFrame,
                      nsIContent*              aContainer,
                      nsIContent*              aChild,
                      nsFrameConstructorState* aState);
  already_AddRefed<nsStyleContext>
  ResolveStyleContext(nsIFrame*                aParentFrame,
                      nsIContent*              aChild,
                      nsFrameConstructorState* aState);
  already_AddRefed<nsStyleContext>
  ResolveStyleContext(const InsertionPoint&    aInsertion,
                      nsIContent*              aChild,
                      nsFrameConstructorState* aState);
  already_AddRefed<nsStyleContext>
  ResolveStyleContext(nsStyleContext*          aParentStyleContext,
                      nsIContent*              aContent,
                      nsFrameConstructorState* aState,
                      Element*                 aOriginatingElementOrNull = nullptr);

  // Add the frame construction items for the given aContent and aParentFrame
  // to the list.  This might add more than one item in some rare cases.
  // If aSuppressWhiteSpaceOptimizations is true, optimizations that
  // may suppress the construction of white-space-only text frames
  // must be skipped for these items and items around them.
  void AddFrameConstructionItems(nsFrameConstructorState& aState,
                                 nsIContent*              aContent,
                                 bool                     aSuppressWhiteSpaceOptimizations,
                                 const InsertionPoint&    aInsertion,
                                 FrameConstructionItemList& aItems);

  // Helper method for AddFrameConstructionItems etc.
  // Unsets the need-frame/restyle bits on aContent.
  // return true iff we should attempt to create frames for aContent.
  bool ShouldCreateItemsForChild(nsFrameConstructorState& aState,
                                 nsIContent* aContent,
                                 nsContainerFrame* aParentFrame);

  // Helper method for AddFrameConstructionItems etc.
  // Make sure ShouldCreateItemsForChild() returned true before calling this.
  void DoAddFrameConstructionItems(nsFrameConstructorState& aState,
                                   nsIContent* aContent,
                                   nsStyleContext* aStyleContext,
                                   bool aSuppressWhiteSpaceOptimizations,
                                   nsContainerFrame* aParentFrame,
                                   nsTArray<nsIAnonymousContentCreator::ContentInfo>* aAnonChildren,
                                   FrameConstructionItemList& aItems);

  // Construct the frames for the document element.  This can return null if the
  // document element is display:none, or if the document element has a
  // not-yet-loaded XBL binding, or if it's an SVG element that's not <svg>.
  nsIFrame* ConstructDocElementFrame(Element*                 aDocElement,
                                     nsILayoutHistoryState*   aFrameState);

  // Set up our mDocElementContainingBlock correctly for the given root
  // content.
  void SetUpDocElementContainingBlock(nsIContent* aDocElement);

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
  void CreateAttributeContent(nsIContent* aParentContent,
                              nsIFrame* aParentFrame,
                              int32_t aAttrNamespace,
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
                                                    RefPtr<nsTextNode>* aText,
                                                    nsGenConInitializer* aInitializer);

  /**
   * Create a content node for the given generated content style.
   * The caller takes care of making it SetIsNativeAnonymousRoot, binding it
   * to the document, and creating frames for it.
   * @param aParentContent is the node that has the before/after style
   * @param aStyleContext is the 'before' or 'after' pseudo-element
   * style context
   * @param aContentIndex is the index of the content item to create
   */
  already_AddRefed<nsIContent> CreateGeneratedContent(nsFrameConstructorState& aState,
                                                      nsIContent*     aParentContent,
                                                      nsStyleContext* aStyleContext,
                                                      uint32_t        aContentIndex);

  // aFrame may be null; this method doesn't use it directly in any case.
  void CreateGeneratedContentItem(nsFrameConstructorState&   aState,
                                  nsContainerFrame*          aFrame,
                                  nsIContent*                aContent,
                                  nsStyleContext*            aStyleContext,
                                  CSSPseudoElementType       aPseudoElement,
                                  FrameConstructionItemList& aItems);

  // This method can change aFrameList: it can chop off the beginning and put
  // it in aParentFrame while putting the remainder into a ib-split sibling of
  // aParentFrame.  aPrevSibling must be the frame after which aFrameList is to
  // be placed on aParentFrame's principal child list.  It may be null if
  // aFrameList is being added at the beginning of the child list.
  void AppendFramesToParent(nsFrameConstructorState&       aState,
                            nsContainerFrame*              aParentFrame,
                            nsFrameItems&                  aFrameList,
                            nsIFrame*                      aPrevSibling,
                            bool                           aIsRecursiveCall = false);

  // BEGIN TABLE SECTION
  /**
   * Construct a table wrapper frame. This is the FrameConstructionData
   * callback used for the job.
   */
  nsIFrame* ConstructTable(nsFrameConstructorState& aState,
                           FrameConstructionItem&   aItem,
                           nsContainerFrame*        aParentFrame,
                           const nsStyleDisplay*    aDisplay,
                           nsFrameItems&            aFrameItems);

  /**
   * FrameConstructionData callback for constructing table rows and row groups.
   */
  nsIFrame* ConstructTableRowOrRowGroup(nsFrameConstructorState& aState,
                                        FrameConstructionItem&   aItem,
                                        nsContainerFrame*        aParentFrame,
                                        const nsStyleDisplay*    aStyleDisplay,
                                        nsFrameItems&            aFrameItems);

  /**
   * FrameConstructionData callback used for constructing table columns.
   */
  nsIFrame* ConstructTableCol(nsFrameConstructorState& aState,
                              FrameConstructionItem&   aItem,
                              nsContainerFrame*        aParentFrame,
                              const nsStyleDisplay*    aStyleDisplay,
                              nsFrameItems&            aFrameItems);

  /**
   * FrameConstructionData callback used for constructing table cells.
   */
  nsIFrame* ConstructTableCell(nsFrameConstructorState& aState,
                               FrameConstructionItem&   aItem,
                               nsContainerFrame*        aParentFrame,
                               const nsStyleDisplay*    aStyleDisplay,
                               nsFrameItems&            aFrameItems);

private:
  /* An enum of possible parent types for anonymous table or ruby object
     construction */
  enum ParentType {
    eTypeBlock = 0, /* This includes all non-table-related frames */
    eTypeRow,
    eTypeRowGroup,
    eTypeColGroup,
    eTypeTable,
    eTypeRuby,
    eTypeRubyBase,
    eTypeRubyBaseContainer,
    eTypeRubyText,
    eTypeRubyTextContainer,
    eParentTypeCount
  };

  /* 4 bits is enough to handle our ParentType values */
#define FCDATA_PARENT_TYPE_OFFSET 28
  /* Macro to get the desired parent type out of an mBits member of
     FrameConstructionData */
#define FCDATA_DESIRED_PARENT_TYPE(_bits)           \
  ParentType((_bits) >> FCDATA_PARENT_TYPE_OFFSET)
  /* Macro to create FrameConstructionData bits out of a desired parent type */
#define FCDATA_DESIRED_PARENT_TYPE_TO_BITS(_type)     \
  (((uint32_t)(_type)) << FCDATA_PARENT_TYPE_OFFSET)

  /* Get the parent type that aParentFrame has. */
  static ParentType GetParentType(nsIFrame* aParentFrame) {
    return GetParentType(aParentFrame->Type());
  }

  /* Get the parent type for the given LayoutFrameType */
  static ParentType GetParentType(mozilla::LayoutFrameType aFrameType);

  static bool IsRubyParentType(ParentType aParentType) {
    return (aParentType == eTypeRuby ||
            aParentType == eTypeRubyBase ||
            aParentType == eTypeRubyBaseContainer ||
            aParentType == eTypeRubyText ||
            aParentType == eTypeRubyTextContainer);
  }

  static bool IsTableParentType(ParentType aParentType) {
    return (aParentType == eTypeTable ||
            aParentType == eTypeRow ||
            aParentType == eTypeRowGroup ||
            aParentType == eTypeColGroup);
  }

  /* A constructor function that just creates an nsIFrame object.  The caller
     is responsible for initializing the object, adding it to frame lists,
     constructing frames for the children, etc.

     @param nsIPresShell the presshell whose arena should be used to allocate
                         the frame.
     @param nsStyleContext the style context to use for the frame. */
  typedef nsIFrame* (* FrameCreationFunc)(nsIPresShell*, nsStyleContext*);
  typedef nsContainerFrame* (* ContainerFrameCreationFunc)(nsIPresShell*, nsStyleContext*);
  typedef nsBlockFrame* (* BlockFrameCreationFunc)(nsIPresShell*, nsStyleContext*);

  /* A function that can be used to get a FrameConstructionData.  Such
     a function is allowed to return null.

     @param nsIContent the node for which the frame is being constructed.
     @param nsStyleContext the style context to be used for the frame.
  */
  struct FrameConstructionData;
  typedef const FrameConstructionData*
    (* FrameConstructionDataGetter)(Element*, nsStyleContext*);

  /* A constructor function that's used for complicated construction tasks.
     This is expected to create the new frame, initialize it, add whatever
     needs to be added to aFrameItems (XXXbz is that really necessary?  Could
     caller add?  Might there be cases when the returned frame or its
     placeholder is not the thing that ends up in aFrameItems?  If not, would
     it be safe to do the add into the frame construction state after
     processing kids?  Look into this as a followup!), process children as
     needed, etc.  It is NOT expected to deal with setting the frame on the
     content.

     @param aState the frame construction state to use.
     @param aItem the frame construction item to use
     @param aParentFrame the frame to set as the parent of the
                         newly-constructed frame.
     @param aStyleDisplay the display struct from aItem's mStyleContext
     @param aFrameItems the frame list to add the new frame (or its
                        placeholder) to.
     @return the frame that was constructed.  This frame is what the caller
             will set as the frame on the content.  Guaranteed non-null.
  */
  typedef nsIFrame*
    (nsCSSFrameConstructor::* FrameFullConstructor)(nsFrameConstructorState& aState,
                                                    FrameConstructionItem& aItem,
                                                    nsContainerFrame* aParentFrame,
                                                    const nsStyleDisplay* aStyleDisplay,
                                                    nsFrameItems& aFrameItems);

  /* Bits that modify the way a FrameConstructionData is handled */

  /* If the FCDATA_SKIP_FRAMESET bit is set, then the frame created should not
     be set as the primary frame on the content node.  This should only be used
     in very rare cases when we create more than one frame for a given content
     node. */
#define FCDATA_SKIP_FRAMESET 0x1
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
  /* If FCDATA_WRAP_KIDS_IN_BLOCKS is set, the inline kids of the frame
     will be wrapped in blocks.  This is only usable for MathML at the
     moment. */
#define FCDATA_WRAP_KIDS_IN_BLOCKS 0x20
  /* If FCDATA_SUPPRESS_FRAME is set, no frame should be created for the
     content.  If this bit is set, nothing else in the struct needs to be
     set. */
#define FCDATA_SUPPRESS_FRAME 0x40
  /* If FCDATA_MAY_NEED_SCROLLFRAME is set, the new frame should be wrapped in
     a scrollframe if its overflow type so requires. */
#define FCDATA_MAY_NEED_SCROLLFRAME 0x80
#ifdef MOZ_XUL
  /* If FCDATA_IS_POPUP is set, the new frame is a XUL popup frame.  These need
     some really weird special handling.  */
#define FCDATA_IS_POPUP 0x100
#endif /* MOZ_XUL */
  /* If FCDATA_SKIP_ABSPOS_PUSH is set, don't push this frame as an
     absolute containing block, no matter what its style says. */
#define FCDATA_SKIP_ABSPOS_PUSH 0x200
  /* If FCDATA_DISALLOW_GENERATED_CONTENT is set, then don't allow generated
     content when processing kids of this frame.  This should not be used with
     FCDATA_FUNC_IS_FULL_CTOR */
#define FCDATA_DISALLOW_GENERATED_CONTENT 0x400
  /* If FCDATA_IS_TABLE_PART is set, then the frame is some sort of
     table-related thing and we should not attempt to fetch a table-cell parent
     for it if it's inside another table-related frame. */
#define FCDATA_IS_TABLE_PART 0x800
  /* If FCDATA_IS_INLINE is set, then the frame is a non-replaced CSS
     inline box. */
#define FCDATA_IS_INLINE 0x1000
  /* If FCDATA_IS_LINE_PARTICIPANT is set, the frame is something that will
     return true for IsFrameOfType(nsIFrame::eLineParticipant) */
#define FCDATA_IS_LINE_PARTICIPANT 0x2000
  /* If FCDATA_IS_LINE_BREAK is set, the frame is something that will
     induce a line break boundary before and after itself. */
#define FCDATA_IS_LINE_BREAK 0x4000
  /* If FCDATA_ALLOW_BLOCK_STYLES is set, allow block styles when processing
     children.  This should not be used with FCDATA_FUNC_IS_FULL_CTOR. */
#define FCDATA_ALLOW_BLOCK_STYLES 0x8000
  /* If FCDATA_USE_CHILD_ITEMS is set, then use the mChildItems in the relevant
     FrameConstructionItem instead of trying to process the content's children.
     This can be used with or without FCDATA_FUNC_IS_FULL_CTOR.
     The child items might still need table pseudo processing. */
#define FCDATA_USE_CHILD_ITEMS 0x10000
  /* If FCDATA_FORCED_NON_SCROLLABLE_BLOCK is set, then this block
     would have been scrollable but has been forced to be
     non-scrollable due to being in a paginated context. */
#define FCDATA_FORCED_NON_SCROLLABLE_BLOCK 0x20000
  /* If FCDATA_CREATE_BLOCK_WRAPPER_FOR_ALL_KIDS is set, then create a
     block formatting context wrapper around the kids of this frame
     using the FrameConstructionData's mPseudoAtom for its anonymous
     box type. */
#define FCDATA_CREATE_BLOCK_WRAPPER_FOR_ALL_KIDS 0x40000
  /* If FCDATA_IS_SVG_TEXT is set, then this text frame is a descendant of
     an SVG text frame. */
#define FCDATA_IS_SVG_TEXT 0x80000
  /**
   * display:contents
   */
#define FCDATA_IS_CONTENTS 0x100000
  /**
   * When FCDATA_CREATE_BLOCK_WRAPPER_FOR_ALL_KIDS is set, this bit says
   * if we should create a grid/flex/columnset container instead of
   * a block wrapper when the styles says so.
   */
#define FCDATA_ALLOW_GRID_FLEX_COLUMNSET 0x200000

  /* Structure representing information about how a frame should be
     constructed.  */
  struct FrameConstructionData {
    // Flag bits that can modify the way the construction happens
    uint32_t mBits;
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
    // For cases when FCDATA_CREATE_BLOCK_WRAPPER_FOR_ALL_KIDS is set, the
    // anonymous box type to use for that wrapper.
    nsICSSAnonBoxPseudo * const * const mAnonBoxPseudo;
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
    const int32_t mInt;
    const FrameConstructionData mData;
  };

  struct FrameConstructionDataByDisplay {
#ifdef DEBUG
    const mozilla::StyleDisplay mDisplay;
#endif
    const FrameConstructionData mData;
  };

#ifdef DEBUG
#define FCDATA_FOR_DISPLAY(_display, _fcdata) \
  { _display, _fcdata }
#else
#define FCDATA_FOR_DISPLAY(_display, _fcdata) \
  { _fcdata }
#endif

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
    FindDataByInt(int32_t aInt, Element* aElement,
                  nsStyleContext* aStyleContext,
                  const FrameConstructionDataByInt* aDataPtr,
                  uint32_t aDataLength);

  /* A function that takes a tag, content, style context, and array of
     FrameConstructionDataByTags and finds the appropriate frame construction
     data to use and returns it.  This can return null if none of the tags
     match or if the matching tag has a FrameConstructionDataGetter that
     returns null. */
  static const FrameConstructionData*
    FindDataByTag(nsIAtom* aTag, Element* aElement,
                  nsStyleContext* aStyleContext,
                  const FrameConstructionDataByTag* aDataPtr,
                  uint32_t aDataLength);

  /* A class representing a list of FrameConstructionItems */
  class FrameConstructionItemList final {
  public:
    FrameConstructionItemList() :
      mInlineCount(0),
      mBlockCount(0),
      mLineParticipantCount(0),
      mItemCount(0),
      mLineBoundaryAtStart(false),
      mLineBoundaryAtEnd(false),
      mParentHasNoXBLChildren(false),
      mTriedConstructingFrames(false)
    {
      memset(mDesiredParentCounts, 0, sizeof(mDesiredParentCounts));
    }

    ~FrameConstructionItemList() {
      while (FrameConstructionItem* item = mItems.popFirst()) {
        delete item;
      }

      // Create the undisplayed entries for our mUndisplayedItems, if any, but
      // only if we have tried constructing frames for this item list.  If we
      // haven't, then we're just throwing it away and will probably try again.
      if (!mUndisplayedItems.IsEmpty() && mTriedConstructingFrames) {
        // We could store the frame manager in a member, but just
        // getting it off the style context is not too bad.
        nsFrameManager *mgr =
          mUndisplayedItems[0].mStyleContext->PresContext()->FrameManager();
        for (uint32_t i = 0; i < mUndisplayedItems.Length(); ++i) {
          UndisplayedItem& item = mUndisplayedItems[i];
          mgr->SetUndisplayedContent(item.mContent, item.mStyleContext);
        }
      }
    }

    void SetLineBoundaryAtStart(bool aBoundary) { mLineBoundaryAtStart = aBoundary; }
    void SetLineBoundaryAtEnd(bool aBoundary) { mLineBoundaryAtEnd = aBoundary; }
    void SetParentHasNoXBLChildren(bool aHasNoXBLChildren) {
      mParentHasNoXBLChildren = aHasNoXBLChildren;
    }
    void SetTriedConstructingFrames() { mTriedConstructingFrames = true; }
    bool HasLineBoundaryAtStart() { return mLineBoundaryAtStart; }
    bool HasLineBoundaryAtEnd() { return mLineBoundaryAtEnd; }
    bool ParentHasNoXBLChildren() { return mParentHasNoXBLChildren; }
    bool IsEmpty() const { return mItems.isEmpty(); }
    bool AnyItemsNeedBlockParent() const { return mLineParticipantCount != 0; }
    bool AreAllItemsInline() const { return mInlineCount == mItemCount; }
    bool AreAllItemsBlock() const { return mBlockCount == mItemCount; }
    bool AllWantParentType(ParentType aDesiredParentType) const {
      return mDesiredParentCounts[aDesiredParentType] == mItemCount;
    }

    // aSuppressWhiteSpaceOptimizations is true if optimizations that
    // skip constructing whitespace frames for this item or items
    // around it cannot be performed.
    // Also, the return value is always non-null, thanks to infallible 'new'.
    FrameConstructionItem* AppendItem(const FrameConstructionData* aFCData,
                                      nsIContent* aContent,
                                      nsIAtom* aTag,
                                      int32_t aNameSpaceID,
                                      PendingBinding* aPendingBinding,
                                      already_AddRefed<nsStyleContext>&& aStyleContext,
                                      bool aSuppressWhiteSpaceOptimizations,
                                      nsTArray<nsIAnonymousContentCreator::ContentInfo>* aAnonChildren)
    {
      FrameConstructionItem* item =
        new FrameConstructionItem(aFCData, aContent, aTag, aNameSpaceID,
                                  aPendingBinding, aStyleContext,
                                  aSuppressWhiteSpaceOptimizations,
                                  aAnonChildren);
      mItems.insertBack(item);
      ++mItemCount;
      ++mDesiredParentCounts[item->DesiredParentType()];
      return item;
    }

    // Arguments are the same as AppendItem().
    FrameConstructionItem* PrependItem(const FrameConstructionData* aFCData,
                                       nsIContent* aContent,
                                       nsIAtom* aTag,
                                       int32_t aNameSpaceID,
                                       PendingBinding* aPendingBinding,
                                       already_AddRefed<nsStyleContext>&& aStyleContext,
                                       bool aSuppressWhiteSpaceOptimizations,
                                       nsTArray<nsIAnonymousContentCreator::ContentInfo>* aAnonChildren)
    {
      FrameConstructionItem* item =
        new FrameConstructionItem(aFCData, aContent, aTag, aNameSpaceID,
                                  aPendingBinding, aStyleContext,
                                  aSuppressWhiteSpaceOptimizations,
                                  aAnonChildren);
      mItems.insertFront(item);
      ++mItemCount;
      ++mDesiredParentCounts[item->DesiredParentType()];
      return item;
    }

    void AppendUndisplayedItem(nsIContent* aContent,
                               nsStyleContext* aStyleContext) {
      mUndisplayedItems.AppendElement(UndisplayedItem(aContent, aStyleContext));
    }

    void InlineItemAdded() { ++mInlineCount; }
    void BlockItemAdded() { ++mBlockCount; }
    void LineParticipantItemAdded() { ++mLineParticipantCount; }

    class Iterator {
    public:
      explicit Iterator(FrameConstructionItemList& aList)
        : mCurrent(aList.mItems.getFirst())
        , mList(aList)
      {}
      Iterator(const Iterator& aOther) :
        mCurrent(aOther.mCurrent),
        mList(aOther.mList)
      {}

      bool operator==(const Iterator& aOther) const {
        MOZ_ASSERT(&mList == &aOther.mList, "Iterators for different lists?");
        return mCurrent == aOther.mCurrent;
      }
      bool operator!=(const Iterator& aOther) const {
        return !(*this == aOther);
      }
      Iterator& operator=(const Iterator& aOther) {
        MOZ_ASSERT(&mList == &aOther.mList, "Iterators for different lists?");
        mCurrent = aOther.mCurrent;
        return *this;
      }

      FrameConstructionItemList* List() {
        return &mList;
      }

      FrameConstructionItem& item() {
        MOZ_ASSERT(!IsDone(), "Should have checked IsDone()!");
        return *mCurrent;
      }

      const FrameConstructionItem& item() const {
        MOZ_ASSERT(!IsDone(), "Should have checked IsDone()!");
        return *mCurrent;
      }

      bool IsDone() const { return mCurrent == nullptr; }
      bool AtStart() const { return mCurrent == mList.mItems.getFirst(); }
      void Next() {
        NS_ASSERTION(!IsDone(), "Should have checked IsDone()!");
        mCurrent = mCurrent->getNext();
      }
      void Prev() {
        NS_ASSERTION(!AtStart(), "Should have checked AtStart()!");
        mCurrent = mCurrent ? mCurrent->getPrevious() : mList.mItems.getLast();
      }
      void SetToEnd() { mCurrent = nullptr; }

      // Skip over all items that want the given parent type. Return whether
      // the iterator is done after doing that.  The iterator must not be done
      // when this is called.
      inline bool SkipItemsWantingParentType(ParentType aParentType);

      // Skip over all items that want a parent type different from the given
      // one.  Return whether the iterator is done after doing that.  The
      // iterator must not be done when this is called.
      inline bool SkipItemsNotWantingParentType(ParentType aParentType);

      // Skip over non-replaced inline frames and positioned frames.
      // Return whether the iterator is done after doing that.
      // The iterator must not be done when this is called.
      inline bool SkipItemsThatNeedAnonFlexOrGridItem(
        const nsFrameConstructorState& aState,
        bool aIsWebkitBox);

      // Skip to the first frame that is a non-replaced inline or is
      // positioned.  Return whether the iterator is done after doing that.
      // The iterator must not be done when this is called.
      inline bool SkipItemsThatDontNeedAnonFlexOrGridItem(
        const nsFrameConstructorState& aState,
        bool aIsWebkitBox);

      // Skip over all items that do not want a ruby parent.  Return whether
      // the iterator is done after doing that.  The iterator must not be done
      // when this is called.
      inline bool SkipItemsNotWantingRubyParent();

      // Skip over whitespace.  Return whether the iterator is done after doing
      // that.  The iterator must not be done, and must be pointing to a
      // whitespace item when this is called.
      inline bool SkipWhitespace(nsFrameConstructorState& aState);

      // Remove the item pointed to by this iterator from its current list and
      // Append it to aTargetList.  This iterator is advanced to point to the
      // next item in its list.  aIter must not be done.  aTargetList must not be
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
      FrameConstructionItem* mCurrent;
      FrameConstructionItemList& mList;
    };

  private:
    struct UndisplayedItem {
      UndisplayedItem(nsIContent* aContent, nsStyleContext* aStyleContext) :
        mContent(aContent), mStyleContext(aStyleContext)
      {}

      nsIContent * const mContent;
      RefPtr<nsStyleContext> mStyleContext;
    };

    // Adjust our various counts for aItem being added or removed.  aDelta
    // should be either +1 or -1 depending on which is happening.
    void AdjustCountsForItem(FrameConstructionItem* aItem, int32_t aDelta);

    mozilla::LinkedList<FrameConstructionItem> mItems;
    uint32_t mInlineCount;
    uint32_t mBlockCount;
    uint32_t mLineParticipantCount;
    uint32_t mItemCount;
    uint32_t mDesiredParentCounts[eParentTypeCount];
    // True if there is guaranteed to be a line boundary before the
    // frames created by these items
    bool mLineBoundaryAtStart;
    // True if there is guaranteed to be a line boundary after the
    // frames created by these items
    bool mLineBoundaryAtEnd;
    // True if the parent is guaranteed to have no XBL anonymous children
    bool mParentHasNoXBLChildren;
    // True if we have tried constructing frames from this list
    bool mTriedConstructingFrames;

    nsTArray<UndisplayedItem> mUndisplayedItems;
  };

  typedef FrameConstructionItemList::Iterator FCItemIterator;

  /* A struct representing an item for which frames might need to be
   * constructed.  This contains all the information needed to construct the
   * frame other than the parent frame and whatever would be stored in the
   * frame constructor state. */
  struct FrameConstructionItem final
    : public mozilla::LinkedListElement<FrameConstructionItem> {
    FrameConstructionItem(const FrameConstructionData* aFCData,
                          nsIContent* aContent,
                          nsIAtom* aTag,
                          int32_t aNameSpaceID,
                          PendingBinding* aPendingBinding,
                          already_AddRefed<nsStyleContext>& aStyleContext,
                          bool aSuppressWhiteSpaceOptimizations,
                          nsTArray<nsIAnonymousContentCreator::ContentInfo>* aAnonChildren) :
      mFCData(aFCData), mContent(aContent), mTag(aTag),
      mPendingBinding(aPendingBinding), mStyleContext(aStyleContext),
      mNameSpaceID(aNameSpaceID),
      mSuppressWhiteSpaceOptimizations(aSuppressWhiteSpaceOptimizations),
      mIsText(false), mIsGeneratedContent(false),
      mIsAnonymousContentCreatorContent(false),
      mIsRootPopupgroup(false), mIsAllInline(false), mIsBlock(false),
      mHasInlineEnds(false), mIsPopup(false),
      mIsLineParticipant(false), mIsForSVGAElement(false)
    {
      if (aAnonChildren) {
        NS_ASSERTION(!(mFCData->mBits & FCDATA_FUNC_IS_FULL_CTOR) ||
                     mFCData->mFullConstructor ==
                       &nsCSSFrameConstructor::ConstructInline,
                     "This is going to fail");
        NS_ASSERTION(!(mFCData->mBits & FCDATA_USE_CHILD_ITEMS),
                     "nsIAnonymousContentCreator::CreateAnonymousContent "
                     "implementations should not output a list where the "
                     "items have children in this case");
        mAnonChildren.SwapElements(*aAnonChildren);
      }
    }
    ~FrameConstructionItem() {
      if (mIsGeneratedContent) {
        mContent->UnbindFromTree();
        NS_RELEASE(mContent);
      }
    }

    ParentType DesiredParentType() {
      return FCDATA_DESIRED_PARENT_TYPE(mFCData->mBits);
    }

    // Indicates whether (when in a flex or grid container) this item needs
    // to be wrapped in an anonymous block.  (Note that we implement
    // -webkit-box/-webkit-inline-box using our standard flexbox frame class,
    // but we use different rules for what gets wrapped. The aIsWebkitBox
    // parameter here tells us whether to use those different rules.)
    bool NeedsAnonFlexOrGridItem(const nsFrameConstructorState& aState,
                                 bool aIsWebkitBox);

    // Don't call this unless the frametree really depends on the answer!
    // Especially so for generated content, where we don't want to reframe
    // things.
    bool IsWhitespace(nsFrameConstructorState& aState) const;

    bool IsLineBoundary() const {
      return mIsBlock || (mFCData->mBits & FCDATA_IS_LINE_BREAK);
    }

    // Child frame construction items.
    FrameConstructionItemList mChildItems;

    // ContentInfo list for children that have yet to have
    // FrameConstructionItem objects created for them. This exists because
    // AddFrameConstructionItemsInternal needs a valid frame, but in the case
    // that nsIAnonymousContentCreator::CreateAnonymousContent returns items
    // that have their own children (so we have a tree of ContentInfo objects
    // rather than a flat list) we don't yet have a frame to provide to
    // AddFrameConstructionItemsInternal in order to create the items for the
    // grandchildren. That prevents FrameConstructionItems from being created
    // for these grandchildren (and any descendants that they may have),
    // otherwise they could have been added to the mChildItems member of their
    // parent FrameConstructionItem. As it is, the grandchildren ContentInfo
    // list has to be stored in this mAnonChildren member in order to delay
    // construction of the FrameConstructionItems for the grandchildren until
    // a frame has been created for their parent item.
    nsTArray<nsIAnonymousContentCreator::ContentInfo> mAnonChildren;

    // The FrameConstructionData to use.
    const FrameConstructionData* mFCData;
    // The nsIContent node to use when initializing the new frame.
    nsIContent* mContent;
    // The XBL-resolved tag name to use for frame construction.
    nsIAtom* mTag;
    // The PendingBinding for this frame construction item, if any.  May be
    // null.  We maintain a list of PendingBindings in the frame construction
    // state in the order in which AddToAttachedQueue should be called on them:
    // depth-first, post-order traversal order.  Since we actually traverse the
    // DOM in a mix of breadth-first and depth-first, it is the responsibility
    // of whoever constructs FrameConstructionItem kids of a given
    // FrameConstructionItem to push its mPendingBinding as the current
    // insertion point before doing so and pop it afterward.
    PendingBinding* mPendingBinding;
    // The style context to use for creating the new frame.
    RefPtr<nsStyleContext> mStyleContext;
    // The XBL-resolved namespace to use for frame construction.
    int32_t mNameSpaceID;
    // Whether optimizations to skip constructing textframes around
    // this content need to be suppressed.
    bool mSuppressWhiteSpaceOptimizations:1;
    // Whether this is a text content item.
    bool mIsText:1;
    // Whether this is a generated content container.
    // If it is, mContent is a strong pointer.
    bool mIsGeneratedContent:1;
    // Whether this is an item for nsIAnonymousContentCreator content.
    bool mIsAnonymousContentCreatorContent:1;
    // Whether this is an item for the root popupgroup.
    bool mIsRootPopupgroup:1;
    // Whether construction from this item will create only frames that are
    // IsInlineOutside() in the principal child list.  This is not precise, but
    // conservative: if true the frames will really be inline, whereas if false
    // they might still all be inline.
    bool mIsAllInline:1;
    // Whether construction from this item will create only frames that are
    // IsBlockOutside() in the principal child list.  This is not precise, but
    // conservative: if true the frames will really be blocks, whereas if false
    // they might still be blocks (and in particular, out-of-flows that didn't
    // find a containing block).
    bool mIsBlock:1;
    // Whether construction from this item will give leading and trailing
    // inline frames.  This is equal to mIsAllInline, except for inline frame
    // items, where it's always true, whereas mIsAllInline might be false due
    // to {ib} splits.
    bool mHasInlineEnds:1;
    // Whether construction from this item will create a popup that needs to
    // go into the global popup items.
    bool mIsPopup:1;
    // Whether this item should be treated as a line participant
    bool mIsLineParticipant:1;
    // Whether this item is for an SVG <a> element
    bool mIsForSVGAElement:1;

  private:
    FrameConstructionItem(const FrameConstructionItem& aOther) = delete; /* not implemented */
  };

  /**
   * Function to create the anonymous flex or grid items that we need.
   * If aParentFrame is not a nsFlexContainerFrame or nsGridContainerFrame then
   * this method is a NOP.
   * @param aItems the child frame construction items before pseudo creation
   * @param aParentFrame the parent frame
   */
  void CreateNeededAnonFlexOrGridItems(nsFrameConstructorState& aState,
                                       FrameConstructionItemList& aItems,
                                       nsIFrame* aParentFrame);

  enum RubyWhitespaceType
  {
    eRubyNotWhitespace,
    eRubyInterLevelWhitespace,
    // Includes inter-base and inter-annotation whitespace
    eRubyInterLeafWhitespace,
    eRubyInterSegmentWhitespace
  };

  /**
   * Function to compute the whitespace type according to the display
   * values of the previous and the next elements.
   */
  static inline RubyWhitespaceType ComputeRubyWhitespaceType(
    mozilla::StyleDisplay aPrevDisplay, mozilla::StyleDisplay aNextDisplay);

  /**
   * Function to interpret the type of whitespace between
   * |aStartIter| and |aEndIter|.
   */
  static inline RubyWhitespaceType InterpretRubyWhitespace(
    nsFrameConstructorState& aState,
    const FCItemIterator& aStartIter, const FCItemIterator& aEndIter);

  /**
   * Function to wrap consecutive misparented inline content into
   * a ruby base box or a ruby text box.
   */
  void WrapItemsInPseudoRubyLeafBox(FCItemIterator& aIter,
                                    nsStyleContext* aParentStyle,
                                    nsIContent* aParentContent);

  /**
   * Function to wrap consecutive misparented items
   * into a ruby level container.
   */
  inline void WrapItemsInPseudoRubyLevelContainer(
    nsFrameConstructorState& aState, FCItemIterator& aIter,
    nsStyleContext* aParentStyle, nsIContent* aParentContent);

  /**
   * Function to trim leading and trailing whitespaces.
   */
  inline void TrimLeadingAndTrailingWhitespaces(
    nsFrameConstructorState& aState, FrameConstructionItemList& aItems);

  /**
   * Function to create internal ruby boxes.
   */
  inline void CreateNeededPseudoInternalRubyBoxes(
    nsFrameConstructorState& aState,
    FrameConstructionItemList& aItems, nsIFrame* aParentFrame);

  /**
   * Function to create the pseudo intermediate containers we need.
   * @param aItems the child frame construction items before pseudo creation
   * @param aParentFrame the parent frame we're creating pseudos for
   */
  inline void CreateNeededPseudoContainers(nsFrameConstructorState& aState,
                                           FrameConstructionItemList& aItems,
                                           nsIFrame* aParentFrame);

  /**
   * Function to wrap consecutive items into a pseudo parent.
   */
  inline void WrapItemsInPseudoParent(nsIContent* aParentContent,
                                      nsStyleContext* aParentStyle,
                                      ParentType aWrapperType,
                                      FCItemIterator& aIter,
                                      const FCItemIterator& aEndIter);

  /**
   * Function to create the pseudo siblings we need.
   */
  inline void CreateNeededPseudoSiblings(nsFrameConstructorState& aState,
                                         FrameConstructionItemList& aItems,
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
  void AdjustParentFrame(nsContainerFrame**           aParentFrame,
                         const FrameConstructionData* aFCData,
                         nsStyleContext*              aStyleContext);

  // END TABLE SECTION

protected:
  static nsIFrame* CreatePlaceholderFrameFor(nsIPresShell*     aPresShell,
                                             nsIContent*       aContent,
                                             nsIFrame*         aFrame,
                                             nsContainerFrame* aParentFrame,
                                             nsIFrame*         aPrevInFlow,
                                             nsFrameState      aTypeBit);

  static nsIFrame* CreateBackdropFrameFor(nsIPresShell* aPresShell,
                                          nsIContent* aContent,
                                          nsIFrame* aFrame,
                                          nsContainerFrame* aParentFrame);

private:
  // ConstructSelectFrame puts the new frame in aFrameItems and
  // handles the kids of the select.
  nsIFrame* ConstructSelectFrame(nsFrameConstructorState& aState,
                                 FrameConstructionItem&   aItem,
                                 nsContainerFrame*        aParentFrame,
                                 const nsStyleDisplay*    aStyleDisplay,
                                 nsFrameItems&            aFrameItems);

  // ConstructFieldSetFrame puts the new frame in aFrameItems and
  // handles the kids of the fieldset
  nsIFrame* ConstructFieldSetFrame(nsFrameConstructorState& aState,
                                   FrameConstructionItem&   aItem,
                                   nsContainerFrame*        aParentFrame,
                                   const nsStyleDisplay*    aStyleDisplay,
                                   nsFrameItems&            aFrameItems);

  // ConstructDetailsFrame puts the new frame in aFrameItems and
  // handles the kids of the details.
  nsIFrame* ConstructDetailsFrame(nsFrameConstructorState& aState,
                                  FrameConstructionItem& aItem,
                                  nsContainerFrame* aParentFrame,
                                  const nsStyleDisplay* aStyleDisplay,
                                  nsFrameItems& aFrameItems);

  // aParentFrame might be null.  If it is, that means it was an
  // inline frame.
  static const FrameConstructionData* FindTextData(nsIFrame* aParentFrame);

  void ConstructTextFrame(const FrameConstructionData* aData,
                          nsFrameConstructorState& aState,
                          nsIContent*              aContent,
                          nsContainerFrame*        aParentFrame,
                          nsStyleContext*          aStyleContext,
                          nsFrameItems&            aFrameItems);

  // If aPossibleTextContent is a text node and doesn't have a frame, append a
  // frame construction item for it to aItems.
  void AddTextItemIfNeeded(nsFrameConstructorState& aState,
                           const InsertionPoint& aInsertion,
                           nsIContent* aPossibleTextContent,
                           FrameConstructionItemList& aItems);

  // If aParentContent's child aContent is a text node and
  // doesn't have a frame, try to create a frame for it.
  void ReframeTextIfNeeded(nsIContent* aParentContent,
                           nsIContent* aContent);

  void AddPageBreakItem(nsIContent* aContent,
                        FrameConstructionItemList& aItems);

  // Function to find FrameConstructionData for aElement.  Will return
  // null if aElement is not HTML.
  // aParentFrame might be null.  If it is, that means it was an
  // inline frame.
  static const FrameConstructionData* FindHTMLData(Element* aContent,
                                                   nsIAtom* aTag,
                                                   int32_t aNameSpaceID,
                                                   nsIFrame* aParentFrame,
                                                   nsStyleContext* aStyleContext);
  // HTML data-finding helper functions
  static const FrameConstructionData*
    FindImgData(Element* aElement, nsStyleContext* aStyleContext);
  static const FrameConstructionData*
    FindImgControlData(Element* aElement, nsStyleContext* aStyleContext);
  static const FrameConstructionData*
    FindInputData(Element* aElement, nsStyleContext* aStyleContext);
  static const FrameConstructionData*
    FindObjectData(Element* aElement, nsStyleContext* aStyleContext);
  static const FrameConstructionData*
    FindCanvasData(Element* aElement, nsStyleContext* aStyleContext);

  /* Construct a frame from the given FrameConstructionItem.  This function
     will handle adding the frame to frame lists, processing children, setting
     the frame as the primary frame for the item's content, and so forth.

     @param aItem the FrameConstructionItem to use.
     @param aState the frame construction state to use.
     @param aParentFrame the frame to set as the parent of the
                         newly-constructed frame.
     @param aFrameItems the frame list to add the new frame (or its
                        placeholder) to.
  */
  void ConstructFrameFromItemInternal(FrameConstructionItem& aItem,
                                      nsFrameConstructorState& aState,
                                      nsContainerFrame* aParentFrame,
                                      nsFrameItems& aFrameItems);

  // possible flags for AddFrameConstructionItemInternal's aFlags argument
  /* Allow xbl:base to affect the tag/namespace used. */
#define ITEM_ALLOW_XBL_BASE 0x1
  /* Allow page-break before and after items to be created if the
     style asks for them. */
#define ITEM_ALLOW_PAGE_BREAK 0x2
  /* The item is a generated content item. */
#define ITEM_IS_GENERATED_CONTENT 0x4
  /* The item is within an SVG text block frame. */
#define ITEM_IS_WITHIN_SVG_TEXT 0x8
  /* The item allows items to be created for SVG <textPath> children. */
#define ITEM_ALLOWS_TEXT_PATH_CHILD 0x10
  /* The item is content created by an nsIAnonymousContentCreator frame */
#define ITEM_IS_ANONYMOUSCONTENTCREATOR_CONTENT 0x20
  // The guts of AddFrameConstructionItems
  // aParentFrame might be null.  If it is, that means it was an
  // inline frame.
  void AddFrameConstructionItemsInternal(nsFrameConstructorState& aState,
                                         nsIContent*              aContent,
                                         nsContainerFrame*        aParentFrame,
                                         nsIAtom*                 aTag,
                                         int32_t                  aNameSpaceID,
                                         bool                     aSuppressWhiteSpaceOptimizations,
                                         nsStyleContext*          aStyleContext,
                                         uint32_t                 aFlags,
                                         nsTArray<nsIAnonymousContentCreator::ContentInfo>* aAnonChildren,
                                         FrameConstructionItemList& aItems);

  /**
   * Construct frames for the given item list and parent frame, and put the
   * resulting frames in aFrameItems.
   */
  void ConstructFramesFromItemList(nsFrameConstructorState& aState,
                                   FrameConstructionItemList& aItems,
                                   nsContainerFrame* aParentFrame,
                                   nsFrameItems& aFrameItems);
  void ConstructFramesFromItem(nsFrameConstructorState& aState,
                               FCItemIterator& aItem,
                               nsContainerFrame* aParentFrame,
                               nsFrameItems& aFrameItems);
  static bool AtLineBoundary(FCItemIterator& aIter);

  nsresult GetAnonymousContent(nsIContent* aParent,
                               nsIFrame* aParentFrame,
                               nsTArray<nsIAnonymousContentCreator::ContentInfo>& aAnonContent);

//MathML Mod - RBS
  /**
   * Takes the frames in aBlockItems and wraps them in a new anonymous block
   * frame whose content is aContent and whose parent will be aParentFrame.
   * The anonymous block is added to aNewItems and aBlockItems is cleared.
   */
  void FlushAccumulatedBlock(nsFrameConstructorState& aState,
                             nsIContent* aContent,
                             nsContainerFrame* aParentFrame,
                             nsFrameItems& aBlockItems,
                             nsFrameItems& aNewItems);

  // Function to find FrameConstructionData for aContent.  Will return
  // null if aContent is not MathML.
  static const FrameConstructionData* FindMathMLData(Element* aElement,
                                                     nsIAtom* aTag,
                                                     int32_t aNameSpaceID,
                                                     nsStyleContext* aStyleContext);

  // Function to find FrameConstructionData for aContent.  Will return
  // null if aContent is not XUL.
  static const FrameConstructionData* FindXULTagData(Element* aElement,
                                                     nsIAtom* aTag,
                                                     int32_t aNameSpaceID,
                                                     nsStyleContext* aStyleContext);
  // XUL data-finding helper functions and structures
#ifdef MOZ_XUL
  static const FrameConstructionData*
    FindPopupGroupData(Element* aElement, nsStyleContext* aStyleContext);
  // sXULTextBoxData used for both labels and descriptions
  static const FrameConstructionData sXULTextBoxData;
  static const FrameConstructionData*
    FindXULLabelData(Element* aElement, nsStyleContext* aStyleContext);
  static const FrameConstructionData*
    FindXULDescriptionData(Element* aElement, nsStyleContext* aStyleContext);
#ifdef XP_MACOSX
  static const FrameConstructionData*
    FindXULMenubarData(Element* aElement, nsStyleContext* aStyleContext);
#endif /* XP_MACOSX */
  static const FrameConstructionData*
    FindXULListBoxBodyData(Element* aElement, nsStyleContext* aStyleContext);
  static const FrameConstructionData*
    FindXULListItemData(Element* aElement, nsStyleContext* aStyleContext);
#endif /* MOZ_XUL */

  // Function to find FrameConstructionData for aContent using one of the XUL
  // display types.  Will return null if aDisplay doesn't have a XUL display
  // type.  This function performs no other checks, so should only be called if
  // we know for sure that the content is not something that should get a frame
  // constructed by tag.
  static const FrameConstructionData*
    FindXULDisplayData(const nsStyleDisplay* aDisplay,
                       Element* aElement,
                       nsStyleContext* aStyleContext);

  /**
   * Constructs an outer frame, an anonymous child that wraps its real
   * children, and its descendant frames.  This is used by both ConstructOuterSVG
   * and ConstructMarker, which both want an anonymous block child for their
   * children to go in to.
   */
  nsContainerFrame* ConstructFrameWithAnonymousChild(
                                  nsFrameConstructorState& aState,
                                  FrameConstructionItem&   aItem,
                                  nsContainerFrame*        aParentFrame,
                                  nsFrameItems&            aFrameItems,
                                  ContainerFrameCreationFunc aConstructor,
                                  ContainerFrameCreationFunc aInnerConstructor,
                                  nsICSSAnonBoxPseudo*     aInnerPseudo,
                                  bool                     aCandidateRootFrame);

  /**
   * Construct an nsSVGOuterSVGFrame.
   */
  nsIFrame* ConstructOuterSVG(nsFrameConstructorState& aState,
                              FrameConstructionItem&   aItem,
                              nsContainerFrame*        aParentFrame,
                              const nsStyleDisplay*    aDisplay,
                              nsFrameItems&            aFrameItems);

  /**
   * Construct an nsSVGMarkerFrame.
   */
  nsIFrame* ConstructMarker(nsFrameConstructorState& aState,
                            FrameConstructionItem&   aItem,
                            nsContainerFrame*        aParentFrame,
                            const nsStyleDisplay*    aDisplay,
                            nsFrameItems&            aFrameItems);

  static const FrameConstructionData* FindSVGData(Element* aElement,
                                                  nsIAtom* aTag,
                                                  int32_t aNameSpaceID,
                                                  nsIFrame* aParentFrame,
                                                  bool aIsWithinSVGText,
                                                  bool aAllowsTextPathChild,
                                                  nsStyleContext* aStyleContext);

  /* Not static because it does PropagateScrollToViewport.  If this
     changes, make this static */
  const FrameConstructionData*
    FindDisplayData(const nsStyleDisplay* aDisplay, Element* aElement,
                    nsStyleContext* aStyleContext);

  /**
   * Construct a scrollable block frame
   */
  nsIFrame* ConstructScrollableBlock(nsFrameConstructorState& aState,
                                     FrameConstructionItem&   aItem,
                                     nsContainerFrame*        aParentFrame,
                                     const nsStyleDisplay*    aDisplay,
                                     nsFrameItems&            aFrameItems);

  /**
   * Construct a scrollable block frame using the given block frame creation
   * function.
   */
  nsIFrame* ConstructScrollableBlockWithConstructor(
    nsFrameConstructorState& aState,
    FrameConstructionItem& aItem,
    nsContainerFrame* aParentFrame,
    const nsStyleDisplay* aDisplay,
    nsFrameItems& aFrameItems,
    BlockFrameCreationFunc aConstructor);

  /**
   * Construct a non-scrollable block frame
   */
  nsIFrame* ConstructNonScrollableBlock(nsFrameConstructorState& aState,
                                        FrameConstructionItem&   aItem,
                                        nsContainerFrame*        aParentFrame,
                                        const nsStyleDisplay*    aDisplay,
                                        nsFrameItems&            aFrameItems);

  /**
   * Construct a non-scrollable block frame using the given block frame creation
   * function.
   */
  nsIFrame* ConstructNonScrollableBlockWithConstructor(
    nsFrameConstructorState& aState,
    FrameConstructionItem& aItem,
    nsContainerFrame* aParentFrame,
    const nsStyleDisplay* aDisplay,
    nsFrameItems& aFrameItems,
    BlockFrameCreationFunc aConstructor);

  /**
   * This adds FrameConstructionItem objects to aItemsToConstruct for the
   * anonymous content returned by an nsIAnonymousContentCreator::
   * CreateAnonymousContent implementation.
   */
  void AddFCItemsForAnonymousContent(
            nsFrameConstructorState& aState,
            nsContainerFrame* aFrame,
            nsTArray<nsIAnonymousContentCreator::ContentInfo>& aAnonymousItems,
            FrameConstructionItemList& aItemsToConstruct,
            uint32_t aExtraFlags = 0);

  /**
   * Construct the frames for the children of aContent.  "children" is defined
   * as "whatever FlattenedChildIterator returns for aContent".  This means we're
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
   * @param aParentFrame the frame to use as the parent frame for the new in-flow
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
   * @param aPendingBinding Make sure to push this into aState before doing any
   *        child item construction.
   * @param aPossiblyLeafFrame if non-null, this should be used for the isLeaf
   *        test and the anonymous content creation.  If null, aFrame will be
   *        used.
   */
  void ProcessChildren(nsFrameConstructorState& aState,
                       nsIContent*              aContent,
                       nsStyleContext*          aStyleContext,
                       nsContainerFrame*        aParentFrame,
                       const bool               aCanHaveGeneratedContent,
                       nsFrameItems&            aFrameItems,
                       const bool               aAllowBlockStyles,
                       PendingBinding*          aPendingBinding,
                       nsIFrame*                aPossiblyLeafFrame = nullptr);

  /**
   * These two functions are used when we start frame creation from a non-root
   * element. They should recreate the same state that we would have
   * arrived at if we had built frames from the root frame to aFrame.
   * Therefore, any calls to PushFloatContainingBlock and
   * PushAbsoluteContainingBlock during frame construction should get
   * corresponding logic in these functions.
   */
public:
  enum ContainingBlockType {
    ABS_POS,
    FIXED_POS
  };
  nsContainerFrame* GetAbsoluteContainingBlock(nsIFrame* aFrame,
                                               ContainingBlockType aType);
  nsContainerFrame* GetFloatContainingBlock(nsIFrame* aFrame);

private:
  // Build a scroll frame:
  //  Calls BeginBuildingScrollFrame, InitAndRestoreFrame, and then FinishBuildingScrollFrame.
  // @param aNewFrame the created scrollframe --- output only
  // @param aParentFrame the geometric parent that the scrollframe will have.
  void
  BuildScrollFrame(nsFrameConstructorState& aState,
                   nsIContent*              aContent,
                   nsStyleContext*          aContentStyle,
                   nsIFrame*                aScrolledFrame,
                   nsContainerFrame*        aParentFrame,
                   nsContainerFrame*&       aNewFrame);

  // Builds the initial ScrollFrame
  already_AddRefed<nsStyleContext>
  BeginBuildingScrollFrame(nsFrameConstructorState& aState,
                           nsIContent*              aContent,
                           nsStyleContext*          aContentStyle,
                           nsContainerFrame*        aParentFrame,
                           nsIAtom*                 aScrolledPseudo,
                           bool                     aIsRoot,
                           nsContainerFrame*&       aNewFrame);

  // Completes the building of the scrollframe:
  // Creates a view for the scrolledframe and makes it the child of the scrollframe.
  void
  FinishBuildingScrollFrame(nsContainerFrame* aScrollFrame,
                            nsIFrame* aScrolledFrame);

  // InitializeSelectFrame puts scrollFrame in aFrameItems if aBuildCombobox is false
  // aBuildCombobox indicates if we are building a combobox that has a dropdown
  // popup widget or not.
  void
  InitializeSelectFrame(nsFrameConstructorState& aState,
                        nsContainerFrame*        aScrollFrame,
                        nsContainerFrame*        aScrolledFrame,
                        nsIContent*              aContent,
                        nsContainerFrame*        aParentFrame,
                        nsStyleContext*          aStyleContext,
                        bool                     aBuildCombobox,
                        PendingBinding*          aPendingBinding,
                        nsFrameItems&            aFrameItems);

  /**
   * ReResolve style for aElement then recreate frames if required.
   * Do nothing for other types of style changes, except for undisplayed nodes
   * (display:none/contents) which will have their style context updated in the
   * frame manager undisplayed maps.
   * @return null if frames were recreated, the new style context otherwise
   */
  nsStyleContext* MaybeRecreateFramesForElement(Element* aElement);

  /**
   * Recreate frames for aContent.
   * @param aContent the content to recreate frames for
   * @param aAsyncInsert if true then a restyle event will be posted to handle
   *   the required ContentInserted call instead of doing it immediately.
   * @param aFlags normally you want to pass REMOVE_FOR_RECONSTRUCTION here
   * @param aDestroyedFramesFor if non-null, it will contain the content that
   *   was actually reframed - it may be different than aContent.
   */
  void
  RecreateFramesForContent(nsIContent*  aContent,
                           bool         aAsyncInsert,
                           RemoveFlags  aFlags,
                           nsIContent** aDestroyedFramesFor);

  // If removal of aFrame from the frame tree requires reconstruction of some
  // containing block (either of aFrame or of its parent) due to {ib} splits or
  // table pseudo-frames, recreate the relevant frame subtree.  The return value
  // indicates whether this happened.  If this method returns true, *aResult is
  // the return value of ReframeContainingBlock or RecreateFramesForContent.  If
  // this method returns false, the value of *aResult is not affected.  aFrame
  // and aResult must not be null.  aFrame must be the result of a
  // GetPrimaryFrame() call on a content node (which means its parent is also
  // not null).   If this method returns true, aDestroyedFramesFor contains the
  // content that was reframed.
  bool MaybeRecreateContainerForFrameRemoval(nsIFrame*    aFrame,
                                             RemoveFlags  aFlags,
                                             nsIContent** aDestroyedFramesFor);

  nsIFrame* CreateContinuingOuterTableFrame(nsIPresShell*     aPresShell,
                                            nsPresContext*    aPresContext,
                                            nsIFrame*         aFrame,
                                            nsContainerFrame* aParentFrame,
                                            nsIContent*       aContent,
                                            nsStyleContext*   aStyleContext);

  nsIFrame* CreateContinuingTableFrame(nsIPresShell*     aPresShell,
                                       nsIFrame*         aFrame,
                                       nsContainerFrame* aParentFrame,
                                       nsIContent*       aContent,
                                       nsStyleContext*   aStyleContext);

  //----------------------------------------

  // Methods support creating block frames and their children

  already_AddRefed<nsStyleContext>
  GetFirstLetterStyle(nsIContent*      aContent,
                      nsStyleContext*  aStyleContext);

  already_AddRefed<nsStyleContext>
  GetFirstLineStyle(nsIContent*      aContent,
                    nsStyleContext*  aStyleContext);

  bool ShouldHaveFirstLetterStyle(nsIContent*      aContent,
                                    nsStyleContext*  aStyleContext);

  // Check whether a given block has first-letter style.  Make sure to
  // only pass in blocks!  And don't pass in null either.
  bool HasFirstLetterStyle(nsIFrame* aBlockFrame);

  bool ShouldHaveFirstLineStyle(nsIContent*      aContent,
                                  nsStyleContext*  aStyleContext);

  void ShouldHaveSpecialBlockStyle(nsIContent*      aContent,
                                   nsStyleContext*  aStyleContext,
                                   bool*          aHaveFirstLetterStyle,
                                   bool*          aHaveFirstLineStyle);

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
  // @param aPositionedFrameForAbsPosContainer if non-null, then the new
  // block should be an abs-pos container and aPositionedFrameForAbsPosContainer
  // is the frame whose style is making this block an abs-pos container.
  // @param aPendingBinding the pending binding  from this block's frame
  // construction item.
  void ConstructBlock(nsFrameConstructorState& aState,
                      nsIContent*              aContent,
                      nsContainerFrame*        aParentFrame,
                      nsContainerFrame*        aContentParentFrame,
                      nsStyleContext*          aStyleContext,
                      nsContainerFrame**       aNewFrame,
                      nsFrameItems&            aFrameItems,
                      nsIFrame*                aPositionedFrameForAbsPosContainer,
                      PendingBinding*          aPendingBinding);

  nsIFrame* ConstructInline(nsFrameConstructorState& aState,
                            FrameConstructionItem&   aItem,
                            nsContainerFrame*        aParentFrame,
                            const nsStyleDisplay*    aDisplay,
                            nsFrameItems&            aFrameItems);

  /**
   * Create any additional {ib} siblings needed to contain aChildItems and put
   * them in aSiblings.
   *
   * @param aState the frame constructor state
   * @param aInitialInline is an already-existing inline frame that will be
   *                       part of this {ib} split and come before everything
   *                       in aSiblings.
   * @param aIsPositioned true if aInitialInline is positioned.
   * @param aChildItems is a child list starting with a block; this method
   *                    assumes that the inline has already taken all the
   *                    children it wants.  When the method returns aChildItems
   *                    will be empty.
   * @param aSiblings the nsFrameItems to put the newly-created siblings into.
   *
   * This method is responsible for making any SetFrameIsIBSplit calls that are
   * needed.
   */
  void CreateIBSiblings(nsFrameConstructorState& aState,
                        nsContainerFrame* aInitialInline,
                        bool aIsPositioned,
                        nsFrameItems& aChildItems,
                        nsFrameItems& aSiblings);

  /**
   * For an inline aParentItem, construct its list of child
   * FrameConstructionItems and set its mIsAllInline flag appropriately.
   */
  void BuildInlineChildItems(nsFrameConstructorState& aState,
                             FrameConstructionItem& aParentItem,
                             bool aItemIsWithinSVGText,
                             bool aItemAllowsTextPathChild);

  // Determine whether we need to wipe out what we just did and start over
  // because we're doing something like adding block kids to an inline frame
  // (and therefore need an {ib} split).  aPrevSibling must be correct, even in
  // aIsAppend cases.  Passing aIsAppend false even when an append is happening
  // is ok in terms of correctness, but can lead to unnecessary reframing.  If
  // aIsAppend is true, then the caller MUST call
  // nsCSSFrameConstructor::AppendFramesToParent (as opposed to
  // nsFrameManager::InsertFrames directly) to add the new frames.
  // @return true if we reconstructed the containing block, false
  // otherwise
  bool WipeContainingBlock(nsFrameConstructorState& aState,
                             nsIFrame*                aContainingBlock,
                             nsIFrame*                aFrame,
                             FrameConstructionItemList& aItems,
                             bool                     aIsAppend,
                             nsIFrame*                aPrevSibling);

  void ReframeContainingBlock(nsIFrame*    aFrame,
                              RemoveFlags  aFlags,
                              nsIContent** aReframeContent);

  //----------------------------------------

  // Methods support :first-letter style

  nsFirstLetterFrame*
  CreateFloatingLetterFrame(nsFrameConstructorState& aState,
                            nsIContent*              aTextContent,
                            nsIFrame*                aTextFrame,
                            nsContainerFrame*        aParentFrame,
                            nsStyleContext*          aParentStyleContext,
                            nsStyleContext*          aStyleContext,
                            nsFrameItems&            aResult);

  void CreateLetterFrame(nsContainerFrame*        aBlockFrame,
                         nsContainerFrame*        aBlockContinuation,
                         nsIContent*              aTextContent,
                         nsContainerFrame*        aParentFrame,
                         nsFrameItems&            aResult);

  void WrapFramesInFirstLetterFrame(nsContainerFrame* aBlockFrame,
                                    nsFrameItems&     aBlockFrames);

  /**
   * Looks in the block aBlockFrame for a text frame that contains the
   * first-letter of the block and creates the necessary first-letter frames
   * and returns them in aLetterFrames.
   *
   * @param aBlockFrame the (first-continuation of) the block we are creating a
   *                    first-letter frame for
   * @param aBlockContinuation the current continuation of the block that we
   *                           are looking in for a textframe with suitable
   *                           contents for first-letter
   * @param aParentFrame the current frame whose children we are looking at for
   *                     a suitable first-letter textframe
   * @param aParentFrameList the first child of aParentFrame
   * @param aModifiedParent returns the parent of the textframe that contains
   *                        the first-letter
   * @param aTextFrame returns the textframe that had the first-letter
   * @param aPrevFrame returns the previous sibling of aTextFrame
   * @param aLetterFrames returns the frames that were created
   */
  void WrapFramesInFirstLetterFrame(nsContainerFrame*  aBlockFrame,
                                    nsContainerFrame*  aBlockContinuation,
                                    nsContainerFrame*  aParentFrame,
                                    nsIFrame*          aParentFrameList,
                                    nsContainerFrame** aModifiedParent,
                                    nsIFrame**         aTextFrame,
                                    nsIFrame**         aPrevFrame,
                                    nsFrameItems&      aLetterFrames,
                                    bool*              aStopLooking);

  void RecoverLetterFrames(nsContainerFrame* aBlockFrame);

  //
  void RemoveLetterFrames(nsIPresShell*     aPresShell,
                          nsContainerFrame* aBlockFrame);

  // Recursive helper for RemoveLetterFrames
  void RemoveFirstLetterFrames(nsIPresShell*     aPresShell,
                               nsContainerFrame* aFrame,
                               nsContainerFrame* aBlockFrame,
                               bool*             aStopLooking);

  // Special remove method for those pesky floating first-letter frames
  void RemoveFloatingFirstLetterFrames(nsIPresShell*    aPresShell,
                                       nsIFrame*        aBlockFrame);

  // Capture state for the frame tree rooted at the frame associated with the
  // content object, aContent
  void CaptureStateForFramesOf(nsIContent* aContent,
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
  void WrapFramesInFirstLineFrame(nsFrameConstructorState& aState,
                                  nsIContent*              aBlockContent,
                                  nsContainerFrame*        aBlockFrame,
                                  nsFirstLineFrame*        aLineFrame,
                                  nsFrameItems&            aFrameItems);

  // Handle the case when a block with first-line style is appended to (by
  // possibly calling WrapFramesInFirstLineFrame as needed).
  void AppendFirstLineFrames(nsFrameConstructorState& aState,
                             nsIContent*              aContent,
                             nsContainerFrame*        aBlockFrame,
                             nsFrameItems&            aFrameItems);

  void InsertFirstLineFrames(nsFrameConstructorState& aState,
                             nsIContent*              aContent,
                             nsIFrame*                aBlockFrame,
                             nsContainerFrame**       aParentFrame,
                             nsIFrame*                aPrevSibling,
                             nsFrameItems&            aFrameItems);

  /**
   * Find the right frame to use for aContent when looking for sibling
   * frames for aTargetContent.  If aPrevSibling is true, this
   * will look for last continuations, etc, as necessary.  This calls
   * IsValidSibling as needed; if that returns false it returns null.
   *
   * @param aContent the content to search for frames
   * @param aTargetContent the content we're finding a sibling frame for
   * @param aTargetContentDisplay the CSS display enum for aTargetContent if
   *          already known, UNSET_DISPLAY otherwise. It will be filled in
   *          if needed.
   * @param aParentFrame the nearest ancestor frame, used internally for
   *          finding ::after / ::before frames
   * @param aPrevSibling true if we're searching in reverse DOM order
   */
  nsIFrame* FindFrameForContentSibling(nsIContent* aContent,
                                       nsIContent* aTargetContent,
                                       mozilla::StyleDisplay& aTargetContentDisplay,
                                       nsContainerFrame* aParentFrame,
                                       bool aPrevSibling);

  /**
   * Find the frame for the content immediately preceding the one aIter
   * points to, following continuations if necessary.  aIter is passed by
   * value on purpose, so as not to modify the caller's iterator.
   *
   * @param aIter should be positioned such that aIter.GetPreviousChild()
   *          is the first content to search for frames
   * @param aTargetContent the content we're finding a sibling frame for
   * @param aTargetContentDisplay the CSS display enum for aTargetContent if
   *          already known, UNSET_DISPLAY otherwise. It will be filled in
   *          if needed.
   * @param aParentFrame the nearest ancestor frame, used inernally for
   *          finding ::after / ::before frames
   */
  nsIFrame* FindPreviousSibling(mozilla::dom::FlattenedChildIterator aIter,
                                nsIContent* aTargetContent,
                                mozilla::StyleDisplay& aTargetContentDisplay,
                                nsContainerFrame* aParentFrame);

  /**
   * Find the frame for the content node immediately following the one aIter
   * points to, following continuations if necessary.  aIter is passed by value
   * on purpose, so as not to modify the caller's iterator.
   *
   * @param aIter should be positioned such that aIter.GetNextChild()
   *          is the first content to search for frames
   * @param aTargetContent the content we're finding a sibling frame for
   * @param aTargetContentDisplay the CSS display enum for aTargetContent if
   *          already known, UNSET_DISPLAY otherwise. It will be filled in
   *          if needed.
   * @param aParentFrame the nearest ancestor frame, used inernally for
   *          finding ::after / ::before frames
   */
  nsIFrame* FindNextSibling(mozilla::dom::FlattenedChildIterator aIter,
                            nsIContent* aTargetContent,
                            mozilla::StyleDisplay& aTargetContentDisplay,
                            nsContainerFrame* aParentFrame);

  // Find the right previous sibling for an insertion.  This also updates the
  // parent frame to point to the correct continuation of the parent frame to
  // use, and returns whether this insertion is to be treated as an append.
  // aChild is the child being inserted.
  // aIsRangeInsertSafe returns whether it is safe to do a range insert with
  // aChild being the first child in the range. It is the callers'
  // responsibility to check whether a range insert is safe with regards to
  // fieldsets.
  // The skip parameters are used to ignore a range of children when looking
  // for a sibling. All nodes starting from aStartSkipChild and up to but not
  // including aEndSkipChild will be skipped over when looking for sibling
  // frames. Skipping a range can deal with XBL but not when there are multiple
  // insertion points.
  nsIFrame* GetInsertionPrevSibling(InsertionPoint* aInsertion, // inout
                                    nsIContent* aChild,
                                    bool* aIsAppend,
                                    bool* aIsRangeInsertSafe,
                                    nsIContent* aStartSkipChild = nullptr,
                                    nsIContent *aEndSkipChild = nullptr);

  /**
   * Return the insertion frame of the primary frame of aContent, or its nearest
   * ancestor that isn't display:contents.
   */
  nsContainerFrame* GetContentInsertionFrameFor(nsIContent* aContent);

  // see if aContent and aSibling are legitimate siblings due to restrictions
  // imposed by table columns
  // XXXbz this code is generally wrong, since the frame for aContent
  // may be constructed based on tag, not based on aDisplay!
  bool IsValidSibling(nsIFrame*              aSibling,
                      nsIContent*            aContent,
                      mozilla::StyleDisplay& aDisplay);

  void QuotesDirty();
  void CountersDirty();

  /**
   * Add the pair (aContent, aStyleContext) to the undisplayed items
   * in aList as needed.  This method enforces the invariant that all
   * style contexts in the undisplayed content map must be non-pseudo
   * contexts and also handles unbinding undisplayed generated content
   * as needed.
   */
  void SetAsUndisplayedContent(nsFrameConstructorState& aState,
                               FrameConstructionItemList& aList,
                               nsIContent* aContent,
                               nsStyleContext* aStyleContext,
                               bool aIsGeneratedContent);
  // Create touch caret frame.
  void ConstructAnonymousContentForCanvas(nsFrameConstructorState& aState,
                                          nsIFrame* aFrame,
                                          nsIContent* aDocElement);

public:

  friend class nsFrameConstructorState;

private:

  nsIDocument*        mDocument;  // Weak ref

  // See the comment at the start of ConstructRootFrame for more details
  // about the following frames.

  // This is just the outermost frame for the root element.
  nsContainerFrame*   mRootElementFrame;
  // This is the frame for the root element that has no pseudo-element style.
  nsIFrame*           mRootElementStyleFrame;
  // This is the containing block that contains the root element ---
  // the real "initial containing block" according to CSS 2.1.
  nsContainerFrame*   mDocElementContainingBlock;
  nsIFrame*           mPageSequenceFrame;
  nsQuoteList         mQuoteList;
  nsCounterManager    mCounterManager;
  // Current ProcessChildren depth.
  uint16_t            mCurrentDepth;
#ifdef DEBUG
  uint16_t            mUpdateCount;
#endif
  bool                mQuotesDirty : 1;
  bool                mCountersDirty : 1;
  bool                mIsDestroyingFrameTree : 1;
  // This is true if mDocElementContainingBlock supports absolute positioning
  bool                mHasRootAbsPosContainingBlock : 1;
  bool                mAlwaysCreateFramesForIgnorableWhitespace : 1;

  nsCOMPtr<nsILayoutHistoryState> mTempFrameTreeState;
};

#endif /* nsCSSFrameConstructor_h___ */
