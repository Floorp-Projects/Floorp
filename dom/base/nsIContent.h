/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsIContent_h___
#define nsIContent_h___

#include "mozilla/FlushType.h"
#include "nsINode.h"
#include "nsStringFwd.h"

// Forward declarations
class nsIURI;
class nsTextFragment;
class nsIFrame;

namespace mozilla {
class EventChainPreVisitor;
struct URLExtraData;
namespace dom {
struct BindContext;
class ShadowRoot;
class HTMLSlotElement;
}  // namespace dom
namespace widget {
enum class IMEEnabled;
struct IMEState;
}  // namespace widget
}  // namespace mozilla

// IID for the nsIContent interface
// Must be kept in sync with xpcom/rust/xpcom/src/interfaces/nonidl.rs
#define NS_ICONTENT_IID                              \
  {                                                  \
    0x8e1bab9d, 0x8815, 0x4d2c, {                    \
      0xa2, 0x4d, 0x7a, 0xba, 0x52, 0x39, 0xdc, 0x22 \
    }                                                \
  }

/**
 * A node of content in a document's content model. This interface
 * is supported by all content objects.
 */
class nsIContent : public nsINode {
 public:
  using IMEEnabled = mozilla::widget::IMEEnabled;
  using IMEState = mozilla::widget::IMEState;
  using BindContext = mozilla::dom::BindContext;

  void ConstructUbiNode(void* storage) override;

#ifdef MOZILLA_INTERNAL_API
  // If you're using the external API, the only thing you can know about
  // nsIContent is that it exists with an IID

  explicit nsIContent(already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
      : nsINode(std::move(aNodeInfo)) {
    MOZ_ASSERT(mNodeInfo);
    MOZ_ASSERT(static_cast<nsINode*>(this) == reinterpret_cast<nsINode*>(this));
    SetNodeIsContent();
  }
#endif  // MOZILLA_INTERNAL_API

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ICONTENT_IID)

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS_FINAL_DELETECYCLECOLLECTABLE

  NS_DECL_CYCLE_COLLECTION_CLASS(nsIContent)

  NS_DECL_DOMARENA_DESTROY

  NS_IMPL_FROMNODE_HELPER(nsIContent, IsContent())

  /**
   * Bind this content node to a tree.  If this method throws, the caller must
   * call UnbindFromTree() on the node.  In the typical case of a node being
   * appended to a parent, this will be called after the node has been added to
   * the parent's child list and before nsIDocumentObserver notifications for
   * the addition are dispatched.
   * BindContext propagates various information down the subtree; see its
   * documentation to know how to set it up.
   * @param aParent The new parent node for the content node. May be a document.
   * @note This method must not be called by consumers of nsIContent on a node
   *       that is already bound to a tree.  Call UnbindFromTree first.
   * @note This method will handle rebinding descendants appropriately (eg
   *       changing their binding parent as needed).
   * @note This method does not add the content node to aParent's child list
   * @throws NS_ERROR_OUT_OF_MEMORY if that happens
   *
   * TODO(emilio): Should we move to nsIContent::BindToTree most of the
   * FragmentOrElement / CharacterData duplicated code?
   */
  virtual nsresult BindToTree(BindContext&, nsINode& aParent) = 0;

  /**
   * Unbind this content node from a tree.  This will set its current document
   * and binding parent to null.  In the typical case of a node being removed
   * from a parent, this will be called after it has been removed from the
   * parent's child list and after the nsIDocumentObserver notifications for
   * the removal have been dispatched.
   * @param aDeep Whether to recursively unbind the entire subtree rooted at
   *        this node.  The only time false should be passed is when the
   *        parent node of the content is being destroyed.
   * @param aNullParent Whether to null out the parent pointer as well.  This
   *        is usually desirable.  This argument should only be false while
   *        recursively calling UnbindFromTree when a subtree is detached.
   * @note This method is safe to call on nodes that are not bound to a tree.
   */
  virtual void UnbindFromTree(bool aNullParent = true) = 0;

  enum {
    /**
     * All XBL flattened tree children of the node, as well as :before and
     * :after anonymous content and native anonymous children.
     *
     * @note the result children order is
     *   1. :before generated node
     *   2. Shadow DOM flattened tree children of this node
     *   3. native anonymous nodes
     *   4. :after generated node
     */
    eAllChildren = 0,

    /**
     * Skip native anonymous content created for placeholder of HTML input.
     */
    eSkipPlaceholderContent = 1 << 0,

    /**
     * Skip native anonymous content created by ancestor frames of the root
     * element's primary frame, such as scrollbar elements created by the root
     * scroll frame.
     */
    eSkipDocumentLevelNativeAnonymousContent = 1 << 1,
  };

  /**
   * Return the flattened tree children of the node, depending on the filter, as
   * well as native anonymous children.
   */
  virtual already_AddRefed<nsINodeList> GetChildren(uint32_t aFilter) = 0;

  /**
   * Makes this content anonymous
   * @see nsIAnonymousContentCreator
   */
  void SetIsNativeAnonymousRoot() {
    SetFlags(NODE_IS_IN_NATIVE_ANONYMOUS_SUBTREE |
             NODE_IS_NATIVE_ANONYMOUS_ROOT);
  }

  /**
   * Returns |this| if it is not chrome-only/native anonymous, otherwise
   * first non chrome-only/native anonymous ancestor.
   */
  nsIContent* FindFirstNonChromeOnlyAccessContent() const;

  /**
   * Return true iff this node is in an HTML document (in the HTML5 sense of
   * the term, i.e. not in an XHTML/XML document).
   */
  inline bool IsInHTMLDocument() const;

  /**
   * Returns true if in a chrome document
   */
  inline bool IsInChromeDocument() const;

  /**
   * Get the namespace that this element's tag is defined in
   * @return the namespace
   */
  inline int32_t GetNameSpaceID() const { return mNodeInfo->NamespaceID(); }

  inline bool IsHTMLElement() const {
    return IsInNamespace(kNameSpaceID_XHTML);
  }

  inline bool IsHTMLElement(const nsAtom* aTag) const {
    return mNodeInfo->Equals(aTag, kNameSpaceID_XHTML);
  }

  template <typename First, typename... Args>
  inline bool IsAnyOfHTMLElements(First aFirst, Args... aArgs) const {
    return IsHTMLElement() && IsNodeInternal(aFirst, aArgs...);
  }

  inline bool IsSVGElement() const { return IsInNamespace(kNameSpaceID_SVG); }

  inline bool IsSVGElement(const nsAtom* aTag) const {
    return mNodeInfo->Equals(aTag, kNameSpaceID_SVG);
  }

  template <typename First, typename... Args>
  inline bool IsAnyOfSVGElements(First aFirst, Args... aArgs) const {
    return IsSVGElement() && IsNodeInternal(aFirst, aArgs...);
  }

  inline bool IsXULElement() const { return IsInNamespace(kNameSpaceID_XUL); }

  inline bool IsXULElement(const nsAtom* aTag) const {
    return mNodeInfo->Equals(aTag, kNameSpaceID_XUL);
  }

  template <typename First, typename... Args>
  inline bool IsAnyOfXULElements(First aFirst, Args... aArgs) const {
    return IsXULElement() && IsNodeInternal(aFirst, aArgs...);
  }

  inline bool IsMathMLElement() const {
    return IsInNamespace(kNameSpaceID_MathML);
  }

  inline bool IsMathMLElement(const nsAtom* aTag) const {
    return mNodeInfo->Equals(aTag, kNameSpaceID_MathML);
  }

  template <typename First, typename... Args>
  inline bool IsAnyOfMathMLElements(First aFirst, Args... aArgs) const {
    return IsMathMLElement() && IsNodeInternal(aFirst, aArgs...);
  }

  bool IsGeneratedContentContainerForBefore() const {
    return IsRootOfNativeAnonymousSubtree() &&
           mNodeInfo->NameAtom() == nsGkAtoms::mozgeneratedcontentbefore;
  }

  bool IsGeneratedContentContainerForAfter() const {
    return IsRootOfNativeAnonymousSubtree() &&
           mNodeInfo->NameAtom() == nsGkAtoms::mozgeneratedcontentafter;
  }

  bool IsGeneratedContentContainerForMarker() const {
    return IsRootOfNativeAnonymousSubtree() &&
           mNodeInfo->NameAtom() == nsGkAtoms::mozgeneratedcontentmarker;
  }

  /**
   * Get direct access (but read only) to the text in the text content.
   * NOTE: For elements this is *not* the concatenation of all text children,
   * it is simply null;
   */
  virtual const nsTextFragment* GetText() = 0;

  /**
   * Get the length of the text content.
   * NOTE: This should not be called on elements.
   */
  virtual uint32_t TextLength() const = 0;

  /**
   * Determines if an event attribute name (such as onclick) is valid for
   * a given element type.
   * @note calls nsContentUtils::IsEventAttributeName with right flag
   * @note *Internal is overridden by subclasses as needed
   * @param aName the event name to look up
   */
  bool IsEventAttributeName(nsAtom* aName);

  virtual bool IsEventAttributeNameInternal(nsAtom* aName) { return false; }

  /**
   * Query method to see if the frame is nothing but whitespace
   * NOTE: Always returns false for elements
   */
  virtual bool TextIsOnlyWhitespace() = 0;

  /**
   * Thread-safe version of TextIsOnlyWhitespace.
   */
  virtual bool ThreadSafeTextIsOnlyWhitespace() const = 0;

  /**
   * Check if this content is focusable and in the current tab order.
   * Note: most callers should use nsIFrame::IsFocusable() instead as it
   *       checks visibility and other layout factors as well.
   * Tabbable is indicated by a nonnegative tabindex & is a subset of focusable.
   * For example, only the selected radio button in a group is in the
   * tab order, unless the radio group has no selection in which case
   * all of the visible, non-disabled radio buttons in the group are
   * in the tab order. On the other hand, all of the visible, non-disabled
   * radio buttons are always focusable via clicking or script.
   * Also, depending on either the accessibility.tabfocus pref or
   * a system setting (nowadays: Full keyboard access, mac only)
   * some widgets may be focusable but removed from the tab order.
   * @param  [inout, optional] aTabIndex the computed tab index
   *         In: default tabindex for element (-1 nonfocusable, == 0 focusable)
   *         Out: computed tabindex
   * @param  [optional] aTabIndex the computed tab index
   *         < 0 if not tabbable
   *         == 0 if in normal tab order
   *         > 0 can be tabbed to in the order specified by this value
   * @return whether the content is focusable via mouse, kbd or script.
   */
  bool IsFocusable(int32_t* aTabIndex = nullptr, bool aWithMouse = false);
  virtual bool IsFocusableInternal(int32_t* aTabIndex, bool aWithMouse);

  /*
   * Get desired IME state for the content.
   *
   * @return The desired IME status for the content.
   *         This is a combination of an IME enabled value and
   *         an IME open value of widget::IMEState.
   *         If you return IMEEnabled::Disabled, you should not set the OPEN
   *         nor CLOSE value.
   *         IMEEnabled::Password should be returned only from password editor,
   *         this value has a special meaning. It is used as alternative of
   *         IMEEnabled::Disabled. IMEENabled::Plugin should be returned only
   *         when plug-in has focus.  When a plug-in is focused content, we
   *         should send native events directly. Because we don't process some
   *         native events, but they may be needed by the plug-in.
   */
  virtual IMEState GetDesiredIMEState();

  /**
   * Gets the ShadowRoot binding for this element.
   *
   * @return The ShadowRoot currently bound to this element.
   */
  inline mozilla::dom::ShadowRoot* GetShadowRoot() const;

  /**
   * Gets the root of the node tree for this content if it is in a shadow tree.
   *
   * @return The ShadowRoot that is the root of the node tree.
   */
  mozilla::dom::ShadowRoot* GetContainingShadow() const {
    const nsExtendedContentSlots* slots = GetExistingExtendedContentSlots();
    return slots ? slots->mContainingShadow.get() : nullptr;
  }

  /**
   * Gets the assigned slot associated with this content.
   *
   * @return The assigned slot element or null.
   */
  mozilla::dom::HTMLSlotElement* GetAssignedSlot() const {
    const nsExtendedContentSlots* slots = GetExistingExtendedContentSlots();
    return slots ? slots->mAssignedSlot.get() : nullptr;
  }

  /**
   * Sets the assigned slot associated with this content.
   *
   * @param aSlot The assigned slot.
   */
  void SetAssignedSlot(mozilla::dom::HTMLSlotElement* aSlot);

  /**
   * Gets the assigned slot associated with this content based on parent's
   * shadow root mode. Returns null if parent's shadow root is "closed".
   * https://dom.spec.whatwg.org/#dom-slotable-assignedslot
   *
   * @return The assigned slot element or null.
   */
  mozilla::dom::HTMLSlotElement* GetAssignedSlotByMode() const;

  mozilla::dom::HTMLSlotElement* GetManualSlotAssignment() const {
    const nsExtendedContentSlots* slots = GetExistingExtendedContentSlots();
    return slots ? slots->mManualSlotAssignment : nullptr;
  }

  void SetManualSlotAssignment(mozilla::dom::HTMLSlotElement* aSlot) {
    MOZ_ASSERT(aSlot || GetExistingExtendedContentSlots());
    ExtendedContentSlots()->mManualSlotAssignment = aSlot;
  }

  /**
   * Same as GetFlattenedTreeParentNode, but returns null if the parent is
   * non-nsIContent.
   */
  inline nsIContent* GetFlattenedTreeParent() const;

  /**
   * Get the index of a child within this content's flat tree children.
   *
   * @param aPossibleChild the child to get the index of.
   * @return the index of the child, or Nothing if not a child. Be aware that
   *         anonymous children (e.g. a <div> child of an <input> element) will
   *         result in Nothing.
   */
  mozilla::Maybe<uint32_t> ComputeFlatTreeIndexOf(
      const nsINode* aPossibleChild) const;

 protected:
  // Handles getting inserted or removed directly under a <slot> element.
  // This is meant to only be called from the two functions below.
  inline void HandleInsertionToOrRemovalFromSlot();

  // Handles Shadow-DOM-related state tracking. Meant to be called near the
  // end of BindToTree(), only if the tree we're in actually changed, that is,
  // after the subtree has been bound to the new parent.
  inline void HandleShadowDOMRelatedInsertionSteps(bool aHadParent);

  // Handles Shadow-DOM related state tracking. Meant to be called near the
  // beginning of UnbindFromTree(), before the node has lost the reference to
  // its parent.
  inline void HandleShadowDOMRelatedRemovalSteps(bool aNullParent);

 public:
  /**
   * This method is called when the parser finishes creating the element.  This
   * particularly means that it has done everything you would expect it to have
   * done after it encounters the > at the end of the tag (for HTML or XML).
   * This includes setting the attributes, setting the document / form, and
   * placing the element into the tree at its proper place.
   *
   * For container elements, this is called *before* any of the children are
   * created or added into the tree.
   *
   * NOTE: this is only called for elements listed in
   * RequiresDoneCreatingElement. This is an efficiency measure.
   *
   * If you also need to determine whether the parser is the one creating your
   * element (through createElement() or cloneNode() generally) then add a
   * uint32_t aFromParser to the NS_NewXXX() constructor for your element and
   * have the parser pass the appropriate flags. See HTMLInputElement.cpp and
   * nsHTMLContentSink::MakeContentObject().
   *
   * DO NOT USE THIS METHOD to get around the fact that it's hard to deal with
   * attributes dynamically.  If you make attributes affect your element from
   * this method, it will only happen on initialization and JavaScript will not
   * be able to create elements (which requires them to first create the
   * element and then call setAttribute() directly, at which point
   * DoneCreatingElement() has already been called and is out of the picture).
   */
  virtual void DoneCreatingElement() {}

  /**
   * This method is called when the parser finishes creating the element's
   * children, if any are present.
   *
   * NOTE: this is only called for elements listed in
   * RequiresDoneAddingChildren. This is an efficiency measure.
   *
   * If you also need to determine whether the parser is the one creating your
   * element (through createElement() or cloneNode() generally) then add a
   * boolean aFromParser to the NS_NewXXX() constructor for your element and
   * have the parser pass true.  See HTMLInputElement.cpp and
   * nsHTMLContentSink::MakeContentObject().
   *
   * @param aHaveNotified Whether there has been a
   *        ContentInserted/ContentAppended notification for this content node
   *        yet.
   */
  virtual void DoneAddingChildren(bool aHaveNotified) {}

  /**
   * For HTML textarea, select, and object elements, returns true if all
   * children have been added OR if the element was not created by the parser.
   * Returns true for all other elements.
   *
   * @returns false if the element was created by the parser and
   *                   it is an HTML textarea, select, or object
   *                   element and not all children have been added.
   *
   * @returns true otherwise.
   */
  virtual bool IsDoneAddingChildren() { return true; }

  /**
   * Returns true if an element needs its DoneCreatingElement method to be
   * called after it has been created.
   * @see nsIContent::DoneCreatingElement
   *
   * @param aNamespaceID the node's namespace ID
   * @param aName the node's tag name
   */
  static inline bool RequiresDoneCreatingElement(int32_t aNamespace,
                                                 nsAtom* aName) {
    if (aNamespace == kNameSpaceID_XHTML &&
        (aName == nsGkAtoms::input || aName == nsGkAtoms::button ||
         aName == nsGkAtoms::audio || aName == nsGkAtoms::video)) {
      MOZ_ASSERT(
          !RequiresDoneAddingChildren(aNamespace, aName),
          "Both DoneCreatingElement and DoneAddingChildren on a same element "
          "isn't supported.");
      return true;
    }
    return false;
  }

  /**
   * Returns true if an element needs its DoneAddingChildren method to be
   * called after all of its children have been added.
   * @see nsIContent::DoneAddingChildren
   *
   * @param aNamespace the node's namespace ID
   * @param aName the node's tag name
   */
  static inline bool RequiresDoneAddingChildren(int32_t aNamespace,
                                                nsAtom* aName) {
    return (aNamespace == kNameSpaceID_XHTML &&
            (aName == nsGkAtoms::select || aName == nsGkAtoms::textarea ||
             aName == nsGkAtoms::head || aName == nsGkAtoms::title ||
             aName == nsGkAtoms::object || aName == nsGkAtoms::output)) ||
           (aNamespace == kNameSpaceID_SVG && aName == nsGkAtoms::title) ||
           (aNamespace == kNameSpaceID_XUL && aName == nsGkAtoms::linkset);
  }

  /**
   * Get the ID of this content node (the atom corresponding to the
   * value of the id attribute).  This may be null if there is no ID.
   */
  nsAtom* GetID() const {
    if (HasID()) {
      return DoGetID();
    }
    return nullptr;
  }

  /**
   * Should be called when the node can become editable or when it can stop
   * being editable (for example when its contentEditable attribute changes,
   * when it is moved into an editable parent, ...).  If aNotify is true and
   * the node is an element, this will notify the state change.
   */
  virtual void UpdateEditableState(bool aNotify);

  /**
   * Destroy this node and its children. Ideally this shouldn't be needed
   * but for now we need to do it to break cycles.
   */
  virtual void DestroyContent() {}

  /**
   * Saves the form state of this node and its children.
   */
  virtual void SaveSubtreeState() = 0;

  /**
   * Getter and setter for our primary frame pointer.  This is the frame that
   * is most closely associated with the content. A frame is more closely
   * associated with the content than another frame if the one frame contains
   * directly or indirectly the other frame (e.g., when a frame is scrolled
   * there is a scroll frame that contains the frame being scrolled). This
   * frame is always the first continuation.
   *
   * In the case of absolutely positioned elements and floated elements, this
   * frame is the out of flow frame, not the placeholder.
   */
  nsIFrame* GetPrimaryFrame() const {
    return (IsInUncomposedDoc() || IsInShadowTree()) ? mPrimaryFrame : nullptr;
  }

  /**
   * Get the primary frame for this content with flushing
   *
   * @param aType the kind of flush to do, typically FlushType::Frames or
   *              FlushType::Layout
   * @return the primary frame
   */
  nsIFrame* GetPrimaryFrame(mozilla::FlushType aType);

  // Defined in nsIContentInlines.h because it needs nsIFrame.
  inline void SetPrimaryFrame(nsIFrame* aFrame);

  nsresult LookupNamespaceURIInternal(const nsAString& aNamespacePrefix,
                                      nsAString& aNamespaceURI) const;

  /**
   * If this content has independent selection, e.g., if this is input field
   * or textarea, this return TRUE.  Otherwise, false.
   */
  bool HasIndependentSelection() const;

  /**
   * If the content is a part of HTML editor, this returns editing
   * host content.  When the content is in designMode, this returns its body
   * element.  Also, when the content isn't editable, this returns null.
   */
  mozilla::dom::Element* GetEditingHost();

  bool SupportsLangAttr() const {
    return IsHTMLElement() || IsSVGElement() || IsXULElement();
  }

  /**
   * Determining language. Look at the nearest ancestor element that has a lang
   * attribute in the XML namespace or is an HTML/SVG element and has a lang in
   * no namespace attribute.
   *
   * Returns null if no language was specified. Can return the empty atom.
   */
  nsAtom* GetLang() const;

  bool GetLang(nsAString& aResult) const {
    if (auto* lang = GetLang()) {
      aResult.Assign(nsDependentAtomString(lang));
      return true;
    }

    return false;
  }

  // Overloaded from nsINode
  nsIURI* GetBaseURI(bool aTryUseXHRDocBaseURI = false) const override;

  // Returns base URI for style attribute.
  nsIURI* GetBaseURIForStyleAttr() const;

  // Returns the URL data for style attribute.
  // If aSubjectPrincipal is passed, it should be the scripted principal
  // responsible for generating the URL data.
  already_AddRefed<mozilla::URLExtraData> GetURLDataForStyleAttr(
      nsIPrincipal* aSubjectPrincipal = nullptr) const;

  void GetEventTargetParent(mozilla::EventChainPreVisitor& aVisitor) override;

  bool IsPurple() const { return mRefCnt.IsPurple(); }

  void RemovePurple() { mRefCnt.RemovePurple(); }

  bool OwnedOnlyByTheDOMTree() {
    uint32_t rc = mRefCnt.get();
    if (GetParent()) {
      --rc;
    }
    rc -= GetChildCount();
    return rc == 0;
  }

 protected:
  /**
   * Lazily allocated extended slots to avoid
   * that may only be instantiated when a content object is accessed
   * through the DOM. Rather than burn actual slots in the content
   * objects for each of these instance variables, we put them off
   * in a side structure that's only allocated when the content is
   * accessed through the DOM.
   */
  class nsExtendedContentSlots {
   public:
    nsExtendedContentSlots();
    virtual ~nsExtendedContentSlots();

    virtual void TraverseExtendedSlots(nsCycleCollectionTraversalCallback&);
    virtual void UnlinkExtendedSlots();

    virtual size_t SizeOfExcludingThis(
        mozilla::MallocSizeOf aMallocSizeOf) const;

    /**
     * @see nsIContent::GetContainingShadow
     */
    RefPtr<mozilla::dom::ShadowRoot> mContainingShadow;

    /**
     * @see nsIContent::GetAssignedSlot
     */
    RefPtr<mozilla::dom::HTMLSlotElement> mAssignedSlot;

    mozilla::dom::HTMLSlotElement* mManualSlotAssignment = nullptr;
  };

  class nsContentSlots : public nsINode::nsSlots {
   public:
    nsContentSlots() : nsINode::nsSlots(), mExtendedSlots(0) {}

    ~nsContentSlots() {
      if (!(mExtendedSlots & sNonOwningExtendedSlotsFlag)) {
        delete GetExtendedContentSlots();
      }
    }

    void Traverse(nsCycleCollectionTraversalCallback& aCb) override {
      nsINode::nsSlots::Traverse(aCb);
      if (mExtendedSlots) {
        GetExtendedContentSlots()->TraverseExtendedSlots(aCb);
      }
    }

    void Unlink() override {
      nsINode::nsSlots::Unlink();
      if (mExtendedSlots) {
        GetExtendedContentSlots()->UnlinkExtendedSlots();
      }
    }

    void SetExtendedContentSlots(nsExtendedContentSlots* aSlots, bool aOwning) {
      mExtendedSlots = reinterpret_cast<uintptr_t>(aSlots);
      if (!aOwning) {
        mExtendedSlots |= sNonOwningExtendedSlotsFlag;
      }
    }

    // OwnsExtendedSlots returns true if we have no extended slots or if we
    // have extended slots and own them.
    bool OwnsExtendedSlots() const {
      return !(mExtendedSlots & sNonOwningExtendedSlotsFlag);
    }

    nsExtendedContentSlots* GetExtendedContentSlots() const {
      return reinterpret_cast<nsExtendedContentSlots*>(
          mExtendedSlots & ~sNonOwningExtendedSlotsFlag);
    }

   private:
    static const uintptr_t sNonOwningExtendedSlotsFlag = 1u;

    uintptr_t mExtendedSlots;
  };

  // Override from nsINode
  nsContentSlots* CreateSlots() override { return new nsContentSlots(); }

  nsContentSlots* ContentSlots() {
    return static_cast<nsContentSlots*>(Slots());
  }

  const nsContentSlots* GetExistingContentSlots() const {
    return static_cast<nsContentSlots*>(GetExistingSlots());
  }

  nsContentSlots* GetExistingContentSlots() {
    return static_cast<nsContentSlots*>(GetExistingSlots());
  }

  virtual nsExtendedContentSlots* CreateExtendedSlots() {
    return new nsExtendedContentSlots();
  }

  const nsExtendedContentSlots* GetExistingExtendedContentSlots() const {
    const nsContentSlots* slots = GetExistingContentSlots();
    return slots ? slots->GetExtendedContentSlots() : nullptr;
  }

  nsExtendedContentSlots* GetExistingExtendedContentSlots() {
    nsContentSlots* slots = GetExistingContentSlots();
    return slots ? slots->GetExtendedContentSlots() : nullptr;
  }

  nsExtendedContentSlots* ExtendedContentSlots() {
    nsContentSlots* slots = ContentSlots();
    if (!slots->GetExtendedContentSlots()) {
      slots->SetExtendedContentSlots(CreateExtendedSlots(), true);
    }
    return slots->GetExtendedContentSlots();
  }

  /**
   * Hook for implementing GetID.  This is guaranteed to only be
   * called if HasID() is true.
   */
  nsAtom* DoGetID() const;

  ~nsIContent() = default;

 public:
#if defined(DEBUG) || defined(MOZ_DUMP_PAINTING)
#  define MOZ_DOM_LIST
#endif

#ifdef MOZ_DOM_LIST
  /**
   * An alias for List() with default arguments. Since some debuggers can't
   * figure the default arguments easily, having an out-of-line, non-static
   * function helps quite a lot.
   */
  void Dump();

  /**
   * List the content (and anything it contains) out to the given
   * file stream. Use aIndent as the base indent during formatting.
   */
  virtual void List(FILE* out = stdout, int32_t aIndent = 0) const = 0;

  /**
   * Dump the content (and anything it contains) out to the given
   * file stream. Use aIndent as the base indent during formatting.
   */
  virtual void DumpContent(FILE* out = stdout, int32_t aIndent = 0,
                           bool aDumpAll = true) const = 0;
#endif

  enum ETabFocusType {
    eTabFocus_textControlsMask =
        (1 << 0),  // textboxes and lists always tabbable
    eTabFocus_formElementsMask = (1 << 1),   // non-text form elements
    eTabFocus_linksMask = (1 << 2),          // links
    eTabFocus_any = 1 + (1 << 1) + (1 << 2)  // everything that can be focused
  };

  // Tab focus model bit field:
  static int32_t sTabFocusModel;

  // accessibility.tabfocus_applies_to_xul pref - if it is set to true,
  // the tabfocus bit field applies to xul elements.
  static bool sTabFocusModelAppliesToXUL;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIContent, NS_ICONTENT_IID)

#endif /* nsIContent_h___ */
