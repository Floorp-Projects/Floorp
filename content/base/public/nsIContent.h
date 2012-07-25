/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsIContent_h___
#define nsIContent_h___

#include "nsCOMPtr.h" // for already_AddRefed
#include "nsStringGlue.h"
#include "nsCaseTreatment.h"
#include "nsChangeHint.h"
#include "nsINode.h"
#include "nsIDocument.h" // for IsInHTMLDocument
#include "nsCSSProperty.h"

// Forward declarations
class nsIAtom;
class nsIDOMEvent;
class nsIContent;
class nsEventListenerManager;
class nsIURI;
class nsRuleWalker;
class nsAttrValue;
class nsAttrName;
class nsTextFragment;
class nsIDocShell;
class nsIFrame;
class nsISMILAttr;
class nsIDOMCSSStyleDeclaration;

namespace mozilla {
namespace css {
class StyleRule;
} // namespace css
namespace widget {
struct IMEState;
} // namespace widget
} // namespace mozilla

enum nsLinkState {
  eLinkState_Unknown    = 0,
  eLinkState_Unvisited  = 1,
  eLinkState_Visited    = 2,
  eLinkState_NotLink    = 3
};

// IID for the nsIContent interface
#define NS_ICONTENT_IID \
{ 0x98fb308d, 0xc6dd, 0x4c6d, \
  { 0xb7, 0x7c, 0x91, 0x18, 0x0c, 0xf0, 0x6f, 0x23 } }

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

  nsIContent(already_AddRefed<nsINodeInfo> aNodeInfo)
    : nsINode(aNodeInfo)
  {
    NS_ASSERTION(mNodeInfo,
                 "No nsINodeInfo passed to nsIContent, PREPARE TO CRASH!!!");
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
   * @param aDocument The new document for the content node.  Must match the
   *                  current document of aParent, if aParent is not null.
   *                  May not be null if aParent is null.
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
  
  /**
   * DEPRECATED - Use GetCurrentDoc or GetOwnerDoc.
   * Get the document for this content.
   * @return the document
   */
  nsIDocument *GetDocument() const
  {
    return GetCurrentDoc();
  }

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
    eSkipPlaceholderContent = 2
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
  virtual already_AddRefed<nsINodeList> GetChildren(PRUint32 aFilter) = 0;

  /**
   * Get whether this content is C++-generated anonymous content
   * @see nsIAnonymousContentCreator
   * @return whether this content is anonymous
   */
  bool IsRootOfNativeAnonymousSubtree() const
  {
    NS_ASSERTION(!HasFlag(NODE_IS_NATIVE_ANONYMOUS_ROOT) ||
                 (HasFlag(NODE_IS_ANONYMOUS) &&
                  HasFlag(NODE_IS_IN_ANONYMOUS_SUBTREE)),
                 "Some flags seem to be missing!");
    return HasFlag(NODE_IS_NATIVE_ANONYMOUS_ROOT);
  }

  /**
   * Makes this content anonymous
   * @see nsIAnonymousContentCreator
   */
  void SetNativeAnonymous()
  {
    SetFlags(NODE_IS_ANONYMOUS | NODE_IS_IN_ANONYMOUS_SUBTREE |
             NODE_IS_NATIVE_ANONYMOUS_ROOT);
  }

  /**
   * Returns |this| if it is not native anonymous, otherwise
   * first non native anonymous ancestor.
   */
  virtual nsIContent* FindFirstNonNativeAnonymous() const;

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
                  HasFlag(NODE_IS_ANONYMOUS)) ||
                 // Unfortunately default content for XBL insertion points is
                 // anonymous content that is bound with the parent of the
                 // insertion point as the parent but the bound element for the
                 // binding as the binding parent.  So we have to complicate
                 // the assert a bit here.
                 (GetBindingParent() &&
                  (GetBindingParent() == GetParent()->GetBindingParent()) ==
                  HasFlag(NODE_IS_ANONYMOUS)),
                 "For nodes with parent, flag and GetBindingParent() check "
                 "should match");
    return HasFlag(NODE_IS_ANONYMOUS);
  }

  /**
   * Returns true if there is NOT a path through child lists
   * from the top of this node's parent chain back to this node or
   * if the node is in native anonymous subtree without a parent.
   */
  bool IsInAnonymousSubtree() const
  {
    NS_ASSERTION(!IsInNativeAnonymousSubtree() || GetBindingParent() || !GetParent(),
                 "must have binding parent when in native anonymous subtree with a parent node");
    return IsInNativeAnonymousSubtree() || GetBindingParent() != nsnull;
  }

  /**
   * Return true iff this node is in an HTML document (in the HTML5 sense of
   * the term, i.e. not in an XHTML/XML document).
   */
  inline bool IsInHTMLDocument() const
  {
    return OwnerDoc()->IsHTML();
  }

  /**
   * Get the namespace that this element's tag is defined in
   * @return the namespace
   */
  inline PRInt32 GetNameSpaceID() const
  {
    return mNodeInfo->NamespaceID();
  }

  /**
   * Get the NodeInfo for this element
   * @return the nodes node info
   */
  inline nsINodeInfo* NodeInfo() const
  {
    return mNodeInfo;
  }

  inline bool IsInNamespace(PRInt32 aNamespace) const
  {
    return mNodeInfo->NamespaceID() == aNamespace;
  }

  inline bool IsHTML() const
  {
    return IsInNamespace(kNameSpaceID_XHTML);
  }

  inline bool IsHTML(nsIAtom* aTag) const
  {
    return mNodeInfo->Equals(aTag, kNameSpaceID_XHTML);
  }

  inline bool IsSVG() const
  {
    return IsInNamespace(kNameSpaceID_SVG);
  }

  inline bool IsSVG(nsIAtom* aTag) const
  {
    return mNodeInfo->Equals(aTag, kNameSpaceID_SVG);
  }

  inline bool IsXUL() const
  {
    return IsInNamespace(kNameSpaceID_XUL);
  }

  inline bool IsMathML() const
  {
    return IsInNamespace(kNameSpaceID_MathML);
  }

  inline bool IsMathML(nsIAtom* aTag) const
  {
    return mNodeInfo->Equals(aTag, kNameSpaceID_MathML);
  }

  /**
   * Returns an atom holding the name of the attribute of type ID on
   * this content node (if applicable).  Returns null for non-element
   * content nodes.
   */
  virtual nsIAtom *GetIDAttributeName() const = 0;

  /**
   * Normalizes an attribute name and returns it as a nodeinfo if an attribute
   * with that name exists. This method is intended for character case
   * conversion if the content object is case insensitive (e.g. HTML). Returns
   * the nodeinfo of the attribute with the specified name if one exists or
   * null otherwise.
   *
   * @param aStr the unparsed attribute string
   * @return the node info. May be nsnull.
   */
  virtual already_AddRefed<nsINodeInfo> GetExistingAttrNameFromQName(const nsAString& aStr) const = 0;

  /**
   * Set attribute values. All attribute values are assumed to have a
   * canonical string representation that can be used for these
   * methods. The SetAttr method is assumed to perform a translation
   * of the canonical form into the underlying content specific
   * form.
   *
   * @param aNameSpaceID the namespace of the attribute
   * @param aName the name of the attribute
   * @param aValue the value to set
   * @param aNotify specifies how whether or not the document should be
   *        notified of the attribute change.
   */
  nsresult SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                   const nsAString& aValue, bool aNotify)
  {
    return SetAttr(aNameSpaceID, aName, nsnull, aValue, aNotify);
  }

  /**
   * Set attribute values. All attribute values are assumed to have a
   * canonical String representation that can be used for these
   * methods. The SetAttr method is assumed to perform a translation
   * of the canonical form into the underlying content specific
   * form.
   *
   * @param aNameSpaceID the namespace of the attribute
   * @param aName the name of the attribute
   * @param aPrefix the prefix of the attribute
   * @param aValue the value to set
   * @param aNotify specifies how whether or not the document should be
   *        notified of the attribute change.
   */
  virtual nsresult SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                           nsIAtom* aPrefix, const nsAString& aValue,
                           bool aNotify) = 0;

  /**
   * Get the current value of the attribute. This returns a form that is
   * suitable for passing back into SetAttr.
   *
   * @param aNameSpaceID the namespace of the attr
   * @param aName the name of the attr
   * @param aResult the value (may legitimately be the empty string) [OUT]
   * @returns true if the attribute was set (even when set to empty string)
   *          false when not set.
   */
  virtual bool GetAttr(PRInt32 aNameSpaceID, nsIAtom* aName, 
                         nsAString& aResult) const = 0;

  /**
   * Determine if an attribute has been set (empty string or otherwise).
   *
   * @param aNameSpaceId the namespace id of the attribute
   * @param aAttr the attribute name
   * @return whether an attribute exists
   */
  virtual bool HasAttr(PRInt32 aNameSpaceID, nsIAtom* aName) const = 0;

  /**
   * Test whether this content node's given attribute has the given value.  If
   * the attribute is not set at all, this will return false.
   *
   * @param aNameSpaceID The namespace ID of the attribute.  Must not
   *                     be kNameSpaceID_Unknown.
   * @param aName The name atom of the attribute.  Must not be null.
   * @param aValue The value to compare to.
   * @param aCaseSensitive Whether to do a case-sensitive compare on the value.
   */
  virtual bool AttrValueIs(PRInt32 aNameSpaceID,
                             nsIAtom* aName,
                             const nsAString& aValue,
                             nsCaseTreatment aCaseSensitive) const
  {
    return false;
  }
  
  /**
   * Test whether this content node's given attribute has the given value.  If
   * the attribute is not set at all, this will return false.
   *
   * @param aNameSpaceID The namespace ID of the attribute.  Must not
   *                     be kNameSpaceID_Unknown.
   * @param aName The name atom of the attribute.  Must not be null.
   * @param aValue The value to compare to.  Must not be null.
   * @param aCaseSensitive Whether to do a case-sensitive compare on the value.
   */
  virtual bool AttrValueIs(PRInt32 aNameSpaceID,
                             nsIAtom* aName,
                             nsIAtom* aValue,
                             nsCaseTreatment aCaseSensitive) const
  {
    return false;
  }
  
  enum {
    ATTR_MISSING = -1,
    ATTR_VALUE_NO_MATCH = -2
  };
  /**
   * Check whether this content node's given attribute has one of a given
   * list of values. If there is a match, we return the index in the list
   * of the first matching value. If there was no attribute at all, then
   * we return ATTR_MISSING. If there was an attribute but it didn't
   * match, we return ATTR_VALUE_NO_MATCH. A non-negative result always
   * indicates a match.
   * 
   * @param aNameSpaceID The namespace ID of the attribute.  Must not
   *                     be kNameSpaceID_Unknown.
   * @param aName The name atom of the attribute.  Must not be null.
   * @param aValues a NULL-terminated array of pointers to atom values to test
   *                against.
   * @param aCaseSensitive Whether to do a case-sensitive compare on the values.
   * @return ATTR_MISSING, ATTR_VALUE_NO_MATCH or the non-negative index
   * indicating the first value of aValues that matched
   */
  typedef nsIAtom* const* const AttrValuesArray;
  virtual PRInt32 FindAttrValueIn(PRInt32 aNameSpaceID,
                                  nsIAtom* aName,
                                  AttrValuesArray* aValues,
                                  nsCaseTreatment aCaseSensitive) const
  {
    return ATTR_MISSING;
  }

  /**
   * Remove an attribute so that it is no longer explicitly specified.
   *
   * @param aNameSpaceID the namespace id of the attribute
   * @param aAttr the name of the attribute to unset
   * @param aNotify specifies whether or not the document should be
   * notified of the attribute change
   */
  virtual nsresult UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aAttr, 
                             bool aNotify) = 0;


  /**
   * Get the namespace / name / prefix of a given attribute.
   * 
   * @param   aIndex the index of the attribute name
   * @returns The name at the given index, or null if the index is
   *          out-of-bounds.
   * @note    The document returned by NodeInfo()->GetDocument() (if one is
   *          present) is *not* necessarily the owner document of the element.
   * @note    The pointer returned by this function is only valid until the
   *          next call of either GetAttrNameAt or SetAttr on the element.
   */
  virtual const nsAttrName* GetAttrNameAt(PRUint32 aIndex) const = 0;

  /**
   * Get the number of all specified attributes.
   *
   * @return the number of attributes
   */
  virtual PRUint32 GetAttrCount() const = 0;

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
  virtual PRUint32 TextLength() const = 0;

  /**
   * Set the text to the given value. If aNotify is true then
   * the document is notified of the content change.
   * NOTE: For elements this always ASSERTS and returns NS_ERROR_FAILURE
   */
  virtual nsresult SetText(const PRUnichar* aBuffer, PRUint32 aLength,
                           bool aNotify) = 0;

  /**
   * Append the given value to the current text. If aNotify is true then
   * the document is notified of the content change.
   * NOTE: For elements this always ASSERTS and returns NS_ERROR_FAILURE
   */
  virtual nsresult AppendText(const PRUnichar* aBuffer, PRUint32 aLength,
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
   * Append the text content to aResult.
   * NOTE: This asserts and returns for elements
   */
  virtual void AppendTextTo(nsAString& aResult) = 0;

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
  virtual bool IsFocusable(PRInt32 *aTabIndex = nsnull, bool aWithMouse = false)
  {
    if (aTabIndex) 
      *aTabIndex = -1; // Default, not tabbable
    return false;
  }

  /**
   * The method focuses (or activates) element that accesskey is bound to. It is
   * called when accesskey is activated.
   *
   * @param aKeyCausesActivation - if true then element should be activated
   * @param aIsTrustedEvent - if true then event that is cause of accesskey
   *                          execution is trusted.
   */
  virtual void PerformAccesskey(bool aKeyCausesActivation,
                                bool aIsTrustedEvent)
  {
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
  virtual nsIContent *GetBindingParent() const = 0;

  /**
   * Returns the content node that is the parent of this node in the flattened
   * tree.
   *
   * @return the flattened tree parent
   */
  nsIContent *GetFlattenedTreeParent() const;

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
   * Get the cached state of the link.  If the state is unknown, 
   * return eLinkState_Unknown.
   *
   * @return The cached link state of the link.
   */
  virtual nsLinkState GetLinkState() const
  {
    return eLinkState_NotLink;
  }

  /**
    * Get a pointer to the full href URI (fully resolved and canonicalized,
    * since it's an nsIURI object) for link elements.
    *
    * @return A pointer to the URI or null if the element is not a link or it
    *         has no HREF attribute.
    */
  virtual already_AddRefed<nsIURI> GetHrefURI() const
  {
    return nsnull;
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
   * PRUint32 aFromParser to the NS_NewXXX() constructor for your element and
   * have the parser pass the appropriate flags. See nsHTMLInputElement.cpp and
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
   * NOTE: this is currently only called for textarea, select, applet, and
   * object elements in the HTML content sink.  If you want
   * to call it on your element, modify the content sink of your
   * choice to do so.  This is an efficiency measure.
   *
   * If you also need to determine whether the parser is the one creating your
   * element (through createElement() or cloneNode() generally) then add a
   * boolean aFromParser to the NS_NewXXX() constructor for your element and
   * have the parser pass true.  See nsHTMLInputElement.cpp and
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
   * For HTML textarea, select, applet, and object elements, returns
   * true if all children have been added OR if the element was not
   * created by the parser. Returns true for all other elements.
   * @returns false if the element was created by the parser and
   *                   it is an HTML textarea, select, applet, or object
   *                   element and not all children have been added.
   * @returns true otherwise.
   */
  virtual bool IsDoneAddingChildren()
  {
    return true;
  }

  /**
   * Get the ID of this content node (the atom corresponding to the
   * value of the null-namespace attribute whose name is given by
   * GetIDAttributeName().  This may be null if there is no ID.
   */
  nsIAtom* GetID() const {
    if (HasID()) {
      return DoGetID();
    }
    return nsnull;
  }

  /**
   * Get the class list of this content node (this corresponds to the
   * value of the null-namespace attribute whose name is given by
   * GetClassAttributeName()).  This may be null if there are no
   * classes, but that's not guaranteed.
   */
  const nsAttrValue* GetClasses() const {
    if (HasFlag(NODE_MAY_HAVE_CLASS)) {
      return DoGetClasses();
    }
    return nsnull;
  }

  /**
   * Walk aRuleWalker over the content style rules (presentational
   * hint rules) for this content node.
   */
  NS_IMETHOD WalkContentStyleRules(nsRuleWalker* aRuleWalker) = 0;

  /**
   * Is the attribute named stored in the mapped attributes?
   *
   * // XXXbz we use this method in HasAttributeDependentStyle, so svg
   *    returns true here even though it stores nothing in the mapped
   *    attributes.
   */
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const = 0;

  /**
   * Get a hint that tells the style system what to do when 
   * an attribute on this node changes, if something needs to happen
   * in response to the change *other* than the result of what is
   * mapped into style data via any type of style rule.
   */
  virtual nsChangeHint GetAttributeChangeHint(const nsIAtom* aAttribute,
                                              PRInt32 aModType) const = 0;

  /**
   * Returns an atom holding the name of the "class" attribute on this
   * content node (if applicable).  Returns null if there is no
   * "class" attribute for this type of content node.
   */
  virtual nsIAtom *GetClassAttributeName() const = 0;

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
  virtual void DestroyContent() = 0;

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
    return IsInDoc() ? mPrimaryFrame : nsnull;
  }
  void SetPrimaryFrame(nsIFrame* aFrame) {
    NS_ASSERTION(IsInDoc(), "This will end badly!");
    NS_PRECONDITION(!aFrame || !mPrimaryFrame || aFrame == mPrimaryFrame,
                    "Losing track of existing primary frame");
    mPrimaryFrame = aFrame;
  }

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

  /**
   * Determing language. Look at the nearest ancestor element that has a lang
   * attribute in the XML namespace or is an HTML/SVG element and has a lang in
   * no namespace attribute.
   */
  void GetLang(nsAString& aResult) const {
    for (const nsIContent* content = this; content; content = content->GetParent()) {
      if (content->GetAttrCount() > 0) {
        // xml:lang has precedence over lang on HTML elements (see
        // XHTML1 section C.7).
        bool hasAttr = content->GetAttr(kNameSpaceID_XML, nsGkAtoms::lang,
                                          aResult);
        if (!hasAttr && (content->IsHTML() || content->IsSVG())) {
          hasAttr = content->GetAttr(kNameSpaceID_None, nsGkAtoms::lang,
                                     aResult);
        }
        NS_ASSERTION(hasAttr || aResult.IsEmpty(),
                     "GetAttr that returns false should not make string non-empty");
        if (hasAttr) {
          return;
        }
      }
    }
  }

  // Overloaded from nsINode
  virtual already_AddRefed<nsIURI> GetBaseURI() const;

  virtual nsresult PreHandleEvent(nsEventChainPreVisitor& aVisitor);

  virtual bool IsPurple() = 0;
  virtual void RemovePurple() = 0;

  virtual bool OwnedOnlyByTheDOMTree() { return false; }
protected:
  /**
   * Hook for implementing GetID.  This is guaranteed to only be
   * called if HasID() is true.
   */
  virtual nsIAtom* DoGetID() const = 0;

private:
  /**
   * Hook for implementing GetClasses.  This is guaranteed to only be
   * called if the NODE_MAY_HAVE_CLASS flag is set.
   */
  virtual const nsAttrValue* DoGetClasses() const = 0;

public:
#ifdef DEBUG
  /**
   * List the content (and anything it contains) out to the given
   * file stream. Use aIndent as the base indent during formatting.
   */
  virtual void List(FILE* out = stdout, PRInt32 aIndent = 0) const = 0;

  /**
   * Dump the content (and anything it contains) out to the given
   * file stream. Use aIndent as the base indent during formatting.
   */
  virtual void DumpContent(FILE* out = stdout, PRInt32 aIndent = 0,
                           bool aDumpAll = true) const = 0;
#endif

  enum ETabFocusType {
  //eTabFocus_textControlsMask = (1<<0),  // unused - textboxes always tabbable
    eTabFocus_formElementsMask = (1<<1),  // non-text form elements
    eTabFocus_linksMask = (1<<2),         // links
    eTabFocus_any = 1 + (1<<1) + (1<<2)   // everything that can be focused
  };

  // Tab focus model bit field:
  static PRInt32 sTabFocusModel;

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

#endif /* nsIContent_h___ */
