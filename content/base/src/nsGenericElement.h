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

/*
 * Base class for all element classes; this provides an implementation
 * of DOM Core's nsIDOMElement, implements nsIContent, provides
 * utility methods for subclasses, and so forth.
 */

#ifndef nsGenericElement_h___
#define nsGenericElement_h___

#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "mozilla/dom/Element.h"
#include "nsIDOMElement.h"
#include "nsIDOMDocumentFragment.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOM3EventTarget.h"
#include "nsIDOM3Node.h"
#include "nsIDOMNSEventTarget.h"
#include "nsIDOMNSElement.h"
#include "nsILinkHandler.h"
#include "nsContentUtils.h"
#include "nsNodeUtils.h"
#include "nsAttrAndChildArray.h"
#include "mozFlushType.h"
#include "nsDOMAttributeMap.h"
#include "nsIWeakReference.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIDocument.h"
#include "nsIDOMNodeSelector.h"
#include "nsIDOMXPathNSResolver.h"
#include "nsPresContext.h"

#ifdef MOZ_SMIL
#include "nsISMILAttr.h"
#endif // MOZ_SMIL

class nsIDOMAttr;
class nsIDOMEventListener;
class nsIFrame;
class nsIDOMNamedNodeMap;
class nsICSSDeclaration;
class nsIDOMCSSStyleDeclaration;
class nsIURI;
class nsINodeInfo;
class nsIControllers;
class nsIDOMNSFeatureFactory;
class nsIEventListenerManager;
class nsIScrollableFrame;
class nsContentList;
class nsDOMTokenList;
struct nsRect;

typedef PRUptrdiff PtrBits;

/**
 * Class that implements the nsIDOMNodeList interface (a list of children of
 * the content), by holding a reference to the content and delegating GetLength
 * and Item to its existing child list.
 * @see nsIDOMNodeList
 */
class nsChildContentList : public nsINodeList,
                           public nsWrapperCache
{
public:
  nsChildContentList(nsINode* aNode)
    : mNode(aNode)
  {
  }

  NS_DECL_ISUPPORTS

  // nsIDOMNodeList interface
  NS_DECL_NSIDOMNODELIST

  // nsINodeList interface
  virtual nsIContent* GetNodeAt(PRUint32 aIndex);
  virtual PRInt32 IndexOf(nsIContent* aContent);

  void DropReference()
  {
    mNode = nsnull;
  }

  nsINode* GetParentObject()
  {
    return mNode;
  }

  static nsChildContentList* FromSupports(nsISupports* aSupports)
  {
    nsINodeList* list = static_cast<nsINodeList*>(aSupports);
#ifdef DEBUG
    {
      nsCOMPtr<nsINodeList> list_qi = do_QueryInterface(aSupports);

      // If this assertion fires the QI implementation for the object in
      // question doesn't use the nsINodeList pointer as the nsISupports
      // pointer. That must be fixed, or we'll crash...
      NS_ASSERTION(list_qi == list, "Uh, fix QI!");
    }
#endif
    return static_cast<nsChildContentList*>(list);
  }

private:
  // The node whose children make up the list (weak reference)
  nsINode* mNode;
};

/**
 * A tearoff class for nsGenericElement to implement additional interfaces
 */
class nsNode3Tearoff : public nsIDOM3Node, public nsIDOMXPathNSResolver
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

  NS_DECL_NSIDOM3NODE

  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsNode3Tearoff, nsIDOM3Node)

  nsNode3Tearoff(nsINode *aNode) : mNode(aNode)
  {
  }

protected:
  virtual ~nsNode3Tearoff() {}

private:
  nsCOMPtr<nsINode> mNode;
};

/**
 * A class that implements nsIWeakReference
 */

class nsNodeWeakReference : public nsIWeakReference
{
public:
  nsNodeWeakReference(nsINode* aNode)
    : mNode(aNode)
  {
  }

  ~nsNodeWeakReference();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIWeakReference
  NS_DECL_NSIWEAKREFERENCE

  void NoticeNodeDestruction()
  {
    mNode = nsnull;
  }

private:
  nsINode* mNode;
};

/**
 * Tearoff to use for nodes to implement nsISupportsWeakReference
 */
class nsNodeSupportsWeakRefTearoff : public nsISupportsWeakReference
{
public:
  nsNodeSupportsWeakRefTearoff(nsINode* aNode)
    : mNode(aNode)
  {
  }

  // nsISupports
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

  // nsISupportsWeakReference
  NS_DECL_NSISUPPORTSWEAKREFERENCE

  NS_DECL_CYCLE_COLLECTION_CLASS(nsNodeSupportsWeakRefTearoff)

private:
  nsCOMPtr<nsINode> mNode;
};

#define NS_EVENT_TEAROFF_CACHE_SIZE 4

/**
 * nsDOMEventRTTearoff is a tearoff class used by nsGenericElement and
 * nsGenericDOMDataNode classes for implementing the interfaces
 * nsIDOMEventTarget, nsIDOM3EventTarget and nsIDOMNSEventTarget.
 *
 * Use the method nsDOMEventRTTearoff::Create() to create one of these babies.
 * @see nsDOMEventRTTearoff::Create
 */

class nsDOMEventRTTearoff : public nsIDOMEventTarget,
                            public nsIDOM3EventTarget,
                            public nsIDOMNSEventTarget
{
private:
  // This class uses a caching scheme so we don't let users of this
  // class create new instances with 'new', in stead the callers
  // should use the static method
  // nsDOMEventRTTearoff::Create(). That's why the constructor and
  // destrucor of this class is private.

  nsDOMEventRTTearoff(nsINode *aNode);

  static nsDOMEventRTTearoff *mCachedEventTearoff[NS_EVENT_TEAROFF_CACHE_SIZE];
  static PRUint32 mCachedEventTearoffCount;

  /**
   * This method gets called by Release() when it's time to delete the
   * this object, in stead of always deleting the object we'll put the
   * object in the cache if unless the cache is already full.
   */
  void LastRelease();

  nsresult GetDOM3EventTarget(nsIDOM3EventTarget **aTarget);

public:
  virtual ~nsDOMEventRTTearoff();

  /**
   * Use this static method to create instances of nsDOMEventRTTearoff.
   * @param aContent the content to create a tearoff for
   */
  static nsDOMEventRTTearoff *Create(nsINode *aNode);

  /**
   * Call before shutdown to clear the cache and free memory for this class.
   */
  static void Shutdown();

  // nsISupports
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

  // nsIDOMEventTarget
  NS_DECL_NSIDOMEVENTTARGET

  // nsIDOM3EventTarget
  NS_DECL_NSIDOM3EVENTTARGET

  // nsIDOMNSEventTarget
  NS_DECL_NSIDOMNSEVENTTARGET

  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsDOMEventRTTearoff,
                                           nsIDOMEventTarget)

private:
  /**
   * Strong reference back to the content object from where an instance of this
   * class was 'torn off'
   */
  nsCOMPtr<nsINode> mNode;
};

/**
 * A tearoff class for nsGenericElement to implement NodeSelector
 */
class nsNodeSelectorTearoff : public nsIDOMNodeSelector
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

  NS_DECL_NSIDOMNODESELECTOR

  NS_DECL_CYCLE_COLLECTION_CLASS(nsNodeSelectorTearoff)

  nsNodeSelectorTearoff(nsINode *aNode) : mNode(aNode)
  {
  }

private:
  ~nsNodeSelectorTearoff() {}

private:
  nsCOMPtr<nsINode> mNode;
};

// Forward declare to allow being a friend
class nsNSElementTearoff;

/**
 * A generic base class for DOM elements, implementing many nsIContent,
 * nsIDOMNode and nsIDOMElement methods.
 */
class nsGenericElement : public mozilla::dom::Element
{
public:
  nsGenericElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  virtual ~nsGenericElement();

  friend class nsNSElementTearoff;

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

  /**
   * Called during QueryInterface to give the binding manager a chance to
   * get an interface for this element.
   */
  nsresult PostQueryInterface(REFNSIID aIID, void** aInstancePtr);

  // nsINode interface methods
  virtual PRUint32 GetChildCount() const;
  virtual nsIContent *GetChildAt(PRUint32 aIndex) const;
  virtual nsIContent * const * GetChildArray(PRUint32* aChildCount) const;
  virtual PRInt32 IndexOf(nsINode* aPossibleChild) const;
  virtual nsresult InsertChildAt(nsIContent* aKid, PRUint32 aIndex,
                                 PRBool aNotify);
  virtual nsresult RemoveChildAt(PRUint32 aIndex, PRBool aNotify, PRBool aMutationEvent = PR_TRUE);
  virtual nsresult PreHandleEvent(nsEventChainPreVisitor& aVisitor);
  virtual nsresult PostHandleEvent(nsEventChainPostVisitor& aVisitor);
  virtual nsresult DispatchDOMEvent(nsEvent* aEvent, nsIDOMEvent* aDOMEvent,
                                    nsPresContext* aPresContext,
                                    nsEventStatus* aEventStatus);
  virtual nsIEventListenerManager* GetListenerManager(PRBool aCreateIfNotFound);
  virtual nsresult AddEventListenerByIID(nsIDOMEventListener *aListener,
                                         const nsIID& aIID);
  virtual nsresult RemoveEventListenerByIID(nsIDOMEventListener *aListener,
                                            const nsIID& aIID);
  virtual nsresult GetSystemEventGroup(nsIDOMEventGroup** aGroup);
  virtual nsIScriptContext* GetContextForEventHandlers(nsresult* aRv)
  {
    return nsContentUtils::GetContextForEventHandlers(this, aRv);
  }
  virtual void GetTextContent(nsAString &aTextContent)
  {
    nsContentUtils::GetNodeTextContent(this, PR_TRUE, aTextContent);
  }
  virtual nsresult SetTextContent(const nsAString& aTextContent)
  {
    // Batch possible DOMSubtreeModified events.
    mozAutoSubtreeModified subtree(GetOwnerDoc(), nsnull);
    return nsContentUtils::SetNodeTextContent(this, aTextContent, PR_FALSE);
  }

  // nsIContent interface methods
  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              PRBool aCompileEventHandlers);
  virtual void UnbindFromTree(PRBool aDeep = PR_TRUE,
                              PRBool aNullParent = PR_TRUE);
  virtual already_AddRefed<nsINodeList> GetChildren(PRUint32 aFilter);
  virtual nsIAtom *GetClassAttributeName() const;
  virtual already_AddRefed<nsINodeInfo> GetExistingAttrNameFromQName(const nsAString& aStr) const;
  nsresult SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                   const nsAString& aValue, PRBool aNotify)
  {
    return SetAttr(aNameSpaceID, aName, nsnull, aValue, aNotify);
  }
  virtual nsresult SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName, nsIAtom* aPrefix,
                           const nsAString& aValue, PRBool aNotify);
  virtual PRBool GetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                         nsAString& aResult) const;
  virtual PRBool HasAttr(PRInt32 aNameSpaceID, nsIAtom* aName) const;
  virtual PRBool AttrValueIs(PRInt32 aNameSpaceID, nsIAtom* aName,
                             const nsAString& aValue,
                             nsCaseTreatment aCaseSensitive) const;
  virtual PRBool AttrValueIs(PRInt32 aNameSpaceID, nsIAtom* aName,
                             nsIAtom* aValue,
                             nsCaseTreatment aCaseSensitive) const;
  virtual PRInt32 FindAttrValueIn(PRInt32 aNameSpaceID,
                                  nsIAtom* aName,
                                  AttrValuesArray* aValues,
                                  nsCaseTreatment aCaseSensitive) const;
  virtual nsresult UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aAttribute,
                             PRBool aNotify);
  virtual const nsAttrName* GetAttrNameAt(PRUint32 aIndex) const;
  virtual PRUint32 GetAttrCount() const;
  virtual const nsTextFragment *GetText();
  virtual PRUint32 TextLength();
  virtual nsresult SetText(const PRUnichar* aBuffer, PRUint32 aLength,
                           PRBool aNotify);
  // Need to implement this here too to avoid hiding.
  nsresult SetText(const nsAString& aStr, PRBool aNotify)
  {
    return SetText(aStr.BeginReading(), aStr.Length(), aNotify);
  }
  virtual nsresult AppendText(const PRUnichar* aBuffer, PRUint32 aLength,
                              PRBool aNotify);
  virtual PRBool TextIsOnlyWhitespace();
  virtual void AppendTextTo(nsAString& aResult);
  virtual nsIContent *GetBindingParent() const;
  virtual PRBool IsNodeOfType(PRUint32 aFlags) const;
  virtual already_AddRefed<nsIURI> GetBaseURI() const;
  virtual PRBool IsLink(nsIURI** aURI) const;

  virtual PRUint32 GetScriptTypeID() const;
  NS_IMETHOD SetScriptTypeID(PRUint32 aLang);

  virtual void DestroyContent();
  virtual void SaveSubtreeState();

#ifdef MOZ_SMIL
  virtual nsISMILAttr* GetAnimatedAttr(PRInt32 /*aNamespaceID*/, nsIAtom* /*aName*/)
  {
    return nsnull;
  }
  virtual nsresult GetSMILOverrideStyle(nsIDOMCSSStyleDeclaration** aStyle);
  virtual nsICSSStyleRule* GetSMILOverrideStyleRule();
  virtual nsresult SetSMILOverrideStyleRule(nsICSSStyleRule* aStyleRule,
                                            PRBool aNotify);
#endif // MOZ_SMIL

#ifdef DEBUG
  virtual void List(FILE* out, PRInt32 aIndent) const
  {
    List(out, aIndent, EmptyCString());
  }
  virtual void DumpContent(FILE* out, PRInt32 aIndent, PRBool aDumpAll) const;
  void List(FILE* out, PRInt32 aIndent, const nsCString& aPrefix) const;
  void ListAttributes(FILE* out) const;
#endif

  virtual const nsAttrValue* DoGetClasses() const;
  NS_IMETHOD WalkContentStyleRules(nsRuleWalker* aRuleWalker);
  virtual nsICSSStyleRule* GetInlineStyleRule();
  NS_IMETHOD SetInlineStyleRule(nsICSSStyleRule* aStyleRule, PRBool aNotify);
  NS_IMETHOD_(PRBool)
    IsAttributeMapped(const nsIAtom* aAttribute) const;
  virtual nsChangeHint GetAttributeChangeHint(const nsIAtom* aAttribute,
                                              PRInt32 aModType) const;
  /*
   * Attribute Mapping Helpers
   */
  struct MappedAttributeEntry {
    nsIAtom** attribute;
  };

  /**
   * A common method where you can just pass in a list of maps to check
   * for attribute dependence. Most implementations of
   * IsAttributeMapped should use this function as a default
   * handler.
   */
  static PRBool
  FindAttributeDependence(const nsIAtom* aAttribute,
                          const MappedAttributeEntry* const aMaps[],
                          PRUint32 aMapCount);

  // nsIDOMNode method implementation
  NS_IMETHOD GetNodeName(nsAString& aNodeName);
  NS_IMETHOD GetLocalName(nsAString& aLocalName);
  NS_IMETHOD GetNodeValue(nsAString& aNodeValue);
  NS_IMETHOD SetNodeValue(const nsAString& aNodeValue);
  NS_IMETHOD GetNodeType(PRUint16* aNodeType);
  NS_IMETHOD GetAttributes(nsIDOMNamedNodeMap** aAttributes);
  NS_IMETHOD GetNamespaceURI(nsAString& aNamespaceURI);
  NS_IMETHOD GetPrefix(nsAString& aPrefix);
  NS_IMETHOD SetPrefix(const nsAString& aPrefix);
  NS_IMETHOD Normalize();
  NS_IMETHOD IsSupported(const nsAString& aFeature,
                         const nsAString& aVersion, PRBool* aReturn);
  NS_IMETHOD HasAttributes(PRBool* aHasAttributes);
  NS_IMETHOD HasChildNodes(PRBool* aHasChildNodes);
  nsresult InsertBefore(nsIDOMNode* aNewChild, nsIDOMNode* aRefChild,
                        nsIDOMNode** aReturn)
  {
    return ReplaceOrInsertBefore(PR_FALSE, aNewChild, aRefChild, aReturn);
  }
  nsresult ReplaceChild(nsIDOMNode* aNewChild, nsIDOMNode* aOldChild,
                        nsIDOMNode** aReturn)
  {
    return ReplaceOrInsertBefore(PR_TRUE, aNewChild, aOldChild, aReturn);
  }
  nsresult RemoveChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn)
  {
    return nsINode::RemoveChild(aOldChild, aReturn);
  }
  nsresult AppendChild(nsIDOMNode* aNewChild, nsIDOMNode** aReturn)
  {
    return InsertBefore(aNewChild, nsnull, aReturn);
  }

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
  nsresult CloneNode(PRBool aDeep, nsIDOMNode **aResult)
  {
    return nsNodeUtils::CloneNodeImpl(this, aDeep, aResult);
  }

  //----------------------------------------

  /**
   * Add a script event listener with the given event handler name
   * (like onclick) and with the value as JS
   * @param aEventName the event listener name
   * @param aValue the JS to attach
   * @param aDefer indicates if deferred execution is allowed
   */
  nsresult AddScriptEventListener(nsIAtom* aEventName,
                                  const nsAString& aValue,
                                  PRBool aDefer = PR_TRUE);

  /**
   * Do whatever needs to be done when the mouse leaves a link
   */
  nsresult LeaveLink(nsPresContext* aPresContext);

  /**
   * Take two text nodes and append the second to the first.
   * @param aFirst the node which will contain first + second [INOUT]
   * @param aSecond the node which will be appended
   */
  nsresult JoinTextNodes(nsIContent* aFirst,
                         nsIContent* aSecond);

  /**
   * Check whether a spec feature/version is supported.
   * @param aObject the object, which should support the feature,
   *        for example nsIDOMNode or nsIDOMDOMImplementation
   * @param aFeature the feature ("Views", "Core", "HTML", "Range" ...)
   * @param aVersion the version ("1.0", "2.0", ...)
   * @param aReturn whether the feature is supported or not [OUT]
   */
  static nsresult InternalIsSupported(nsISupports* aObject,
                                      const nsAString& aFeature,
                                      const nsAString& aVersion,
                                      PRBool* aReturn);

  static PRBool ShouldBlur(nsIContent *aContent);

  /**
   * If there are listeners for DOMNodeInserted event, fires the event on all
   * aNodes
   */
  static void FireNodeInserted(nsIDocument* aDoc,
                               nsINode* aParent,
                               nsCOMArray<nsIContent>& aNodes);

  /**
   * Helper methods for implementing querySelector/querySelectorAll
   */
  static nsIContent* doQuerySelector(nsINode* aRoot, const nsAString& aSelector,
                                     nsresult *aResult NS_OUTPARAM);
  static nsresult doQuerySelectorAll(nsINode* aRoot,
                                     const nsAString& aSelector,
                                     nsIDOMNodeList **aReturn);

  /**
   * Default event prehandling for content objects. Handles event retargeting.
   */
  static nsresult doPreHandleEvent(nsIContent* aContent,
                                   nsEventChainPreVisitor& aVisitor);

  /**
   * Method to create and dispatch a left-click event loosely based on
   * aSourceEvent. If aFullDispatch is true, the event will be dispatched
   * through the full dispatching of the presshell of the aPresContext; if it's
   * false the event will be dispatched only as a DOM event.
   * If aPresContext is nsnull, this does nothing.
   */
  static nsresult DispatchClickEvent(nsPresContext* aPresContext,
                                     nsInputEvent* aSourceEvent,
                                     nsIContent* aTarget,
                                     PRBool aFullDispatch,
                                     nsEventStatus* aStatus);

  /**
   * Method to dispatch aEvent to aTarget. If aFullDispatch is true, the event
   * will be dispatched through the full dispatching of the presshell of the
   * aPresContext; if it's false the event will be dispatched only as a DOM
   * event.
   * If aPresContext is nsnull, this does nothing.
   */
  static nsresult DispatchEvent(nsPresContext* aPresContext,
                                nsEvent* aEvent,
                                nsIContent* aTarget,
                                PRBool aFullDispatch,
                                nsEventStatus* aStatus);

  /**
   * Get the primary frame for this content with flushing
   *
   * @param aType the kind of flush to do, typically Flush_Frames or
   *              Flush_Layout
   * @return the primary frame
   */
  nsIFrame* GetPrimaryFrame(mozFlushType aType);
  // Work around silly C++ name hiding stuff
  nsIFrame* GetPrimaryFrame() const { return nsIContent::GetPrimaryFrame(); }

  /**
   * Struct that stores info on an attribute.  The name and value must
   * either both be null or both be non-null.
   */
  struct nsAttrInfo {
    nsAttrInfo(const nsAttrName* aName, const nsAttrValue* aValue) :
      mName(aName), mValue(aValue) {}
    nsAttrInfo(const nsAttrInfo& aOther) :
      mName(aOther.mName), mValue(aOther.mValue) {}

    const nsAttrName* mName;
    const nsAttrValue* mValue;
  };

  // Be careful when using this method. This does *NOT* handle
  // XUL prototypes. You may want to use GetAttrInfo.
  const nsAttrValue* GetParsedAttr(nsIAtom* aAttr) const
  {
    return mAttrsAndChildren.GetAttr(aAttr);
  }

  /**
   * Returns the attribute map, if there is one.
   *
   * @return existing attribute map or nsnull.
   */
  nsDOMAttributeMap *GetAttributeMap()
  {
    nsDOMSlots *slots = GetExistingDOMSlots();

    return slots ? slots->mAttributeMap.get() : nsnull;
  }

  virtual void RecompileScriptEventListeners()
  {
  }

  // nsIDOMNSElement methods
  nsresult GetElementsByClassName(const nsAString& aClasses,
                                  nsIDOMNodeList** aReturn)
  {
    return nsContentUtils::GetElementsByClassName(this, aClasses, aReturn);
  }
  nsresult GetClientRects(nsIDOMClientRectList** aResult);
  nsresult GetBoundingClientRect(nsIDOMClientRect** aResult);
  PRInt32 GetScrollTop();
  void SetScrollTop(PRInt32 aScrollTop);
  PRInt32 GetScrollLeft();
  void SetScrollLeft(PRInt32 aScrollLeft);
  PRInt32 GetScrollHeight();
  PRInt32 GetScrollWidth();
  PRInt32 GetClientTop()
  {
    return nsPresContext::AppUnitsToIntCSSPixels(GetClientAreaRect().y);
  }
  PRInt32 GetClientLeft()
  {
    return nsPresContext::AppUnitsToIntCSSPixels(GetClientAreaRect().x);
  }
  PRInt32 GetClientHeight()
  {
    return nsPresContext::AppUnitsToIntCSSPixels(GetClientAreaRect().height);
  }
  PRInt32 GetClientWidth()
  {
    return nsPresContext::AppUnitsToIntCSSPixels(GetClientAreaRect().width);
  }
  nsIContent* GetFirstElementChild();
  nsIContent* GetLastElementChild();
  nsIContent* GetPreviousElementSibling();
  nsIContent* GetNextElementSibling();
  nsresult GetChildElementCount(PRUint32* aResult)
  {
    nsContentList* list = GetChildrenList();
    if (!list) {
      *aResult = 0;

      return NS_ERROR_OUT_OF_MEMORY;
    }

    *aResult = list->Length(PR_TRUE);

    return NS_OK;
  }
  nsresult GetChildren(nsIDOMNodeList** aResult)
  {
    nsContentList* list = GetChildrenList();
    if (!list) {
      *aResult = nsnull;

      return NS_ERROR_OUT_OF_MEMORY;
    }

    NS_ADDREF(*aResult = list);

    return NS_OK;
  }
  nsIDOMDOMTokenList* GetClassList(nsresult *aResult);
  void SetCapture(PRBool aRetargetToElement);
  void ReleaseCapture();
  PRBool MozMatchesSelector(const nsAString& aSelector, nsresult* aResult);

  /**
   * Get the attr info for the given namespace ID and attribute name.  The
   * namespace ID must not be kNameSpaceID_Unknown and the name must not be
   * null.  Note that this can only return info on attributes that actually
   * live on this element (and is only virtual to handle XUL prototypes).  That
   * is, this should only be called from methods that only care about attrs
   * that effectively live in mAttrsAndChildren.
   */
  virtual nsAttrInfo GetAttrInfo(PRInt32 aNamespaceID, nsIAtom* aName) const;

  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(nsGenericElement)

  virtual void NodeInfoChanged(nsINodeInfo* aOldNodeInfo)
  {
  }

protected:
  /**
   * Set attribute and (if needed) notify documentobservers and fire off
   * mutation events.  This will send the AttributeChanged notification.
   * Callers of this method are responsible for calling AttributeWillChange,
   * since that needs to happen before the new attr value has been set, and
   * in particular before it has been parsed.
   *
   * @param aNamespaceID  namespace of attribute
   * @param aAttribute    local-name of attribute
   * @param aPrefix       aPrefix of attribute
   * @param aOldValue     previous value of attribute. Only needed if
   *                      aFireMutation is true.
   * @param aParsedValue  parsed new value of attribute
   * @param aModType      nsIDOMMutationEvent::MODIFICATION or ADDITION.  Only
   *                      needed if aFireMutation or aNotify is true.
   * @param aFireMutation should mutation-events be fired?
   * @param aNotify       should we notify document-observers?
   * @param aValueForAfterSetAttr If not null, AfterSetAttr will be called
   *                      with the value pointed by this parameter.
   */
  nsresult SetAttrAndNotify(PRInt32 aNamespaceID,
                            nsIAtom* aName,
                            nsIAtom* aPrefix,
                            const nsAString& aOldValue,
                            nsAttrValue& aParsedValue,
                            PRUint8 aModType,
                            PRBool aFireMutation,
                            PRBool aNotify,
                            const nsAString* aValueForAfterSetAttr);

  /**
   * Convert an attribute string value to attribute type based on the type of
   * attribute.  Called by SetAttr().  Note that at the moment we only do this
   * for attributes in the null namespace (kNameSpaceID_None).
   *
   * @param aNamespaceID the namespace of the attribute to convert
   * @param aAttribute the attribute to convert
   * @param aValue the string value to convert
   * @param aResult the nsAttrValue [OUT]
   * @return PR_TRUE if the parsing was successful, PR_FALSE otherwise
   */
  virtual PRBool ParseAttribute(PRInt32 aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult);

  /**
   * Try to set the attribute as a mapped attribute, if applicable.  This will
   * only be called for attributes that are in the null namespace and only on
   * attributes that returned true when passed to IsAttributeMapped.  The
   * caller will not try to set the attr in any other way if this method
   * returns PR_TRUE (the value of aRetval does not matter for that purpose).
   *
   * @param aDocument the current document of this node (an optimization)
   * @param aName the name of the attribute
   * @param aValue the nsAttrValue to set
   * @param [out] aRetval the nsresult status of the operation, if any.
   * @return PR_TRUE if the setting was attempted, PR_FALSE otherwise.
   */
  virtual PRBool SetMappedAttribute(nsIDocument* aDocument,
                                    nsIAtom* aName,
                                    nsAttrValue& aValue,
                                    nsresult* aRetval);

  /**
   * Hook that is called by nsGenericElement::SetAttr to allow subclasses to
   * deal with attribute sets.  This will only be called after we verify that
   * we're actually doing an attr set and will be called before
   * AttributeWillChange and before ParseAttribute and hence before we've set
   * the new value.
   *
   * @param aNamespaceID the namespace of the attr being set
   * @param aName the localname of the attribute being set
   * @param aValue the value it's being set to.  If null, the attr is being
   *        removed.
   * @param aNotify Whether we plan to notify document observers.
   */
  // Note that this is inlined so that when subclasses call it it gets
  // inlined.  Those calls don't go through a vtable.
  virtual nsresult BeforeSetAttr(PRInt32 aNamespaceID, nsIAtom* aName,
                                 const nsAString* aValue, PRBool aNotify)
  {
    return NS_OK;
  }

  /**
   * Hook that is called by nsGenericElement::SetAttr to allow subclasses to
   * deal with attribute sets.  This will only be called after we have called
   * SetAndTakeAttr and AttributeChanged (that is, after we have actually set
   * the attr).
   *
   * @param aNamespaceID the namespace of the attr being set
   * @param aName the localname of the attribute being set
   * @param aValue the value it's being set to.  If null, the attr is being
   *        removed.
   * @param aNotify Whether we plan to notify document observers.
   */
  // Note that this is inlined so that when subclasses call it it gets
  // inlined.  Those calls don't go through a vtable.
  virtual nsresult AfterSetAttr(PRInt32 aNamespaceID, nsIAtom* aName,
                                const nsAString* aValue, PRBool aNotify)
  {
    return NS_OK;
  }

  /**
   * Hook to allow subclasses to produce a different nsIEventListenerManager if
   * needed for attachment of attribute-defined handlers
   */
  virtual nsresult
    GetEventListenerManagerForAttr(nsIEventListenerManager** aManager,
                                   nsISupports** aTarget,
                                   PRBool* aDefer);

  /**
   * Copy attributes and state to another element
   * @param aDest the object to copy to
   */
  nsresult CopyInnerTo(nsGenericElement* aDest) const;

  /**
   * Internal hook for converting an attribute name-string to an atomized name
   */
  virtual const nsAttrName* InternalGetExistingAttrNameFromQName(const nsAString& aStr) const;

  /**
   * Retrieve the rectangle for the offsetX properties, which
   * are coordinates relative to the returned aOffsetParent.
   *
   * @param aRect offset rectangle
   * @param aOffsetParent offset parent
   */
  virtual void GetOffsetRect(nsRect& aRect, nsIContent** aOffsetParent);

  nsIFrame* GetStyledFrame();

  virtual mozilla::dom::Element* GetNameSpaceElement()
  {
    return this;
  }

public:
  // Because of a bug in MS C++ compiler nsDOMSlots must be declared public,
  // otherwise nsXULElement::nsXULSlots doesn't compile.
  /**
   * There are a set of DOM- and scripting-specific instance variables
   * that may only be instantiated when a content object is accessed
   * through the DOM. Rather than burn actual slots in the content
   * objects for each of these instance variables, we put them off
   * in a side structure that's only allocated when the content is
   * accessed through the DOM.
   */
  class nsDOMSlots : public nsINode::nsSlots
  {
  public:
    nsDOMSlots(PtrBits aFlags);
    virtual ~nsDOMSlots();

    /**
     * The .style attribute (an interface that forwards to the actual
     * style rules)
     * @see nsGenericHTMLElement::GetStyle */
    nsCOMPtr<nsICSSDeclaration> mStyle;

    /**
     * SMIL Overridde style rules (for SMIL animation of CSS properties)
     * @see nsIContent::GetSMILOverrideStyle
     */
    nsCOMPtr<nsICSSDeclaration> mSMILOverrideStyle;

    /**
     * Holds any SMIL override style rules for this element.
     */
    nsCOMPtr<nsICSSStyleRule> mSMILOverrideStyleRule;

    /**
     * An object implementing nsIDOMNamedNodeMap for this content (attributes)
     * @see nsGenericElement::GetAttributes
     */
    nsRefPtr<nsDOMAttributeMap> mAttributeMap;

    union {
      /**
      * The nearest enclosing content node with a binding that created us.
      * @see nsGenericElement::GetBindingParent
      */
      nsIContent* mBindingParent;  // [Weak]

      /**
      * The controllers of the XUL Element.
      */
      nsIControllers* mControllers; // [OWNER]
    };

    /**
     * An object implementing the .children property for this element.
     */
    nsRefPtr<nsContentList> mChildrenList;

    /**
     * An object implementing the .classList property for this element.
     */
    nsRefPtr<nsDOMTokenList> mClassList;
  };

protected:
  // Override from nsINode
  virtual nsINode::nsSlots* CreateSlots();

  nsDOMSlots *GetDOMSlots()
  {
    return static_cast<nsDOMSlots*>(GetSlots());
  }

  nsDOMSlots *GetExistingDOMSlots() const
  {
    return static_cast<nsDOMSlots*>(GetExistingSlots());
  }

  void RegisterFreezableElement() {
    nsIDocument* doc = GetOwnerDoc();
    if (doc) {
      doc->RegisterFreezableElement(this);
    }
  }
  void UnregisterFreezableElement() {
    nsIDocument* doc = GetOwnerDoc();
    if (doc) {
      doc->UnregisterFreezableElement(this);
    }
  }

  /**
   * Add/remove this element to the documents id cache
   */
  void AddToIdTable(nsIAtom* aId) {
    NS_ASSERTION(HasFlag(NODE_HAS_ID), "Node lacking NODE_HAS_ID flag");
    nsIDocument* doc = GetCurrentDoc();
    if (doc && (!IsInAnonymousSubtree() || doc->IsXUL())) {
      doc->AddToIdTable(this, aId);
    }
  }
  void RemoveFromIdTable() {
    if (HasFlag(NODE_HAS_ID)) {
      nsIDocument* doc = GetCurrentDoc();
      if (doc) {
        nsIAtom* id = DoGetID();
        // id can be null during mutation events evilness. Also, XUL elements
        // loose their proto attributes during cc-unlink, so this can happen
        // during cc-unlink too.
        if (id) {
          doc->RemoveFromIdTable(this, DoGetID());
        }
      }
    }
  }

  /**
   * Functions to carry out event default actions for links of all types
   * (HTML links, XLinks, SVG "XLinks", etc.)
   */

  /**
   * Check that we meet the conditions to handle a link event
   * and that we are actually on a link.
   *
   * @param aVisitor event visitor
   * @param aURI the uri of the link, set only if the return value is PR_TRUE [OUT]
   * @return PR_TRUE if we can handle the link event, PR_FALSE otherwise
   */
  PRBool CheckHandleEventForLinksPrecondition(nsEventChainVisitor& aVisitor,
                                              nsIURI** aURI) const;

  /**
   * Handle status bar updates before they can be cancelled.
   */
  nsresult PreHandleEventForLinks(nsEventChainPreVisitor& aVisitor);

  /**
   * Handle default actions for link event if the event isn't consumed yet.
   */
  nsresult PostHandleEventForLinks(nsEventChainPostVisitor& aVisitor);

  /**
   * Get the target of this link element. Consumers should established that
   * this element is a link (probably using IsLink) before calling this
   * function (or else why call it?)
   *
   * Note: for HTML this gets the value of the 'target' attribute; for XLink
   * this gets the value of the xlink:_moz_target attribute, or failing that,
   * the value of xlink:show, converted to a suitably equivalent named target
   * (e.g. _blank).
   */
  virtual void GetLinkTarget(nsAString& aTarget);

  /**
   * Array containing all attributes and children for this element
   */
  nsAttrAndChildArray mAttrsAndChildren;

private:
  /**
   * Get this element's client area rect in app units.
   * @return the frame's client area
   */
  nsRect GetClientAreaRect();

  nsIScrollableFrame* GetScrollFrame(nsIFrame **aStyledFrame = nsnull);

  nsContentList* GetChildrenList();
};

/**
 * Macros to implement Clone(). _elementName is the class for which to implement
 * Clone.
 */
#define NS_IMPL_ELEMENT_CLONE(_elementName)                                 \
nsresult                                                                    \
_elementName::Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const        \
{                                                                           \
  *aResult = nsnull;                                                        \
  nsCOMPtr<nsINodeInfo> ni = aNodeInfo;                                     \
  _elementName *it = new _elementName(ni.forget());                         \
  if (!it) {                                                                \
    return NS_ERROR_OUT_OF_MEMORY;                                          \
  }                                                                         \
                                                                            \
  nsCOMPtr<nsINode> kungFuDeathGrip = it;                                   \
  nsresult rv = CopyInnerTo(it);                                            \
  if (NS_SUCCEEDED(rv)) {                                                   \
    kungFuDeathGrip.swap(*aResult);                                         \
  }                                                                         \
                                                                            \
  return rv;                                                                \
}

#define NS_IMPL_ELEMENT_CLONE_WITH_INIT(_elementName)                       \
nsresult                                                                    \
_elementName::Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const        \
{                                                                           \
  *aResult = nsnull;                                                        \
  nsCOMPtr<nsINodeInfo> ni = aNodeInfo;                                     \
  _elementName *it = new _elementName(ni.forget());                         \
  if (!it) {                                                                \
    return NS_ERROR_OUT_OF_MEMORY;                                          \
  }                                                                         \
                                                                            \
  nsCOMPtr<nsINode> kungFuDeathGrip = it;                                   \
  nsresult rv = it->Init();                                                 \
  rv |= CopyInnerTo(it);                                                    \
  if (NS_SUCCEEDED(rv)) {                                                   \
    kungFuDeathGrip.swap(*aResult);                                         \
  }                                                                         \
                                                                            \
  return rv;                                                                \
}

#define DOMCI_NODE_DATA(_interface, _class)                             \
  DOMCI_DATA(_interface, _class)                                        \
  nsXPCClassInfo* _class::GetClassInfo()                                \
  {                                                                     \
    return static_cast<nsXPCClassInfo*>(                                \
      NS_GetDOMClassInfoInstance(eDOMClassInfo_##_interface##_id));     \
  }

/**
 * Yet another tearoff class for nsGenericElement
 * to implement additional interfaces
 */
class nsNSElementTearoff : public nsIDOMNSElement
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

  NS_DECL_NSIDOMNSELEMENT

  NS_DECL_CYCLE_COLLECTION_CLASS(nsNSElementTearoff)

  nsNSElementTearoff(nsGenericElement *aContent) : mContent(aContent)
  {
  }

private:
  nsRefPtr<nsGenericElement> mContent;
};

#define NS_ELEMENT_INTERFACE_TABLE_TO_MAP_SEGUE                               \
    rv = nsGenericElement::QueryInterface(aIID, aInstancePtr);                \
    if (NS_SUCCEEDED(rv))                                                     \
      return rv;                                                              \
                                                                              \
    NS_OFFSET_AND_INTERFACE_TABLE_TO_MAP_SEGUE

#define NS_ELEMENT_INTERFACE_MAP_END                                          \
    {                                                                         \
      return PostQueryInterface(aIID, aInstancePtr);                          \
    }                                                                         \
                                                                              \
    NS_ADDREF(foundInterface);                                                \
                                                                              \
    *aInstancePtr = foundInterface;                                           \
                                                                              \
    return NS_OK;                                                             \
  }

#endif /* nsGenericElement_h___ */
