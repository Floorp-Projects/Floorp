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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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
#ifndef nsIContent_h___
#define nsIContent_h___

#include <stdio.h>
#include "nsISupports.h"
#include "nsEvent.h"
#include "nsAString.h"
#include "nsContentErrors.h"

// Forward declarations
class nsIAtom;
class nsIDocument;
class nsIPresContext;
class nsVoidArray;
class nsIDOMEvent;
class nsIContent;
class nsISupportsArray;
class nsIDOMRange;
class nsISizeOfHandler;
class nsINodeInfo;
class nsIEventListenerManager;

// IID for the nsIContent interface
#define NS_ICONTENT_IID       \
{ 0x78030220, 0x9447, 0x11d1, \
  {0x93, 0x23, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32} }

/**
 * A node of content in a document's content model. This interface
 * is supported by all content objects.
 */
class nsIContent : public nsISupports {
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ICONTENT_IID)

  /**
   * Get the document for this content.
   * @param aResult the document [OUT]
   */
  NS_IMETHOD GetDocument(nsIDocument*& aResult) const = 0;

  /**
   * Set the document for this content.
   *
   * @param aDocument the new document to set (could be null)
   * @param aDeep whether to set the document on children
   * @param aCompileEventHandlers whether to initialize the event handlers in
   *        the document (used by nsXULElement)
   */
  NS_IMETHOD SetDocument(nsIDocument* aDocument, PRBool aDeep, PRBool aCompileEventHandlers) = 0;

  /**
   * Get the parent content for this content.
   * @param aResult the parent, or null if no parent [OUT]
   */
  NS_IMETHOD GetParent(nsIContent*& aResult) const = 0;

  /**
   * Set the parent content for this content.  (This does not add the child to
   * its parent's child list.)
   * @param aParent the new parent content to set (could be null)
   */
  NS_IMETHOD SetParent(nsIContent* aParent) = 0;

  /**
   * Get whether this content is C++-generated anonymous content
   * @see nsIAnonymousContentCreator
   * @return whether this content is anonymous
   */
  NS_IMETHOD_(PRBool) IsNativeAnonymous() const = 0;

  /**
   * Set whether this content is anonymous
   * @see nsIAnonymousContentCreator
   * @param aAnonymous whether this content is anonymous
   */
  NS_IMETHOD_(void) SetNativeAnonymous(PRBool aAnonymous) = 0;

  /**
   * Get the namespace that this element's tag is defined in
   * @param aResult the namespace [OUT]
   */
  NS_IMETHOD GetNameSpaceID(PRInt32& aResult) const = 0;

  /**
   * Get the tag for this element
   * @param aResult the tag [OUT]
   */
  NS_IMETHOD GetTag(nsIAtom*& aResult) const = 0;

  /**
   * Get the NodeInfo for this element
   * @param aResult the tag [OUT]
   */
  NS_IMETHOD GetNodeInfo(nsINodeInfo*& aResult) const = 0;

  /**
   * Tell whether this element can contain children
   * @param aResult whether this element can contain children [OUT]
   */
  NS_IMETHOD CanContainChildren(PRBool& aResult) const = 0;

  /**
   * Get the number of children
   * @param aResult the number of children [OUT]
   */
  NS_IMETHOD ChildCount(PRInt32& aResult) const = 0;

  /**
   * Get a child by index
   * @param aIndex the index of the child to get, or null if index out of bounds
   * @param aResult the child [OUT]
   */
  NS_IMETHOD ChildAt(PRInt32 aIndex, nsIContent*& aResult) const = 0;

  /**
   * Get the index of a child within this content
   * @param aPossibleChild the child to get the index
   * @param aIndex the index of the child, or -1 if not a child [OUT]
   */
  NS_IMETHOD IndexOf(nsIContent* aPossibleChild, PRInt32& aIndex) const = 0;

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
  NS_IMETHOD InsertChildAt(nsIContent* aKid, PRInt32 aIndex,
                           PRBool aNotify, PRBool aDeepSetDocument) = 0;

  /**
   * Remove a child and replace it with another.
   *
   * @param aKid the content to replace with
   * @param aIndex the index of the content to replace
   * @param aNotify whether to notify the document that the replace has
   *        occurred
   * @param aDeepSetDocument whether to set document on all children of aKid
   */
  NS_IMETHOD ReplaceChildAt(nsIContent* aKid, PRInt32 aIndex,
                            PRBool aNotify, PRBool aDeepSetDocument) = 0;

  /**
   * Append a content node to the end of the child list.
   *
   * @param aKid the content to append
   * @param aNotify whether to notify the document that the replace has
   *        occurred
   * @param aDeepSetDocument whether to set document on all children of aKid
   */
  NS_IMETHOD AppendChildTo(nsIContent* aKid, PRBool aNotify,
                           PRBool aDeepSetDocument) = 0;

  /**
   * Remove a child from this content node.
   *
   * @param aIndex the index of the child to remove
   * @param aNotify whether to notify the document that the replace has
   *        occurred
   */
  NS_IMETHOD RemoveChildAt(PRInt32 aIndex, PRBool aNotify) = 0;

  /**
   * Normalizes an attribute string into an atom that represents the
   * qualified attribute name of the attribute. This method is intended
   * for character case conversion if the content object is case
   * insensitive (e.g. HTML).
   *
   * @param aStr the unparsed attribute string
   * @param aName out parameter representing the complete name of the
   *        attribute
   */
  NS_IMETHOD NormalizeAttrString(const nsAString& aStr, 
                                 nsINodeInfo*& aNodeInfo) = 0;

  /**
   * Set attribute values. All attribute values are assumed to have a
   * canonical String representation that can be used for these
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
  NS_IMETHOD SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                     const nsAString& aValue,
                     PRBool aNotify) = 0;

  /**
   * Set attribute values. All attribute values are assumed to have a
   * canonical string representation that can be used for these
   * methods. The SetAttr method is assumed to perform a translation
   * of the canonical form into the underlying content specific
   * form.
   *
   * @param aNodeInfo the node info (name, prefix, namespace id) of the
   *        attribute
   * @param aValue the value to set
   * @param aNotify specifies whether or not the document should be
   *        notified of the attribute change.
   */
  NS_IMETHOD SetAttr(nsINodeInfo* aNodeInfo,
                     const nsAString& aValue,
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
  NS_IMETHOD GetAttr(PRInt32 aNameSpaceID, nsIAtom* aName, 
                     nsAString& aResult) const = 0;

  /**
   * Get the current value and prefix of the attribute. This returns a form
   * that is suitable for passing back into SetAttr.
   *
   * @param aNameSpaceID the name space of the attr to get
   * @param aName the name of the attr
   * @param aPrefix the prefix of the attr [OUT]
   * @param aName the name of the attr [OUT]
   * @throws NS_CONTENT_ATTR_NOT_THERE if the attribute is not set and has no
   *         default value
   * @throws NS_CONTENT_ATTR_NO_VALUE if the attribute exists but has no value
   * @throws NS_CONTENT_ATTR_HAS_VALUE if the attribute exists and has a
   *         non-empty value (==NS_OK)
   */

  NS_IMETHOD GetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                     nsIAtom*& aPrefix, nsAString& aResult) const = 0;

  /**
   * Determine if an attribute has been set (empty string or otherwise).
   *
   * @param aNameSpaceId the namespace id of the attribute
   * @param aAttr the attribute name
   * @return whether an attribute exists
   */

  NS_IMETHOD_(PRBool) HasAttr(PRInt32 aNameSpaceID, nsIAtom* aName) const = 0;

  /**
   * Remove an attribute so that it is no longer explicitly specified.
   *
   * @param aNameSpaceID the namespace id of the attribute
   * @param aAttr the name of the attribute to unset
   * @param aNotify specifies whether or not the document should be
   * notified of the attribute change
   */
  NS_IMETHOD UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aAttr, 
                       PRBool aNotify) = 0;


  /**
   * Get the namespace / name / prefix of a given attribute.
   * 
   * @param aIndex the index of the attribute name
   * @param aNameSpace the name space ID of the attribute name [OUT]
   * @param aName the attribute name [OUT]
   * @param aPrefix the attribute prefix [OUt]
   *
   */
  NS_IMETHOD GetAttrNameAt(PRInt32 aIndex,
                           PRInt32& aNameSpaceID, 
                           nsIAtom*& aName,
                           nsIAtom*& aPrefix) const = 0;

  /**
   * Get the number of all specified attributes.
   *
   * @param aCountResult the number of attributes [OUT]
   */
  NS_IMETHOD GetAttrCount(PRInt32& aCountResult) const = 0;

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
  NS_IMETHOD RangeAdd(nsIDOMRange* aRange) = 0;
  /**
   * Inform content that it no longer owns either range endpoint
   * @param aRange the range the content no longer owns
   */
  NS_IMETHOD RangeRemove(nsIDOMRange* aRange) = 0;
  /**
   * Get the list of ranges that have either endpoint in this content item
   * @param aResult the list of ranges owned partially by this content [OUT]
   */
  NS_IMETHOD GetRangeList(nsVoidArray*& aResult) const = 0;
  
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
  NS_IMETHOD HandleDOMEvent(nsIPresContext* aPresContext,
                            nsEvent* aEvent,
                            nsIDOMEvent** aDOMEvent,
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
   * @param aID the unique ID for this content [OUT]
   *
   * These methods are DEPRECATED, DON'T USE THEM!!!
   *
   */
  NS_IMETHOD GetContentID(PRUint32* aID) = 0;
  /**
   * Set the unique content ID for this content.
   * @param aID the ID to set
   */
  NS_IMETHOD SetContentID(PRUint32 aID) = 0;

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
  NS_IMETHOD SetFocus(nsIPresContext* aPresContext) = 0;
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
  NS_IMETHOD RemoveFocus(nsIPresContext* aPresContext) = 0;

  /**
   * Sets content node with the binding responsible for our construction (and
   * existence).  Used by anonymous content (XBL-generated). null for all
   * explicit content.
   *
   * @param aContent the new binding parent
   */
  NS_IMETHOD SetBindingParent(nsIContent* aContent) = 0;
  /**
   * Gets content node with the binding responsible for our construction (and
   * existence).  Used by anonymous content (XBL-generated). null for all
   * explicit content.
   *
   * @param aContent the binding parent [OUT]
   */
  NS_IMETHOD GetBindingParent(nsIContent** aContent) = 0;

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
    eXUL                 = 0x00000010
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
  NS_IMETHOD_(PRBool) IsContentOfType(PRUint32 aFlags) = 0;

  /**
   * Get the event listener manager, the guy you talk to to register for events
   * on this element.
   * @param aResult the event listener manager [OUT]
   */
  NS_IMETHOD GetListenerManager(nsIEventListenerManager** aResult) = 0;

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
   * element (through createElement() or cloneNode() generally)   * aFromParser to the NS_NewXXX() constructor for your element and have the
   * parser pass true.  See nsHTMLInputElement.cpp and
   * nsHTMLContentSink::MakeContentObject().
   *
   * DO NOT USE THIS METHOD to get around the fact that it's hard to deal with
   * attributes dynamically.  If you make attributes affect your element from
   * this method, it will only happen on initialization and JavaScript will not
   * be able to create elements (which requires them to first create the
   * element and then call setAttribute() directly, at which point
   * DoneCreatingElement() has already been called and is out of the picture).
   */
  NS_IMETHOD DoneCreatingElement() = 0;

#ifdef DEBUG
  /**
   * Get the size of the content object. The size value should include
   * all subordinate data referenced by the content that is not
   * accounted for by child content. However, this value should not
   * include the frame objects, style contexts, views or other data
   * that lies logically outside the content model.
   *
   * If the implementation so chooses, instead of returning the total
   * subordinate data it may instead use the sizeof handler to store
   * away subordinate data under its own key so that the subordinate
   * data may be tabulated independently of the frame itself.
   *
   * The caller is responsible for recursing over all children that
   * the content contains.
   */
  NS_IMETHOD  SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const = 0;

  /**
   * List the content (and anything it contains) out to the given
   * file stream. Use aIndent as the base indent during formatting.
   * Returns NS_OK unless a file error occurs.
   */
  NS_IMETHOD List(FILE* out = stdout, PRInt32 aIndent = 0) const = 0;

  /**
   * Dump the content (and anything it contains) out to the given
   * file stream. Use aIndent as the base indent during formatting.
   * Returns NS_OK unless a file error occurs.
   */
  NS_IMETHOD DumpContent(FILE* out = stdout, PRInt32 aIndent = 0,PRBool aDumpAll=PR_TRUE) const = 0;
#endif
};

#endif /* nsIContent_h___ */
