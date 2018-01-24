/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsIContent_h___
#define nsIContent_h___

#include "mozilla/Attributes.h"
#include "mozilla/dom/BorrowedAttrInfo.h"
#include "nsCaseTreatment.h" // for enum, cannot be forward-declared
#include "nsINode.h"
#include "nsStringFwd.h"

// Forward declarations
class nsAtom;
class nsIURI;
class nsAttrValue;
class nsAttrName;
class nsTextFragment;
class nsIFrame;
class nsXBLBinding;
class nsITextControlElement;

namespace mozilla {
class EventChainPreVisitor;
struct URLExtraData;
namespace dom {
class ShadowRoot;
class HTMLSlotElement;
} // namespace dom
namespace widget {
struct IMEState;
} // namespace widget
} // namespace mozilla

enum nsLinkState {
  eLinkState_Unvisited  = 1,
  eLinkState_Visited    = 2,
  eLinkState_NotLink    = 3
};

// IID for the nsIContent interface
// Must be kept in sync with xpcom/rust/xpcom/src/interfaces/nonidl.rs
#define NS_ICONTENT_IID \
{ 0x8e1bab9d, 0x8815, 0x4d2c, \
  { 0xa2, 0x4d, 0x7a, 0xba, 0x52, 0x39, 0xdc, 0x22 } }

/**
 * A node of content in a document's content model. This interface
 * is supported by all content objects.
 */
class nsIContent : public nsINode {
public:
  typedef mozilla::widget::IMEState IMEState;

#ifdef MOZILLA_INTERNAL_API
  // If you're using the external API, the only thing you can know about
  // nsIContent is that it exists with an IID

  explicit nsIContent(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
    : nsINode(aNodeInfo)
  {
    MOZ_ASSERT(mNodeInfo);
    SetNodeIsContent();
  }
#endif // MOZILLA_INTERNAL_API

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ICONTENT_IID)

  /**
   * Bind this content node to a tree.  If this method throws, the caller must
   * call UnbindFromTree() on the node.  In the typical case of a node being
   * appended to a parent, this will be called after the node has been added to
   * the parent's child list and before nsIDocumentObserver notifications for
   * the addition are dispatched.
   * @param aDocument The new document for the content node.  May not be null
   *                  if aParent is null.  Must match the current document of
   *                  aParent, if aParent is not null (note that
   *                  aParent->GetUncomposedDoc() can be null, in which case
   *                  this must also be null).
   * @param aParent The new parent for the content node.  May be null if the
   *                node is being bound as a direct child of the document.
   * @param aBindingParent The new binding parent for the content node.
   *                       This is must either be non-null if a particular
   *                       binding parent is desired or match aParent's binding
   *                       parent.
   * @param aCompileEventHandlers whether to initialize the event handlers in
   *        the document (used by nsXULElement)
   * @note either aDocument or aParent must be non-null.  If both are null,
   *       this method _will_ crash.
   * @note This method must not be called by consumers of nsIContent on a node
   *       that is already bound to a tree.  Call UnbindFromTree first.
   * @note This method will handle rebinding descendants appropriately (eg
   *       changing their binding parent as needed).
   * @note This method does not add the content node to aParent's child list
   * @throws NS_ERROR_OUT_OF_MEMORY if that happens
   */
  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers) = 0;

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
  virtual void UnbindFromTree(bool aDeep = true,
                              bool aNullParent = true) = 0;

  enum {
    /**
     * All XBL flattened tree children of the node, as well as :before and
     * :after anonymous content and native anonymous children.
     *
     * @note the result children order is
     *   1. :before generated node
     *   2. XBL flattened tree children of this node
     *   3. native anonymous nodes
     *   4. :after generated node
     */
    eAllChildren = 0,

    /**
     * All XBL explicit children of the node (see
     * http://www.w3.org/TR/xbl/#explicit3 ), as well as :before and :after
     * anonymous content and native anonymous children.
     *
     * @note the result children order is
     *   1. :before generated node
     *   2. XBL explicit children of the node
     *   3. native anonymous nodes
     *   4. :after generated node
     */
    eAllButXBL = 1,

    /**
     * Skip native anonymous content created for placeholder of HTML input,
     * used in conjunction with eAllChildren or eAllButXBL.
     */
    eSkipPlaceholderContent = 2,

    /**
     * Skip native anonymous content created by ancestor frames of the root
     * element's primary frame, such as scrollbar elements created by the root
     * scroll frame.
     */
    eSkipDocumentLevelNativeAnonymousContent = 4,
  };

  /**
   * Return either the XBL explicit children of the node or the XBL flattened
   * tree children of the node, depending on the filter, as well as
   * native anonymous children.
   *
   * @note calling this method with eAllButXBL will return children that are
   *  also in the eAllButXBL and eAllChildren child lists of other descendants
   *  of this node in the tree, but those other nodes cannot be reached from the
   *  eAllButXBL child list.
   */
  virtual already_AddRefed<nsINodeList> GetChildren(uint32_t aFilter) = 0;

  /**
   * Get whether this content is C++-generated anonymous content
   * @see nsIAnonymousContentCreator
   * @return whether this content is anonymous
   */
  bool IsRootOfNativeAnonymousSubtree() const
  {
    NS_ASSERTION(!HasFlag(NODE_IS_NATIVE_ANONYMOUS_ROOT) ||
                 (HasFlag(NODE_IS_ANONYMOUS_ROOT) &&
                  HasFlag(NODE_IS_IN_NATIVE_ANONYMOUS_SUBTREE)),
                 "Some flags seem to be missing!");
    return HasFlag(NODE_IS_NATIVE_ANONYMOUS_ROOT);
  }

  bool IsRootOfChromeAccessOnlySubtree() const
  {
    return HasFlag(NODE_IS_NATIVE_ANONYMOUS_ROOT |
                   NODE_IS_ROOT_OF_CHROME_ONLY_ACCESS);
  }

  /**
   * Makes this content anonymous
   * @see nsIAnonymousContentCreator
   */
  void SetIsNativeAnonymousRoot()
  {
    SetFlags(NODE_IS_ANONYMOUS_ROOT | NODE_IS_IN_NATIVE_ANONYMOUS_SUBTREE |
             NODE_IS_NATIVE_ANONYMOUS_ROOT | NODE_IS_NATIVE_ANONYMOUS);
  }

  /**
   * Returns |this| if it is not chrome-only/native anonymous, otherwise
   * first non chrome-only/native anonymous ancestor.
   */
  virtual nsIContent* FindFirstNonChromeOnlyAccessContent() const;

  /**
   * Returns true if and only if this node has a parent, but is not in
   * its parent's child list.
   */
  bool IsRootOfAnonymousSubtree() const
  {
    NS_ASSERTION(!IsRootOfNativeAnonymousSubtree() ||
                 (GetParent() && GetBindingParent() == GetParent()),
                 "root of native anonymous subtree must have parent equal "
                 "to binding parent");
    NS_ASSERTION(!GetParent() ||
                 ((GetBindingParent() == GetParent()) ==
                  HasFlag(NODE_IS_ANONYMOUS_ROOT)) ||
                 // Unfortunately default content for XBL insertion points is
                 // anonymous content that is bound with the parent of the
                 // insertion point as the parent but the bound element for the
                 // binding as the binding parent.  So we have to complicate
                 // the assert a bit here.
                 (GetBindingParent() &&
                  (GetBindingParent() == GetParent()->GetBindingParent()) ==
                  HasFlag(NODE_IS_ANONYMOUS_ROOT)),
                 "For nodes with parent, flag and GetBindingParent() check "
                 "should match");
    return HasFlag(NODE_IS_ANONYMOUS_ROOT);
  }

  /**
   * Returns true if there is NOT a path through child lists
   * from the top of this node's parent chain back to this node or
   * if the node is in native anonymous subtree without a parent.
   */
  bool IsInAnonymousSubtree() const
  {
    NS_ASSERTION(!IsInNativeAnonymousSubtree() || GetBindingParent() ||
                 (!IsInUncomposedDoc() &&
                  static_cast<nsIContent*>(SubtreeRoot())->IsInNativeAnonymousSubtree()),
                 "Must have binding parent when in native anonymous subtree which is in document.\n"
                 "Native anonymous subtree which is not in document must have native anonymous root.");
    return IsInNativeAnonymousSubtree() || (!IsInShadowTree() && GetBindingParent() != nullptr);
  }

  /*
   * Return true if this node is the shadow root of an use-element shadow tree.
   */
  bool IsRootOfUseElementShadowTree() const {
    return GetParent() && GetParent()->IsSVGElement(nsGkAtoms::use) &&
           IsRootOfAnonymousSubtree();
  }

  /**
   * Return true iff this node is in an HTML document (in the HTML5 sense of
   * the term, i.e. not in an XHTML/XML document).
   */
  inline bool IsInHTMLDocument() const;


  /**
   * Returns true if in a chrome document
   */
  virtual bool IsInChromeDocument() const;

  /**
   * Get the namespace that this element's tag is defined in
   * @return the namespace
   */
  inline int32_t GetNameSpaceID() const
  {
    return mNodeInfo->NamespaceID();
  }

  inline bool IsHTMLElement() const
  {
    return IsInNamespace(kNameSpaceID_XHTML);
  }

  inline bool IsHTMLElement(nsAtom* aTag) const
  {
    return mNodeInfo->Equals(aTag, kNameSpaceID_XHTML);
  }

  template<typename First, typename... Args>
  inline bool IsAnyOfHTMLElements(First aFirst, Args... aArgs) const
  {
    return IsHTMLElement() && IsNodeInternal(aFirst, aArgs...);
  }

  inline bool IsSVGElement() const
  {
    return IsInNamespace(kNameSpaceID_SVG);
  }

  inline bool IsSVGElement(nsAtom* aTag) const
  {
    return mNodeInfo->Equals(aTag, kNameSpaceID_SVG);
  }

  template<typename First, typename... Args>
  inline bool IsAnyOfSVGElements(First aFirst, Args... aArgs) const
  {
    return IsSVGElement() && IsNodeInternal(aFirst, aArgs...);
  }

  inline bool IsXULElement() const
  {
    return IsInNamespace(kNameSpaceID_XUL);
  }

  inline bool IsXULElement(nsAtom* aTag) const
  {
    return mNodeInfo->Equals(aTag, kNameSpaceID_XUL);
  }

  template<typename First, typename... Args>
  inline bool IsAnyOfXULElements(First aFirst, Args... aArgs) const
  {
    return IsXULElement() && IsNodeInternal(aFirst, aArgs...);
  }

  inline bool IsMathMLElement() const
  {
    return IsInNamespace(kNameSpaceID_MathML);
  }

  inline bool IsMathMLElement(nsAtom* aTag) const
  {
    return mNodeInfo->Equals(aTag, kNameSpaceID_MathML);
  }

  template<typename First, typename... Args>
  inline bool IsAnyOfMathMLElements(First aFirst, Args... aArgs) const
  {
    return IsMathMLElement() && IsNodeInternal(aFirst, aArgs...);
  }
  inline bool IsActiveChildrenElement() const
  {
    return mNodeInfo->Equals(nsGkAtoms::children, kNameSpaceID_XBL) &&
           GetBindingParent();
  }

  bool IsGeneratedContentContainerForBefore() const
  {
    return IsRootOfNativeAnonymousSubtree() &&
           mNodeInfo->NameAtom() == nsGkAtoms::mozgeneratedcontentbefore;
  }

  bool IsGeneratedContentContainerForAfter() const
  {
    return IsRootOfNativeAnonymousSubtree() &&
           mNodeInfo->NameAtom() == nsGkAtoms::mozgeneratedcontentafter;
  }

  /**
   * Get direct access (but read only) to the text in the text content.
   * NOTE: For elements this is *not* the concatenation of all text children,
   * it is simply null;
   */
  virtual const nsTextFragment *GetText() = 0;

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

  virtual bool IsEventAttributeNameInternal(nsAtom* aName)
  {
    return false;
  }

  /**
   * Set the text to the given value. If aNotify is true then
   * the document is notified of the content change.
   * NOTE: For elements this always ASSERTS and returns NS_ERROR_FAILURE
   */
  virtual nsresult SetText(const char16_t* aBuffer, uint32_t aLength,
                           bool aNotify) = 0;

  /**
   * Append the given value to the current text. If aNotify is true then
   * the document is notified of the content change.
   * NOTE: For elements this always ASSERTS and returns NS_ERROR_FAILURE
   */
  virtual nsresult AppendText(const char16_t* aBuffer, uint32_t aLength,
                              bool aNotify) = 0;

  /**
   * Set the text to the given value. If aNotify is true then
   * the document is notified of the content change.
   * NOTE: For elements this always asserts and returns NS_ERROR_FAILURE
   */
  nsresult SetText(const nsAString& aStr, bool aNotify)
  {
    return SetText(aStr.BeginReading(), aStr.Length(), aNotify);
  }

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
   * Method to see if the text node contains data that is useful
   * for a translation: i.e., it consists of more than just whitespace,
   * digits and punctuation.
   * NOTE: Always returns false for elements.
   */
  virtual bool HasTextForTranslation() = 0;

  /**
   * Append the text content to aResult.
   * NOTE: This asserts and returns for elements
   */
  virtual void AppendTextTo(nsAString& aResult) = 0;

  /**
   * Append the text content to aResult.
   * NOTE: This asserts and returns for elements
   */
  MOZ_MUST_USE
  virtual bool AppendTextTo(nsAString& aResult, const mozilla::fallible_t&) = 0;

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

  /**
   * The method focuses (or activates) element that accesskey is bound to. It is
   * called when accesskey is activated.
   *
   * @param aKeyCausesActivation - if true then element should be activated
   * @param aIsTrustedEvent - if true then event that is cause of accesskey
   *                          execution is trusted.
   * @return true if the focus was changed.
   */
  virtual bool PerformAccesskey(bool aKeyCausesActivation,
                                bool aIsTrustedEvent)
  {
    return false;
  }

  /*
   * Get desired IME state for the content.
   *
   * @return The desired IME status for the content.
   *         This is a combination of an IME enabled value and
   *         an IME open value of widget::IMEState.
   *         If you return DISABLED, you should not set the OPEN and CLOSE
   *         value.
   *         PASSWORD should be returned only from password editor, this value
   *         has a special meaning. It is used as alternative of DISABLED.
   *         PLUGIN should be returned only when plug-in has focus.  When a
   *         plug-in is focused content, we should send native events directly.
   *         Because we don't process some native events, but they may be needed
   *         by the plug-in.
   */
  virtual IMEState GetDesiredIMEState();

  /**
   * Gets content node with the binding (or native code, possibly on the
   * frame) responsible for our construction (and existence).  Used by
   * anonymous content (both XBL-generated and native-anonymous).
   *
   * null for all explicit content (i.e., content reachable from the top
   * of its GetParent() chain via child lists).
   *
   * @return the binding parent
   */
  virtual nsIContent* GetBindingParent() const
  {
    const nsExtendedContentSlots* slots = GetExistingExtendedContentSlots();
    return slots ? slots->mBindingParent : nullptr;
  }

  /**
   * Gets the current XBL binding that is bound to this element.
   *
   * @return the current binding.
   */
  nsXBLBinding* GetXBLBinding() const
  {
    if (!HasFlag(NODE_MAY_BE_IN_BINDING_MNGR)) {
      return nullptr;
    }

    return DoGetXBLBinding();
  }

  virtual nsXBLBinding* DoGetXBLBinding() const = 0;

  /**
   * Gets the ShadowRoot binding for this element.
   *
   * @return The ShadowRoot currently bound to this element.
   */
  inline mozilla::dom::ShadowRoot* GetShadowRoot() const;

  /**
   * Gets the root of the node tree for this content if it is in a shadow tree.
   * This method is called |GetContainingShadow| instead of |GetRootShadowRoot|
   * to avoid confusion with |GetShadowRoot|.
   *
   * @return The ShadowRoot that is the root of the node tree.
   */
  mozilla::dom::ShadowRoot* GetContainingShadow() const
  {
    const nsExtendedContentSlots* slots = GetExistingExtendedContentSlots();
    return slots ? slots->mContainingShadow.get() : nullptr;
  }

  /**
   * Gets the shadow host if this content is in a shadow tree. That is, the host
   * of |GetContainingShadow|, if its not null.
   *
   * @return The shadow host, if this is in shadow tree, or null.
   */
  nsIContent* GetContainingShadowHost() const;

  /**
   * Gets the assigned slot associated with this content.
   *
   * @return The assigned slot element or null.
   */
  mozilla::dom::HTMLSlotElement* GetAssignedSlot() const
  {
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

  nsIContent* GetXBLInsertionParent() const
  {
    nsIContent* ip = GetXBLInsertionPoint();
    return ip ? ip->GetParent() : nullptr;
  }

  /**
   * Gets the insertion parent element of the XBL binding.
   * The insertion parent is our one true parent in the transformed DOM.
   *
   * @return the insertion parent element.
   */
  nsIContent* GetXBLInsertionPoint() const
  {
    const nsExtendedContentSlots* slots = GetExistingExtendedContentSlots();
    return slots ? slots->mXBLInsertionPoint.get() : nullptr;
  }

  /**
   * Sets the insertion parent element of the XBL binding.
   *
   * @param aContent The insertion parent element.
   */
  void SetXBLInsertionPoint(nsIContent* aContent);

  /**
   * Same as GetFlattenedTreeParentNode, but returns null if the parent is
   * non-nsIContent.
   */
  inline nsIContent* GetFlattenedTreeParent() const;

  /**
   * Get the flattened tree parent for NAC holding from the document element,
   * from the point of view of the style system.
   *
   * Document-level anonymous content holds from the document element, even
   * though they should not be treated as such (they should be parented to the
   * document instead, and shouldn't inherit from the document element).
   */
  nsINode* GetFlattenedTreeParentForDocumentElementNAC() const;

  /**
   * API to check if this is a link that's traversed in response to user input
   * (e.g. a click event). Specializations for HTML/SVG/generic XML allow for
   * different types of link in different types of content.
   *
   * @param aURI Required out param. If this content is a link, a new nsIURI
   *             set to this link's URI will be passed out.
   *
   * @note The out param, aURI, is guaranteed to be set to a non-null pointer
   *   when the return value is true.
   *
   * XXXjwatt: IMO IsInteractiveLink would be a better name.
   */
  virtual bool IsLink(nsIURI** aURI) const = 0;

  /**
    * Get a pointer to the full href URI (fully resolved and canonicalized,
    * since it's an nsIURI object) for link elements.
    *
    * @return A pointer to the URI or null if the element is not a link or it
    *         has no HREF attribute.
    */
  virtual already_AddRefed<nsIURI> GetHrefURI() const
  {
    return nullptr;
  }

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
   * NOTE: this is currently only called for input and button, in the HTML
   * content sink.  If you want to call it on your element, modify the content
   * sink of your choice to do so.  This is an efficiency measure.
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
  virtual void DoneCreatingElement()
  {
  }

  /**
   * This method is called when the parser begins creating the element's
   * children, if any are present.
   *
   * This is only called for XTF elements currently.
   */
  virtual void BeginAddingChildren()
  {
  }

  /**
   * This method is called when the parser finishes creating the element's children,
   * if any are present.
   *
   * NOTE: this is currently only called for textarea, select, and object
   * elements in the HTML content sink. If you want to call it on your element,
   * modify the content sink of your choice to do so. This is an efficiency
   * measure.
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
  virtual void DoneAddingChildren(bool aHaveNotified)
  {
  }

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
  virtual bool IsDoneAddingChildren()
  {
    return true;
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
  virtual void DestroyContent()
  {
  }

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
  nsIFrame* GetPrimaryFrame() const
  {
    return (IsInUncomposedDoc() || IsInShadowTree()) ? mPrimaryFrame : nullptr;
  }

  // Defined in nsIContentInlines.h because it needs nsIFrame.
  inline void SetPrimaryFrame(nsIFrame* aFrame);

  nsresult LookupNamespaceURIInternal(const nsAString& aNamespacePrefix,
                                      nsAString& aNamespaceURI) const;

  /**
   * If this content has independent selection, e.g., if this is input field
   * or textarea, this return TRUE.  Otherwise, false.
   */
  bool HasIndependentSelection();

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

  // Returns true if this element is native-anonymous scrollbar content.
  bool IsNativeScrollbarContent() const {
    return IsNativeAnonymous() &&
           IsAnyOfXULElements(nsGkAtoms::scrollbar,
                              nsGkAtoms::resizer,
                              nsGkAtoms::scrollcorner);
  }

  // Overloaded from nsINode
  virtual already_AddRefed<nsIURI> GetBaseURI(bool aTryUseXHRDocBaseURI = false) const override;

  // Returns base URI for style attribute.
  nsIURI* GetBaseURIForStyleAttr() const;

  // Returns the URL data for style attribute.
  // If aSubjectPrincipal is passed, it should be the scripted principal
  // responsible for generating the URL data.
  already_AddRefed<mozilla::URLExtraData>
  GetURLDataForStyleAttr(nsIPrincipal* aSubjectPrincipal = nullptr) const;

  virtual nsresult GetEventTargetParent(
                     mozilla::EventChainPreVisitor& aVisitor) override;

  virtual bool IsPurple() = 0;
  virtual void RemovePurple() = 0;

  virtual bool OwnedOnlyByTheDOMTree() { return false; }

  virtual already_AddRefed<nsITextControlElement> GetAsTextControlElement()
  {
    return nullptr;
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
  class nsExtendedContentSlots
  {
  public:
    nsExtendedContentSlots();
    virtual ~nsExtendedContentSlots();

    virtual void Traverse(nsCycleCollectionTraversalCallback&);
    virtual void Unlink();

    /**
     * The nearest enclosing content node with a binding that created us.
     * @see nsIContent::GetBindingParent
     */
    nsIContent* mBindingParent;  // [Weak]

    /**
     * @see nsIContent::GetXBLInsertionPoint
     */
    nsCOMPtr<nsIContent> mXBLInsertionPoint;

    /**
     * @see nsIContent::GetContainingShadow
     */
    RefPtr<mozilla::dom::ShadowRoot> mContainingShadow;

    /**
     * @see nsIContent::GetAssignedSlot
     */
    RefPtr<mozilla::dom::HTMLSlotElement> mAssignedSlot;
  };

  class nsContentSlots : public nsINode::nsSlots
  {
  public:
    void Traverse(nsCycleCollectionTraversalCallback& aCb) override
    {
      nsINode::nsSlots::Traverse(aCb);
      if (mExtendedSlots) {
        mExtendedSlots->Traverse(aCb);
      }
    }

    void Unlink() override
    {
      nsINode::nsSlots::Unlink();
      if (mExtendedSlots) {
        mExtendedSlots->Unlink();
      }
    }

    mozilla::UniquePtr<nsExtendedContentSlots> mExtendedSlots;
  };

  // Override from nsINode
  nsContentSlots* CreateSlots() override
  {
    return new nsContentSlots();
  }

  nsContentSlots* ContentSlots()
  {
    return static_cast<nsContentSlots*>(Slots());
  }

  const nsContentSlots* GetExistingContentSlots() const
  {
    return static_cast<nsContentSlots*>(GetExistingSlots());
  }

  nsContentSlots* GetExistingContentSlots()
  {
    return static_cast<nsContentSlots*>(GetExistingSlots());
  }

  virtual nsExtendedContentSlots* CreateExtendedSlots()
  {
    return new nsExtendedContentSlots();
  }

  const nsExtendedContentSlots* GetExistingExtendedContentSlots() const
  {
    const nsContentSlots* slots = GetExistingContentSlots();
    return slots ? slots->mExtendedSlots.get() : nullptr;
  }

  nsExtendedContentSlots* GetExistingExtendedContentSlots()
  {
    nsContentSlots* slots = GetExistingContentSlots();
    return slots ? slots->mExtendedSlots.get() : nullptr;
  }

  nsExtendedContentSlots* ExtendedContentSlots()
  {
    nsContentSlots* slots = ContentSlots();
    if (!slots->mExtendedSlots) {
      slots->mExtendedSlots.reset(CreateExtendedSlots());
    }
    return slots->mExtendedSlots.get();
  }

  /**
   * Hook for implementing GetID.  This is guaranteed to only be
   * called if HasID() is true.
   */
  nsAtom* DoGetID() const;

public:
#ifdef DEBUG
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

  /**
   * Append to aOutDescription a short (preferably one line) string
   * describing the content.
   * Currently implemented for elements only.
   */
  virtual void Describe(nsAString& aOutDescription) const {
    aOutDescription = NS_LITERAL_STRING("(not an element)");
  }

  enum ETabFocusType {
    eTabFocus_textControlsMask = (1<<0),  // textboxes and lists always tabbable
    eTabFocus_formElementsMask = (1<<1),  // non-text form elements
    eTabFocus_linksMask = (1<<2),         // links
    eTabFocus_any = 1 + (1<<1) + (1<<2)   // everything that can be focused
  };

  // Tab focus model bit field:
  static int32_t sTabFocusModel;

  // accessibility.tabfocus_applies_to_xul pref - if it is set to true,
  // the tabfocus bit field applies to xul elements.
  static bool sTabFocusModelAppliesToXUL;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIContent, NS_ICONTENT_IID)

inline nsIContent* nsINode::AsContent()
{
  MOZ_ASSERT(IsContent());
  return static_cast<nsIContent*>(this);
}

#define NS_IMPL_FROMCONTENT_HELPER(_class, _check)                             \
  static _class* FromContent(nsIContent* aContent)                             \
  {                                                                            \
    return aContent->_check ? static_cast<_class*>(aContent) : nullptr;        \
  }                                                                            \
  static const _class* FromContent(const nsIContent* aContent)                 \
  {                                                                            \
    return aContent->_check ? static_cast<const _class*>(aContent) : nullptr;  \
  }                                                                            \
  static _class* FromContentOrNull(nsIContent* aContent)                       \
  {                                                                            \
    return aContent ? FromContent(aContent) : nullptr;                         \
  }                                                                            \
  static const _class* FromContentOrNull(const nsIContent* aContent)           \
  {                                                                            \
    return aContent ? FromContent(aContent) : nullptr;                         \
  }

#define NS_IMPL_FROMCONTENT(_class, _nsid)                                     \
  NS_IMPL_FROMCONTENT_HELPER(_class, IsInNamespace(_nsid))

#define NS_IMPL_FROMCONTENT_WITH_TAG(_class, _nsid, _tag)                      \
  NS_IMPL_FROMCONTENT_HELPER(_class, NodeInfo()->Equals(nsGkAtoms::_tag, _nsid))

#define NS_IMPL_FROMCONTENT_HTML_WITH_TAG(_class, _tag)                        \
  NS_IMPL_FROMCONTENT_WITH_TAG(_class, kNameSpaceID_XHTML, _tag)

#endif /* nsIContent_h___ */
