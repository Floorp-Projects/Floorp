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
 * The Original Code is Mozilla Communicator client code.
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
#ifndef nsIContent_h___
#define nsIContent_h___

#include <stdio.h>
#include "nsCOMPtr.h"
#include "nsISupports.h"
#include "nsEvent.h"
#include "nsAString.h"
#include "nsContentErrors.h"

// Forward declarations
class nsIAtom;
class nsIDocument;
class nsPresContext;
class nsVoidArray;
class nsIDOMEvent;
class nsIContent;
class nsISupportsArray;
class nsIDOMRange;
class nsINodeInfo;
class nsIEventListenerManager;
class nsIURI;

// IID for the nsIContent interface
#define NS_ICONTENT_IID       \
{ 0x658d21a4, 0xc446, 0x11d8, \
  { 0x84, 0xe1, 0x00, 0x0a, 0x95, 0xdc, 0x23, 0x4c } }

/**
 * A node of content in a document's content model. This interface
 * is supported by all content objects.
 */
class nsIContent : public nsISupports {
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ICONTENT_IID)

  nsIContent()
    : mParentPtrBits(0) { }

  /**
   * DEPRECATED - Use GetCurrentDoc or GetOwnerDoc.
   * Get the document for this content.
   * @return the document
   */
  virtual nsIDocument* GetDocument() const = 0;

  /**
   * Set the document for this content.
   *
   * @param aDocument the new document to set (could be null)
   * @param aDeep whether to set the document on children
   * @param aCompileEventHandlers whether to initialize the event handlers in
   *        the document (used by nsXULElement)
   */
  virtual void SetDocument(nsIDocument* aDocument, PRBool aDeep,
                           PRBool aCompileEventHandlers) = 0;

  /**
   * Returns true if the content has an ancestor that is a document.
   *
   * @return whether this content is in a document tree
   */
  virtual PRBool IsInDoc() const = 0;

  /**
   * Get the document that this content is currently in, if any. This will be
   * null if the content has no ancestor that is a document.
   *
   * @return the current document
   */
  nsIDocument *GetCurrentDoc() const
  {
    // XXX This should become:
    // return IsInDoc() ? GetOwnerDoc() : nsnull;
    return GetDocument();
  }

  /**
   * Get the ownerDocument for this content.
   *
   * @return the ownerDocument
   */
  virtual nsIDocument *GetOwnerDoc() const = 0;

  /**
   * Get the parent content for this content.
   * @return the parent, or null if no parent
   */
  virtual nsIContent* GetParent() const
  {
    return NS_REINTERPRET_CAST(nsIContent *, mParentPtrBits & ~kParentBitMask);
  }

  /**
   * Set the parent content for this content.  (This does not add the child to
   * its parent's child list.)  This clobbers the low 2 bits of the parent
   * pointer, so subclasses which use those bits should override this.
   * @param aParent the new parent content to set (could be null)
   */
  virtual void SetParent(nsIContent* aParent) = 0;

  /**
   * Get whether this content is C++-generated anonymous content
   * @see nsIAnonymousContentCreator
   * @return whether this content is anonymous
   */
  virtual PRBool IsNativeAnonymous() const = 0;

  /**
   * Set whether this content is anonymous
   * @see nsIAnonymousContentCreator
   * @param aAnonymous whether this content is anonymous
   */
  virtual void SetNativeAnonymous(PRBool aAnonymous) = 0;

  /**
   * Get the namespace that this element's tag is defined in
   * @param aResult the namespace [OUT]
   */
  virtual void GetNameSpaceID(PRInt32* aResult) const = 0;

  /**
   * Get the tag for this element. This will always return a non-null
   * atom pointer (as implied by the naming of the method).
   */
  virtual nsIAtom *Tag() const = 0;

  /**
   * Get the NodeInfo for this element
   * @return the nodes node info
   */
  virtual nsINodeInfo * GetNodeInfo() const = 0;

  /**
   * Get the number of children
   * @return the number of children
   */
  virtual PRUint32 GetChildCount() const = 0;

  /**
   * Get a child by index
   * @param aIndex the index of the child to get, or null if index out
   *               of bounds
   * @return the child
   */
  virtual nsIContent *GetChildAt(PRUint32 aIndex) const = 0;

  /**
   * Get the index of a child within this content
   * @param aPossibleChild the child to get the index
   * @return the index of the child, or -1 if not a child
   */
  virtual PRInt32 IndexOf(nsIContent* aPossibleChild) const = 0;

  /**
   * Insert a content node at a particular index.
   *
   * @param aKid the content to insert
   * @param aIndex the index it is being inserted at (the index it will have
   *        after it is inserted)
   * @param aNotify whether to notify the document that the insert has
   *        occurred
   * @param aDeepSetDocument whether to set document on all children of aKid
   */
  virtual nsresult InsertChildAt(nsIContent* aKid, PRUint32 aIndex,
                                 PRBool aNotify, PRBool aDeepSetDocument) = 0;

  /**
   * Append a content node to the end of the child list.
   *
   * @param aKid the content to append
   * @param aNotify whether to notify the document that the replace has
   *        occurred
   * @param aDeepSetDocument whether to set document on all children of aKid
   */
  virtual nsresult AppendChildTo(nsIContent* aKid, PRBool aNotify,
                                 PRBool aDeepSetDocument) = 0;

  /**
   * Remove a child from this content node.
   *
   * @param aIndex the index of the child to remove
   * @param aNotify whether to notify the document that the replace has
   *        occurred
   */
  virtual nsresult RemoveChildAt(PRUint32 aIndex, PRBool aNotify) = 0;

  /**
   * Returns an atom holding the name of the attribute of type ID on
   * this content node (if applicable).  Returns null for non-element
   * content nodes.
   */
  virtual nsIAtom *GetIDAttributeName() const = 0;

  /**
   * Returns an atom holding the name of the "class" attribute on this
   * content node (if applicable).  Returns null for non-element
   * content nodes.
   */
  virtual nsIAtom *GetClassAttributeName() const = 0;

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
                   const nsAString& aValue, PRBool aNotify)
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
                           PRBool aNotify) = 0;

  /**
   * Get the current value of the attribute. This returns a form that is
   * suitable for passing back into SetAttr.
   *
   * @param aNameSpaceID the namespace of the attr
   * @param aName the name of the attr
   * @param aResult the value (may legitimately be the empty string) [OUT]
   * @throws NS_CONTENT_ATTR_NOT_THERE if the attribute is not set and has no
   *         default value
   * @throws NS_CONTENT_ATTR_NO_VALUE if the attribute exists but has no value
   * @throws NS_CONTENT_ATTR_HAS_VALUE if the attribute exists and has a
   *         non-empty value (==NS_OK)
   */
  virtual nsresult GetAttr(PRInt32 aNameSpaceID, nsIAtom* aName, 
                           nsAString& aResult) const = 0;

  /**
   * Determine if an attribute has been set (empty string or otherwise).
   *
   * @param aNameSpaceId the namespace id of the attribute
   * @param aAttr the attribute name
   * @return whether an attribute exists
   */
  virtual PRBool HasAttr(PRInt32 aNameSpaceID, nsIAtom* aName) const = 0;

  /**
   * Remove an attribute so that it is no longer explicitly specified.
   *
   * @param aNameSpaceID the namespace id of the attribute
   * @param aAttr the name of the attribute to unset
   * @param aNotify specifies whether or not the document should be
   * notified of the attribute change
   */
  virtual nsresult UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aAttr, 
                             PRBool aNotify) = 0;


  /**
   * Get the namespace / name / prefix of a given attribute.
   * 
   * @param aIndex the index of the attribute name
   * @param aNameSpace the name space ID of the attribute name [OUT]
   * @param aName the attribute name [OUT]
   * @param aPrefix the attribute prefix [OUT]
   *
   */
  virtual nsresult GetAttrNameAt(PRUint32 aIndex, PRInt32* aNameSpaceID,
                                 nsIAtom** aName, nsIAtom** aPrefix) const = 0;

  /**
   * Get the number of all specified attributes.
   *
   * @return the number of attributes
   */
  virtual PRUint32 GetAttrCount() const = 0;

  /**
   * Inform content of range ownership changes.  This allows content
   * to do the right thing to ranges in the face of changes to the content
   * model.
   * RangeRemove -- informs content that it no longer owns a range endpoint
   * GetRangeList -- returns the list of ranges that have one or both endpoints
   *                 within this content item
   */
  /**
   * Inform content that it owns one or both range endpoints
   * @param aRange the range the content owns
   */
  virtual nsresult RangeAdd(nsIDOMRange* aRange) = 0;

  /**
   * Inform content that it no longer owns either range endpoint
   * @param aRange the range the content no longer owns
   */
  virtual void RangeRemove(nsIDOMRange* aRange) = 0;

  /**
   * Get the list of ranges that have either endpoint in this content
   * item.
   * @return the list of ranges owned partially by this content. The
   * nsVoidArray is owned by the content object and its lifetime is
   * controlled completely by the content object.
   */
  virtual const nsVoidArray *GetRangeList() const = 0;

  /**
   * Handle a DOM event for this piece of content.  This method is responsible
   * for handling and controlling all three stages of events, capture, local
   * and bubble.  It also does strange things to anonymous content which whiz
   * right by this author's head.
   *
   * Here are some beginning explanations:
   * - if in INIT or CAPTURE mode, it must pass the event to its parent in
   *   CAPTURE mode (this happens before the event is fired, therefore the
   *   firing of events will occur from the root up to the target).
   * - The event is fired to listeners.
   * - If in INIT or BUBBLE mode, it passes the event to its parent in BUBBLE
   *   mode.  This means that the events will be fired up the chain starting
   *   from the target to the ancestor.
   *
   * NOTE: if you are extending nsGenericElement and have to do a default
   * action, call super::HandleDOMEvent() first and check for
   * aEventStatus != nsEventStatus_eIgnore and make sure you are not in CAPTURE
   * mode before proceeding.
   *
   * XXX Go comment that method, Will Robinson.
   * @param aPresContext the current presentation context
   * @param aEvent the event that is being propagated
   * @param aDOMEvent a special event that may contain a modified target.  Pass
   *        in null here or aDOMEvent if you are in HandleDOMEvent already;
   *        don't worry your pretty little head about it.
   * @param aFlags flags that describe what mode we are in.  Generally
   *        NS_EVENT_FLAG_CAPTURE, NS_EVENT_FLAG_BUBBLE and NS_EVENT_FLAG_INIT
   *        are the ones that matter.
   * @param aEventStatus the status returned from the function.  Generally
   *        nsEventStatus_eIgnore
   */
  virtual nsresult HandleDOMEvent(nsPresContext* aPresContext,
                                  nsEvent* aEvent, nsIDOMEvent** aDOMEvent,
                                  PRUint32 aFlags,
                                  nsEventStatus* aEventStatus) = 0;

  /**
   * Get a unique ID for this piece of content.
   * This ID is used as a key to store state information 
   * about this content object and its associated frame object.
   * The state information is stored in a dictionary that is
   * manipulated by the frame manager (nsIFrameManager) inside layout.
   * An opaque pointer to this dictionary is passed to the session
   * history as a handle associated with the current document's state
   *
   * These methods are DEPRECATED, DON'T USE THEM!!!
   *
   */
  virtual PRUint32 ContentID() const = 0;
  /**
   * Set the unique content ID for this content.
   * @param aID the ID to set
   */
  virtual void SetContentID(PRUint32 aID)
  {
  }

  /**
   * Set the focus on this content.  This is generally something for the event
   * state manager to do, not ordinary people.  Ordinary people should do
   * something like nsGenericHTMLElement::SetElementFocus().  This method is
   * the end result, the point where the content finds out it has been focused.
   * 
   * All content elements are potentially focusable (according to CSS3).
   *
   * @param aPresContext the pres context
   * @see nsGenericHTMLElement::SetElementFocus()
   */
  virtual void SetFocus(nsPresContext* aPresContext)
  {
  }

  /**
   * Remove the focus on this content.  This is generally something for the
   * event state manager to do, not ordinary people.  Ordinary people should do
   * something like nsGenericHTMLElement::SetElementFocus().  This method is
   * the end result, the point where the content finds out it has been focused.
   * 
   * All content elements are potentially focusable (according to CSS3).
   *
   * @param aPresContext the pres context
   * @see nsGenericHTMLElement::SetElementFocus()
   */
  virtual void RemoveFocus(nsPresContext* aPresContext)
  {
  }

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
   * Also, depending on the pref accessibility.tabfocus some widgets may be 
   * focusable but removed from the tab order. This is the default on
   * Mac OS X, where fewer items are focusable.
   * @param  [inout, optional] aTabIndex the computed tab index
   *         In: default tabindex for element (-1 nonfocusable, == 0 focusable)
   *         Out: computed tabindex
   * @param  [optional] aTabIndex the computed tab index
   *         < 0 if not tabbable
   *         == 0 if in normal tab order
   *         > 0 can be tabbed to in the order specified by this value
   * @return whether the content is focusable via mouse, kbd or script.
   */
  virtual PRBool IsFocusable(PRInt32 *aTabIndex = nsnull)
  {
    if (aTabIndex) 
      *aTabIndex = -1; // Default, not tabbable
    return PR_FALSE;
  }

  /**
   * Sets content node with the binding responsible for our construction (and
   * existence).  Used by anonymous content (XBL-generated). null for all
   * explicit content.
   *
   * @param aContent the new binding parent
   */
  virtual nsresult SetBindingParent(nsIContent* aContent) = 0;

  /**
   * Gets content node with the binding responsible for our construction (and
   * existence).  Used by anonymous content (XBL-generated). null for all
   * explicit content.
   *
   * @return the binding parent
   */
  virtual nsIContent *GetBindingParent() const = 0;

  /**
   * Bit-flags to pass (or'ed together) to IsContentOfType()
   */
  enum {
    /** text elements */
    eTEXT                = 0x00000001,
    /** dom elements */
    eELEMENT             = 0x00000002,
    /** html elements */
    eHTML                = 0x00000004,
    /** form controls */
    eHTML_FORM_CONTROL   = 0x00000008,
    /** XUL elements */
    eXUL                 = 0x00000010,
    /** xml processing instructions */
    ePROCESSING_INSTRUCTION = 0x00000020,
    /** svg elements */
    eSVG                 = 0x00000040
  };

  /**
   * API for doing a quick check if a content object is of a given
   * type, such as HTML, XUL, Text, ...  Use this when you can instead of
   * checking the tag.
   *
   * @param aFlags what types you want to test for (see above, eTEXT, eELEMENT,
   *        eHTML, eHTML_FORM_CONTROL, eXUL)
   * @return whether the content matches ALL flags passed in
   */
  virtual PRBool IsContentOfType(PRUint32 aFlags) const = 0;

  /**
   * Get the event listener manager, the guy you talk to to register for events
   * on this element.
   * @param aResult the event listener manager [OUT]
   */
  virtual nsresult GetListenerManager(nsIEventListenerManager** aResult) = 0;

  /**
   * Get the base URI for any relative URIs within this piece of
   * content. Generally, this is the document's base URI, but certain
   * content carries a local base for backward compatibility.
   *
   * @return the base URI
   */
  virtual already_AddRefed<nsIURI> GetBaseURI() const = 0;

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
   * boolean aFromParser to the NS_NewXXX() constructor for your element and
   * have the parser pass true.  See nsHTMLInputElement.cpp and
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
   */
  virtual void DoneAddingChildren()
  {
  }

  /**
   * For HTML textarea, select, applet, and object elements, returns
   * PR_TRUE if all children have been added OR if the element was not
   * created by the parser. Returns PR_TRUE for all other elements.
   */
  virtual PRBool IsDoneAddingChildren()
  {
    return PR_TRUE;
  }

#ifdef DEBUG
  /**
   * List the content (and anything it contains) out to the given
   * file stream. Use aIndent as the base indent during formatting.
   * Returns NS_OK unless a file error occurs.
   */
  virtual void List(FILE* out = stdout, PRInt32 aIndent = 0) const = 0;

  /**
   * Dump the content (and anything it contains) out to the given
   * file stream. Use aIndent as the base indent during formatting.
   * Returns NS_OK unless a file error occurs.
   */
  virtual void DumpContent(FILE* out = stdout, PRInt32 aIndent = 0,
                           PRBool aDumpAll = PR_TRUE) const = 0;
#endif

  enum ETabFocusType {
  //eTabFocus_textControlsMask = (1<<0),  // unused - textboxes always tabbable
    eTabFocus_formElementsMask = (1<<1),  // non-text form elements
    eTabFocus_linksMask = (1<<2),         // links
    eTabFocus_any = 1 + (1<<1) + (1<<2)   // everything that can be focused
  };

  // Tab focus model bit field:
  static PRInt32 sTabFocusModel;


protected:
  typedef PRWord PtrBits;

  // Subclasses may use the low two bits of mParentPtrBits to store other data
  enum { kParentBitMask = 0x3 };

  PtrBits      mParentPtrBits;
};

#endif /* nsIContent_h___ */
