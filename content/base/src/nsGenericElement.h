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
#ifndef nsGenericElement_h___
#define nsGenericElement_h___

#include "nsCOMPtr.h"
#include "nsIHTMLContent.h"
#include "nsIDOMAttr.h"
#include "nsIDOMNamedNodeMap.h"
#include "nsIDOMElement.h"
#include "nsIDOMDocumentFragment.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMLinkStyle.h"
#include "nsIDOMEventReceiver.h"
#include "nsIDOM3EventTarget.h"
#include "nsIStyleSheetLinkingElement.h"
#include "nsICSSStyleSheet.h"
#include "nsICSSLoaderObserver.h"
#include "nsVoidArray.h"
#include "nsILinkHandler.h"
#include "nsGenericDOMNodeList.h"
#include "nsIEventListenerManager.h"
#include "nsINodeInfo.h"
#include "nsIParser.h"
#include "nsContentUtils.h"
#include "pldhash.h"

class nsIDOMAttr;
class nsIDOMEventListener;
class nsIFrame;
class nsISupportsArray;
class nsDOMCSSDeclaration;
class nsIDOMCSSStyleDeclaration;
class nsDOMAttributeMap;
class nsIURI;
class nsINodeInfo;

typedef unsigned long PtrBits;

/** This bit will be set if the nsGenericElement has nsDOMSlots */
#define GENERIC_ELEMENT_DOESNT_HAVE_DOMSLOTS   0x00000001U

/**
 * This bit will be set if the element has a range list in the range list hash
 */
#define GENERIC_ELEMENT_HAS_RANGELIST          0x00000002U

/**
 * This bit will be set if the element has a listener manager in the listener
 * manager hash
 */
#define GENERIC_ELEMENT_HAS_LISTENERMANAGER    0x00000004U

/** Whether this content is anonymous */
#define GENERIC_ELEMENT_IS_ANONYMOUS           0x00000008U

/** The number of bits to shift the bit field to get at the content ID */
#define GENERIC_ELEMENT_CONTENT_ID_BITS_OFFSET 4

/** This mask masks out the bits that are used for the content ID */
#define GENERIC_ELEMENT_CONTENT_ID_MASK \
  ((~PtrBits(0)) << GENERIC_ELEMENT_CONTENT_ID_BITS_OFFSET)

/**
 * The largest value for content ID that fits in
 * GENERIC_ELEMENT_CONTENT_ID_MASK
 */
#define GENERIC_ELEMENT_CONTENT_ID_MAX_VALUE \
  ((~PtrBits(0)) >> GENERIC_ELEMENT_CONTENT_ID_BITS_OFFSET)


/**
 * Class that implements the nsIDOMNodeList interface (a list of children of
 * the content), by holding a reference to the content and delegating GetLength
 * and Item to its existing child list.
 * @see nsIDOMNodeList
 */
class nsChildContentList : public nsGenericDOMNodeList 
{
public:
  /**
   * @param aContent the content whose children will make up the list
   */
  nsChildContentList(nsIContent *aContent);
  virtual ~nsChildContentList();

  // nsIDOMNodeList interface
  NS_DECL_NSIDOMNODELIST
  
  /** Drop the reference to the content */
  void DropReference();

private:
  /** The content whose children make up the list (weak reference) */
  nsIContent *mContent;
};

/**
 * There are a set of DOM- and scripting-specific instance variables
 * that may only be instantiated when a content object is accessed
 * through the DOM. Rather than burn actual slots in the content
 * objects for each of these instance variables, we put them off
 * in a side structure that's only allocated when the content is
 * accessed through the DOM.
 */
class nsDOMSlots
{
public:
  nsDOMSlots(PtrBits aFlags);
  ~nsDOMSlots();

  PRBool IsEmpty();

  PtrBits mFlags;

  /**
   * An object implementing nsIDOMNodeList for this content (childNodes)
   * @see nsIDOMNodeList
   * @see nsGenericHTMLLeafElement::GetChildNodes
   */
  nsChildContentList *mChildNodes;

  /**
   * The .style attribute (an interface that forwards to the actual
   * style rules)
   * @see nsGenericHTMLElement::GetStyle */
  nsDOMCSSDeclaration *mStyle;

  /**
   * An object implementing nsIDOMNamedNodeMap for this content (attributes)
   * @see nsGenericElement::GetAttributes
   */
  nsDOMAttributeMap* mAttributeMap;

  /**
   * The nearest enclosing content node with a binding that created us.
   * @see nsGenericElement::GetBindingParent
   */
  nsIContent* mBindingParent; // [Weak]

  // DEPRECATED, DON'T USE THIS
  PRUint32 mContentID;
};

class RangeListMapEntry : public PLDHashEntryHdr
{
public:
  RangeListMapEntry(const void *aKey)
    : mKey(aKey), mRangeList(nsnull)
  {
  }

  ~RangeListMapEntry()
  {
    delete mRangeList;
  }

private:
  const void *mKey; // must be first to look like PLDHashEntryStub

public:
  // We want mRangeList to be an nsAutoVoidArray but we can't make an
  // nsAutoVoidArray a direct member of RangeListMapEntry since it
  // will be moved around in memory, and nsAutoVoidArray can't deal
  // with that.
  nsVoidArray *mRangeList;
};

class EventListenerManagerMapEntry : public PLDHashEntryHdr
{
public:
  EventListenerManagerMapEntry(const void *aKey)
    : mKey(aKey)
  {
  }

  ~EventListenerManagerMapEntry()
  {
    if (mListenerManager) {
      mListenerManager->SetListenerTarget(nsnull);
    }
  }

private:
  const void *mKey; // must be first, to look like PLDHashEntryStub

public:
  nsCOMPtr<nsIEventListenerManager> mListenerManager;
};


/**
 * A tearoff class for nsGenericElement to implement the nsIDOM3Node functions
 */
class nsNode3Tearoff : public nsIDOM3Node
{
  NS_DECL_ISUPPORTS

  NS_DECL_NSIDOM3NODE

  nsNode3Tearoff(nsIContent *aContent) : mContent(aContent)
  {
  }
  virtual ~nsNode3Tearoff() {};

private:
  nsCOMPtr<nsIContent> mContent;
};


#define NS_EVENT_TEAROFF_CACHE_SIZE 4

/**
 * nsDOMEventRTTearoff is a tearoff class used by nsGenericElement and
 * nsGenericDOMDataNode classes for implementing the interfaces
 * nsIDOMEventReceiver and nsIDOMEventTarget
 *
 * Use the method nsDOMEventRTTearoff::Create() to create one of these babies.
 * @see nsDOMEventRTTearoff::Create
 */

class nsDOMEventRTTearoff : public nsIDOMEventReceiver,
                            public nsIDOM3EventTarget
{
private:
  // This class uses a caching scheme so we don't let users of this
  // class create new instances with 'new', in stead the callers
  // should use the static method
  // nsDOMEventRTTearoff::Create(). That's why the constructor and
  // destrucor of this class is private.

  nsDOMEventRTTearoff(nsIContent *aContent);

  static nsDOMEventRTTearoff *mCachedEventTearoff[NS_EVENT_TEAROFF_CACHE_SIZE];
  static PRUint32 mCachedEventTearoffCount;

  /**
   * This method gets called by Release() when it's time to delete the
   * this object, in stead of always deleting the object we'll put the
   * object in the cache if unless the cache is already full.
   */
  void LastRelease();

  nsresult GetEventReceiver(nsIDOMEventReceiver **aReceiver);
  nsresult GetDOM3EventTarget(nsIDOM3EventTarget **aTarget);

public:
  virtual ~nsDOMEventRTTearoff();

  /**
   * Use this static method to create instances of nsDOMEventRTTearoff.
   * @param aContent the content to create a tearoff for
   */
  static nsDOMEventRTTearoff *Create(nsIContent *aContent);

  /**
   * Call before shutdown to clear the cache and free memory for this class.
   */
  static void Shutdown();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIDOMEventTarget
  NS_DECL_NSIDOMEVENTTARGET

  // nsIDOM3EventTarget
  NS_DECL_NSIDOM3EVENTTARGET

  // nsIDOMEventReceiver
  NS_IMETHOD AddEventListenerByIID(nsIDOMEventListener *aListener,
                                   const nsIID& aIID);
  NS_IMETHOD RemoveEventListenerByIID(nsIDOMEventListener *aListener,
                                      const nsIID& aIID);
  NS_IMETHOD GetListenerManager(nsIEventListenerManager** aResult);
  NS_IMETHOD HandleEvent(nsIDOMEvent *aEvent);
  NS_IMETHOD GetSystemEventGroup(nsIDOMEventGroup** aGroup);

private:
  /**
   * Strong reference back to the content object from where an instance of this
   * class was 'torn off'
   */
  nsCOMPtr<nsIContent> mContent;
};


/**
 * A generic base class for DOM elements, implementing many nsIContent,
 * nsIDOMNode and nsIDOMElement methods.
 */
class nsGenericElement : public nsIHTMLContent
{
public:
  nsGenericElement();
  virtual ~nsGenericElement();

  NS_DECL_ISUPPORTS

  /**
   * Initialize this element given a NodeInfo object
   * @param aNodeInfo information about this type of node
   */
  nsresult Init(nsINodeInfo *aNodeInfo);

  /**
   * Called during QueryInterface to give the binding manager a chance to
   * get an interface for this element.
   */
  nsresult PostQueryInterface(REFNSIID aIID, void** aInstancePtr);

  /** Free globals, to be called from module destructor */
  static void Shutdown();

  // nsIContent interface methods
  NS_IMETHOD GetDocument(nsIDocument*& aResult) const;
  NS_IMETHOD SetDocument(nsIDocument* aDocument, PRBool aDeep,
                         PRBool aCompileEventHandlers);
  NS_IMETHOD GetParent(nsIContent*& aResult) const;
  NS_IMETHOD SetParent(nsIContent* aParent);
  NS_IMETHOD_(PRBool) IsNativeAnonymous() const;
  NS_IMETHOD_(void) SetNativeAnonymous(PRBool aAnonymous);
  NS_IMETHOD GetNameSpaceID(PRInt32& aNameSpaceID) const;
  NS_IMETHOD GetTag(nsIAtom*& aResult) const;
  NS_IMETHOD GetNodeInfo(nsINodeInfo*& aResult) const;
  // NS_IMETHOD CanContainChildren(PRBool& aResult) const;
  // NS_IMETHOD ChildCount(PRInt32& aResult) const;
  // NS_IMETHOD ChildAt(PRInt32 aIndex, nsIContent*& aResult) const;
  // NS_IMETHOD IndexOf(nsIContent* aPossibleChild, PRInt32& aResult) const;
  // NS_IMETHOD InsertChildAt(nsIContent* aKid, PRInt32 aIndex,
  //                          PRBool aNotify);
  // NS_IMETHOD ReplaceChildAt(nsIContent* aKid, PRInt32 aIndex,
  //                           PRBool aNotify);
  // NS_IMETHOD AppendChildTo(nsIContent* aKid, PRBool aNotify);
  // NS_IMETHOD RemoveChildAt(PRInt32 aIndex, PRBool aNotify);
  // NS_IMETHOD NormalizeAttrString(const nsAString& aStr,
  //                                nsINodeInfo*& aNodeInfo);
  // NS_IMETHOD SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName, 
  //                    const nsAString& aValue,
  //                    PRBool aNotify);
  // NS_IMETHOD SetAttr(nsINodeInfo* aNodeInfo,
  //                    const nsAString& aValue,
  //                    PRBool aNotify);
  // NS_IMETHOD GetAttr(PRInt32 aNameSpaceID, nsIAtom* aName, 
  //                    nsAString& aResult) const;
  // NS_IMETHOD GetAttr(PRInt32 aNameSpaceID, nsIAtom* aName, 
  //                    nsIAtom*& aPrefix,
  //                    nsAString& aResult) const;
  // NS_IMETHOD UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aAttribute, 
  //                      PRBool aNotify);
  // NS_IMETHOD GetAttrNameAt(PRInt32 aIndex,
  //                          PRInt32& aNameSpaceID, 
  //                          nsIAtom*& aName,
  //                          nsIAtom*& aPrefix) const;
  // NS_IMETHOD GetAttrCount(PRInt32& aResult) const;
#ifdef DEBUG
  // NS_IMETHOD List(FILE* out, PRInt32 aIndent) const;
  // NS_IMETHOD DumpContent(FILE* out, PRInt32 aIndent,PRBool aDumpAll) const;
#endif
  NS_IMETHOD RangeAdd(nsIDOMRange* aRange);
  NS_IMETHOD RangeRemove(nsIDOMRange* aRange);
  NS_IMETHOD GetRangeList(nsVoidArray*& aResult) const;
  NS_IMETHOD HandleDOMEvent(nsIPresContext* aPresContext,
                            nsEvent* aEvent,
                            nsIDOMEvent** aDOMEvent,
                            PRUint32 aFlags,
                            nsEventStatus* aEventStatus);
  NS_IMETHOD GetContentID(PRUint32* aID);
  NS_IMETHOD SetContentID(PRUint32 aID);
  NS_IMETHOD SetFocus(nsIPresContext* aContext);
  NS_IMETHOD RemoveFocus(nsIPresContext* aContext);
  NS_IMETHOD GetBindingParent(nsIContent** aContent);
  NS_IMETHOD SetBindingParent(nsIContent* aParent);
  NS_IMETHOD_(PRBool) IsContentOfType(PRUint32 aFlags);
  NS_IMETHOD GetListenerManager(nsIEventListenerManager** aInstancePtrResult);
  NS_IMETHOD DoneCreatingElement();


  // nsIStyledContent interface methods
  NS_IMETHOD GetID(nsIAtom*& aResult) const;
  NS_IMETHOD GetClasses(nsVoidArray& aArray) const;
  NS_IMETHOD_(PRBool) HasClass(nsIAtom* aClass, PRBool aCaseSensitive) const;
  NS_IMETHOD WalkContentStyleRules(nsRuleWalker* aRuleWalker);
  NS_IMETHOD GetInlineStyleRule(nsIStyleRule** aStyleRule);
  NS_IMETHOD GetMappedAttributeImpact(const nsIAtom* aAttribute,
                                      PRInt32 aModType, nsChangeHint& aHint) const;

  // nsIXMLContent interface methods
  NS_IMETHOD MaybeTriggerAutoLink(nsIWebShell *aShell);
  NS_IMETHOD GetXMLBaseURI(nsIURI **aURI);

  // nsIHTMLContent interface methods
  NS_IMETHOD Compact();
  NS_IMETHOD SetHTMLAttribute(nsIAtom* aAttribute,
                              const nsHTMLValue& aValue,
                              PRBool aNotify);
  NS_IMETHOD GetHTMLAttribute(nsIAtom* aAttribute,
                              nsHTMLValue& aValue) const;
  NS_IMETHOD GetAttributeMappingFunction(nsMapRuleToAttributesFunc& aMapRuleFunc) const;
  NS_IMETHOD AttributeToString(nsIAtom* aAttribute,
                               const nsHTMLValue& aValue,
                               nsAString& aResult) const;
  NS_IMETHOD StringToAttribute(nsIAtom* aAttribute,
                               const nsAString& aValue,
                               nsHTMLValue& aResult);
  NS_IMETHOD GetBaseURL(nsIURI*& aBaseURL) const;
  NS_IMETHOD GetBaseTarget(nsAString& aBaseTarget) const;

  // nsIDOMNode method implementation
  NS_IMETHOD GetNodeName(nsAString& aNodeName);
  NS_IMETHOD GetLocalName(nsAString& aLocalName);
  NS_IMETHOD GetNodeValue(nsAString& aNodeValue);
  NS_IMETHOD SetNodeValue(const nsAString& aNodeValue);
  NS_IMETHOD GetNodeType(PRUint16* aNodeType);
  NS_IMETHOD GetParentNode(nsIDOMNode** aParentNode);
  NS_IMETHOD GetAttributes(nsIDOMNamedNodeMap** aAttributes);
  NS_IMETHOD GetPreviousSibling(nsIDOMNode** aPreviousSibling);
  NS_IMETHOD GetNextSibling(nsIDOMNode** aNextSibling);
  NS_IMETHOD GetOwnerDocument(nsIDOMDocument** aOwnerDocument);
  NS_IMETHOD GetNamespaceURI(nsAString& aNamespaceURI);
  NS_IMETHOD GetPrefix(nsAString& aPrefix);
  NS_IMETHOD SetPrefix(const nsAString& aPrefix);
  NS_IMETHOD Normalize();
  NS_IMETHOD IsSupported(const nsAString& aFeature,
                         const nsAString& aVersion, PRBool* aReturn);
  NS_IMETHOD HasAttributes(PRBool* aHasAttributes);

  // nsIDOMElement method implementation
  NS_IMETHOD GetTagName(nsAString& aTagName);
  NS_IMETHOD GetAttribute(const nsAString& aName,
                          nsAString& aReturn);
  NS_IMETHOD SetAttribute(const nsAString& aName,
                          const nsAString& aValue);
  NS_IMETHOD RemoveAttribute(const nsAString& aName);
  NS_IMETHOD GetAttributeNode(const nsAString& aName,
                              nsIDOMAttr** aReturn);
  NS_IMETHOD SetAttributeNode(nsIDOMAttr* aNewAttr, nsIDOMAttr** aReturn);
  NS_IMETHOD RemoveAttributeNode(nsIDOMAttr* aOldAttr, nsIDOMAttr** aReturn);
  NS_IMETHOD GetElementsByTagName(const nsAString& aTagname,
                                  nsIDOMNodeList** aReturn);
  NS_IMETHOD GetAttributeNS(const nsAString& aNamespaceURI,
                            const nsAString& aLocalName,
                            nsAString& aReturn);
  NS_IMETHOD SetAttributeNS(const nsAString& aNamespaceURI,
                            const nsAString& aQualifiedName,
                            const nsAString& aValue);
  NS_IMETHOD RemoveAttributeNS(const nsAString& aNamespaceURI,
                               const nsAString& aLocalName);
  NS_IMETHOD GetAttributeNodeNS(const nsAString& aNamespaceURI,
                                const nsAString& aLocalName,
                                nsIDOMAttr** aReturn);
  NS_IMETHOD SetAttributeNodeNS(nsIDOMAttr* aNewAttr, nsIDOMAttr** aReturn);
  NS_IMETHOD GetElementsByTagNameNS(const nsAString& aNamespaceURI,
                                    const nsAString& aLocalName,
                                    nsIDOMNodeList** aReturn);
  NS_IMETHOD HasAttribute(const nsAString& aName, PRBool* aReturn);
  NS_IMETHOD HasAttributeNS(const nsAString& aNamespaceURI,
                            const nsAString& aLocalName,
                            PRBool* aReturn);

  // Generic DOMNode implementations
  /**
   * Generic implementation of InsertBefore to be called by subclasses
   * @see nsIDOMNode::InsertBefore
   */
  nsresult  doInsertBefore(nsIDOMNode* aNewChild, nsIDOMNode* aRefChild,
                           nsIDOMNode** aReturn);
  /**
   * Generic implementation of ReplaceChild to be called by subclasses
   * @see nsIDOMNode::ReplaceChild
   */
  nsresult  doReplaceChild(nsIDOMNode* aNewChild, nsIDOMNode* aOldChild,
                           nsIDOMNode** aReturn);
  /**
   * Generic implementation of RemoveChild to be called by subclasses
   * @see nsIDOMNode::RemoveChild
   */
  nsresult  doRemoveChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn);

  //----------------------------------------

  /**
   * Add a script event listener with the given attr name (like onclick)
   * and with the value as JS
   * @param aAttribute the event listener name
   * @param aValue the JS to attach
   */
  nsresult AddScriptEventListener(nsIAtom* aAttribute,
                                  const nsAString& aValue);

  /**
   * Load a link, putting together the proper URL from the pieces given.
   * @param aPresContext the pres context.
   * @param aVerb how the link will be loaded (replace page, new window, etc.)
   * @param aBaseURL the base URL to start with (content.baseURL, may be null)
   * @param aURLSpec the URL of the link (may be relative)
   * @param aTargetSpec the target (like target=, may be null)
   * @param aClick whether this was a click or not (if false, it assumes you
   *        just hovered over the link)
   */
  nsresult TriggerLink(nsIPresContext* aPresContext,
                       nsLinkVerb aVerb,
                       nsIURI* aBaseURL,
                       const nsAString& aURLSpec,
                       const nsAFlatString& aTargetSpec,
                       PRBool aClick);
  /**
   * Do whatever needs to be done when the mouse leaves a link
   */
  nsresult LeaveLink(nsIPresContext* aPresContext);

  /**
   * Take two text nodes and append the second to the first.
   * @param aFirst the node which will contain first + second [INOUT]
   * @param aSecond the node which will be appended
   */
  nsresult JoinTextNodes(nsIContent* aFirst,
                         nsIContent* aSecond);

  /**
   * Set .document in the immediate children of said content (but not in
   * content itself).  SetDocument() in the children will recursively call
   * this.
   *
   * @param aContent the content to get the children of
   * @param aDocument the document to set
   * @param aCompileEventHandlers whether to initialize the event handlers in
   *        the document (used by nsXULElement)
   */
  static void SetDocumentInChildrenOf(nsIContent* aContent, 
				      nsIDocument* aDocument, PRBool aCompileEventHandlers);

  /**
   * Check whether a spec feature/version is supported.
   * @param aFeature the feature ("Views", "Core", "HTML", "Range" ...)
   * @param aVersion the version ("1.0", "2.0", ...)
   * @param aReturn whether the feature is supported or not [OUT]
   */
  static nsresult InternalIsSupported(const nsAString& aFeature,
                                      const nsAString& aVersion,
                                      PRBool* aReturn);

  /**
   * Quick helper to determine whether there are any mutation listeners
   * of a given type that apply to this content (are at or above it).
   * @param aContent the content to look for listeners for
   * @param aType the type of listener (NS_EVENT_BITS_MUTATION_*)
   * @return whether there are mutation listeners of the specified type for
   *         this content or not
   */
  static PRBool HasMutationListeners(nsIContent* aContent,
                                     PRUint32 aType);

  static nsresult InitHashes();

  static PLDHashTable sEventListenerManagersHash;
  static PLDHashTable sRangeListsHash;

protected:
#ifdef DEBUG
  virtual PRUint32 BaseSizeOf(nsISizeOfHandler *aSizer) const;
#endif

  PRBool HasDOMSlots() const
  {
    return !(mFlagsOrSlots & GENERIC_ELEMENT_DOESNT_HAVE_DOMSLOTS);
  }

  nsDOMSlots *GetDOMSlots()
  {
    if (!HasDOMSlots()) {
      nsDOMSlots *slots = new nsDOMSlots(mFlagsOrSlots);

      if (!slots) {
        return nsnull;
      }

      mFlagsOrSlots = NS_REINTERPRET_CAST(PtrBits, slots);
    }

    return NS_REINTERPRET_CAST(nsDOMSlots *, mFlagsOrSlots);
  }

  nsDOMSlots *GetExistingDOMSlots() const
  {
    if (!HasDOMSlots()) {
      return nsnull;
    }

    return NS_REINTERPRET_CAST(nsDOMSlots *, mFlagsOrSlots);
  }

  PtrBits GetFlags() const
  {
    if (HasDOMSlots()) {
      return NS_REINTERPRET_CAST(nsDOMSlots *, mFlagsOrSlots)->mFlags;
    }

    return mFlagsOrSlots;
  }

  void SetFlags(PtrBits aFlagsToSet)
  {
    NS_ASSERTION(!((aFlagsToSet & GENERIC_ELEMENT_CONTENT_ID_MASK) &&
                   (aFlagsToSet & ~GENERIC_ELEMENT_CONTENT_ID_MASK)),
                 "Whaaa, don't set content ID bits and flags together!!!");

    nsDOMSlots *slots = GetExistingDOMSlots();

    if (slots) {
      slots->mFlags |= aFlagsToSet;

      return;
    }

    mFlagsOrSlots |= aFlagsToSet;
  }

  void UnsetFlags(PtrBits aFlagsToUnset)
  {
    NS_ASSERTION(!((aFlagsToUnset & GENERIC_ELEMENT_CONTENT_ID_MASK) &&
                   (aFlagsToUnset & ~GENERIC_ELEMENT_CONTENT_ID_MASK)),
                 "Whaaa, don't set content ID bits and flags together!!!");

    nsDOMSlots *slots = GetExistingDOMSlots();

    if (slots) {
      slots->mFlags &= ~aFlagsToUnset;

      return;
    }

    mFlagsOrSlots &= ~aFlagsToUnset;
  }

  PRBool HasRangeList() const
  {
    PtrBits flags = GetFlags();

    return (flags & GENERIC_ELEMENT_HAS_RANGELIST && sRangeListsHash.ops);
  }

  PRBool HasEventListenerManager() const
  {
    PtrBits flags = GetFlags();

    return (flags & GENERIC_ELEMENT_HAS_LISTENERMANAGER &&
            sEventListenerManagersHash.ops);
  }

  /**
   * The document for this content
   */
  nsIDocument* mDocument;                   // WEAK

  /**
   * The parent content
   */
  nsIContent* mParent;                      // WEAK
  
  /**
   * Information about this type of node
   */
  nsINodeInfo* mNodeInfo;                   // OWNER

  /**
   * Used for either storing flags for this element or a pointer to
   * this elements nsDOMSlots. See the definition of the
   * GENERIC_ELEMENT_* macros for the layout of the bits in this
   * member.
   */
  PtrBits mFlagsOrSlots;
};

/**
 * A generic element that contains children
 */
class nsGenericContainerElement : public nsGenericElement {
public:
  nsGenericContainerElement();
  virtual ~nsGenericContainerElement();

  /**
   * Copy attributes and children from another content object
   * @param aSrcContent the object to copy from
   * @param aDest the destination object
   * @param aDeep whether to copy children
   */
  // XXX This can probably be static
  NS_IMETHOD CopyInnerTo(nsIContent* aSrcContent,
                         nsGenericContainerElement* aDest,
                         PRBool aDeep);

  // nsIDOMElement methods
  NS_METHOD GetAttribute(const nsAString& aName,
                         nsAString& aReturn) 
  {
    return nsGenericElement::GetAttribute(aName, aReturn);
  }
  NS_METHOD SetAttribute(const nsAString& aName,
                         const nsAString& aValue)
  {
    return nsGenericElement::SetAttribute(aName, aValue);
  }

  // Remainder of nsIDOMHTMLElement (and nsIDOMNode)
  NS_IMETHOD GetChildNodes(nsIDOMNodeList** aChildNodes);
  NS_IMETHOD HasChildNodes(PRBool* aHasChildNodes);
  NS_IMETHOD GetFirstChild(nsIDOMNode** aFirstChild);
  NS_IMETHOD GetLastChild(nsIDOMNode** aLastChild);
  
  NS_IMETHOD InsertBefore(nsIDOMNode* aNewChild, nsIDOMNode* aRefChild,
                          nsIDOMNode** aReturn)
  {
    return nsGenericElement::doInsertBefore(aNewChild, aRefChild, aReturn);
  }
  NS_IMETHOD ReplaceChild(nsIDOMNode* aNewChild, nsIDOMNode* aOldChild,
                          nsIDOMNode** aReturn)
  {
    return nsGenericElement::doReplaceChild(aNewChild, aOldChild, aReturn);
  }
  NS_IMETHOD RemoveChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn)
  {
    return nsGenericElement::doRemoveChild(aOldChild, aReturn);
  }
  NS_IMETHOD AppendChild(nsIDOMNode* aNewChild, nsIDOMNode** aReturn)
  {
    return nsGenericElement::doInsertBefore(aNewChild, nsnull, aReturn);
  }

  // Remainder of nsIContent
  NS_IMETHOD NormalizeAttrString(const nsAString& aStr,
                                 nsINodeInfo*& aNodeInfo);
  NS_IMETHOD SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                     const nsAString& aValue,
                     PRBool aNotify);
  NS_IMETHOD SetAttr(nsINodeInfo* aNodeInfo,
                     const nsAString& aValue,
                     PRBool aNotify);
  NS_IMETHOD GetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                     nsAString& aResult) const;
  NS_IMETHOD GetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                     nsIAtom*& aPrefix, nsAString& aResult) const;
  NS_IMETHOD_(PRBool) HasAttr(PRInt32 aNameSpaceID, nsIAtom* aName) const;
  NS_IMETHOD UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aAttribute,
                       PRBool aNotify);
  NS_IMETHOD GetAttrNameAt(PRInt32 aIndex,
                           PRInt32& aNameSpaceID,
                           nsIAtom*& aName,
                           nsIAtom*& aPrefix) const;
  NS_IMETHOD GetAttrCount(PRInt32& aResult) const;
#ifdef DEBUG
  NS_IMETHOD List(FILE* out, PRInt32 aIndent) const;
  NS_IMETHOD DumpContent(FILE* out, PRInt32 aIndent,PRBool aDumpAll) const;
#endif
  NS_IMETHOD CanContainChildren(PRBool& aResult) const;
  NS_IMETHOD ChildCount(PRInt32& aResult) const;
  NS_IMETHOD ChildAt(PRInt32 aIndex, nsIContent*& aResult) const;
  NS_IMETHOD IndexOf(nsIContent* aPossibleChild, PRInt32& aResult) const;
  NS_IMETHOD InsertChildAt(nsIContent* aKid, PRInt32 aIndex, PRBool aNotify,
                           PRBool aDeepSetDocument);
  NS_IMETHOD ReplaceChildAt(nsIContent* aKid, PRInt32 aIndex, PRBool aNotify,
                            PRBool aDeepSetDocument);
  NS_IMETHOD AppendChildTo(nsIContent* aKid, PRBool aNotify,
                           PRBool aDeepSetDocument);
  NS_IMETHOD RemoveChildAt(PRInt32 aIndex, PRBool aNotify);

#ifdef DEBUG
  void ListAttributes(FILE* out) const;
#endif

protected:
#ifdef DEBUG
  virtual PRUint32 BaseSizeOf(nsISizeOfHandler *aSizer) const;
#endif

  /**
   * The attributes (stored as nsGenericAttribute*)
   * @see nsGenericAttribute
   * */
  nsVoidArray* mAttributes;
  /** The list of children (stored as nsIContent*) */
  nsSmallVoidArray mChildren;
};


// Internal non-public interface

// IID for the nsIDocumentFragment interface
#define NS_IDOCUMENTFRAGMENT_IID      \
{ 0xd8fb2853, 0xf6d6, 0x4499, \
  {0x9c, 0x60, 0x6c, 0xa2, 0x75, 0x35, 0x09, 0xeb} }

// nsIDocumentFragment interface
/**
 * These methods are supposed to be used when *all* children of a
 * document fragment are moved at once into a new parent w/o
 * changing the relationship between the children. If the moving
 * operation fails and some children were moved to a new parent and
 * some weren't, ReconnectChildren() should be called to remove the
 * children from their possible new parent and re-insert the
 * children into the document fragment. Once the operation is
 * complete and all children are successfully moved into their new
 * parent DropChildReferences() should be called so that the
 * document fragment will loose its references to the children.
 */
class nsIDocumentFragment : public nsIDOMDocumentFragment
{
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IDOCUMENTFRAGMENT_IID)

  /** Tell the children their parent is gone */
  NS_IMETHOD DisconnectChildren() = 0;
  /** Put all children back in the fragment */
  NS_IMETHOD ReconnectChildren() = 0;
  /** Drop references to children */
  NS_IMETHOD DropChildReferences() = 0;
};


#define NS_FORWARD_NSIDOMNODE_NO_CLONENODE(_to)                               \
  NS_IMETHOD GetNodeName(nsAString& aNodeName) {                              \
    return _to GetNodeName(aNodeName);                                        \
  }                                                                           \
  NS_IMETHOD GetNodeValue(nsAString& aNodeValue) {                            \
    return _to GetNodeValue(aNodeValue);                                      \
  }                                                                           \
  NS_IMETHOD SetNodeValue(const nsAString& aNodeValue) {                      \
    return _to SetNodeValue(aNodeValue);                                      \
  }                                                                           \
  NS_IMETHOD GetNodeType(PRUint16* aNodeType) {                               \
    return _to GetNodeType(aNodeType);                                        \
  }                                                                           \
  NS_IMETHOD GetParentNode(nsIDOMNode** aParentNode) {                        \
    return _to GetParentNode(aParentNode);                                    \
  }                                                                           \
  NS_IMETHOD GetChildNodes(nsIDOMNodeList** aChildNodes) {                    \
    return _to GetChildNodes(aChildNodes);                                    \
  }                                                                           \
  NS_IMETHOD GetFirstChild(nsIDOMNode** aFirstChild) {                        \
    return _to GetFirstChild(aFirstChild);                                    \
  }                                                                           \
  NS_IMETHOD GetLastChild(nsIDOMNode** aLastChild) {                          \
    return _to GetLastChild(aLastChild);                                      \
  }                                                                           \
  NS_IMETHOD GetPreviousSibling(nsIDOMNode** aPreviousSibling) {              \
    return _to GetPreviousSibling(aPreviousSibling);                          \
  }                                                                           \
  NS_IMETHOD GetNextSibling(nsIDOMNode** aNextSibling) {                      \
    return _to GetNextSibling(aNextSibling);                                  \
  }                                                                           \
  NS_IMETHOD GetAttributes(nsIDOMNamedNodeMap** aAttributes) {                \
    return _to GetAttributes(aAttributes);                                    \
  }                                                                           \
  NS_IMETHOD GetOwnerDocument(nsIDOMDocument** aOwnerDocument) {              \
    return _to GetOwnerDocument(aOwnerDocument);                              \
  }                                                                           \
  NS_IMETHOD GetNamespaceURI(nsAString& aNamespaceURI) {                      \
    return _to GetNamespaceURI(aNamespaceURI);                                \
  }                                                                           \
  NS_IMETHOD GetPrefix(nsAString& aPrefix) {                                  \
    return _to GetPrefix(aPrefix);                                            \
  }                                                                           \
  NS_IMETHOD SetPrefix(const nsAString& aPrefix) {                            \
    return _to SetPrefix(aPrefix);                                            \
  }                                                                           \
  NS_IMETHOD GetLocalName(nsAString& aLocalName) {                            \
    return _to GetLocalName(aLocalName);                                      \
  }                                                                           \
  NS_IMETHOD InsertBefore(nsIDOMNode* aNewChild, nsIDOMNode* aRefChild,       \
                          nsIDOMNode** aReturn) {                             \
    return _to InsertBefore(aNewChild, aRefChild, aReturn);                   \
  }                                                                           \
  NS_IMETHOD ReplaceChild(nsIDOMNode* aNewChild, nsIDOMNode* aOldChild,       \
                          nsIDOMNode** aReturn) {                             \
    return _to ReplaceChild(aNewChild, aOldChild, aReturn);                   \
  }                                                                           \
  NS_IMETHOD RemoveChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn) {       \
    return _to RemoveChild(aOldChild, aReturn);                               \
  }                                                                           \
  NS_IMETHOD AppendChild(nsIDOMNode* aNewChild, nsIDOMNode** aReturn) {       \
    return _to AppendChild(aNewChild, aReturn);                               \
  }                                                                           \
  NS_IMETHOD HasChildNodes(PRBool* aReturn) {                                 \
    return _to HasChildNodes(aReturn);                                        \
  }                                                                           \
  NS_IMETHOD Normalize() {                                                    \
    return _to Normalize();                                                   \
  }                                                                           \
  NS_IMETHOD IsSupported(const nsAString& aFeature,                           \
                         const nsAString& aVersion, PRBool* aReturn) {        \
    return _to IsSupported(aFeature, aVersion, aReturn);                      \
  }                                                                           \
  NS_IMETHOD HasAttributes(PRBool* aReturn) {                                 \
    return _to HasAttributes(aReturn);                                        \
  }                                                                           \
  NS_IMETHOD CloneNode(PRBool aDeep, nsIDOMNode** aReturn);

#define NS_INTERFACE_MAP_ENTRY_TEAROFF(_iid, _tearoff)                        \
  if (aIID.Equals(NS_GET_IID(_iid))) {                                        \
    foundInterface = new _tearoff;                                            \
    NS_ENSURE_TRUE(foundInterface, NS_ERROR_OUT_OF_MEMORY);                   \
  } else

#endif /* nsGenericElement_h___ */
