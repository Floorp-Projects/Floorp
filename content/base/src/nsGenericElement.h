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
#include "nsIContent.h"
#include "nsIDOMElement.h"
#include "nsIDOMDocumentFragment.h"
#include "nsIDOMEventReceiver.h"
#include "nsIDOM3EventTarget.h"
#include "nsIDOM3Node.h"
#include "nsIDOMNSEventTarget.h"
#include "nsIDOMNSElement.h"
#include "nsILinkHandler.h"
#include "nsGenericDOMNodeList.h"
#include "nsContentUtils.h"
#include "nsNodeUtils.h"
#include "nsAttrAndChildArray.h"
#include "mozFlushType.h"
#include "nsDOMAttributeMap.h"
#include "nsIWeakReference.h"
#include "nsCycleCollectionParticipant.h"

class nsIDOMAttr;
class nsIDOMEventListener;
class nsIFrame;
class nsIDOMNamedNodeMap;
class nsDOMCSSDeclaration;
class nsIDOMCSSStyleDeclaration;
class nsIURI;
class nsVoidArray;
class nsINodeInfo;
class nsIControllers;
class nsIDOMNSFeatureFactory;
class nsIEventListenerManager;

typedef unsigned long PtrBits;

/**
 * Class that implements the nsIDOMNodeList interface (a list of children of
 * the content), by holding a reference to the content and delegating GetLength
 * and Item to its existing child list.
 * @see nsIDOMNodeList
 */
class nsChildContentList : public nsGenericDOMNodeList 
{
public:
  nsChildContentList(nsINode* aNode)
    : mNode(aNode)
  {
    MOZ_COUNT_CTOR(nsChildContentList);
  }
  virtual ~nsChildContentList();

  // nsIDOMNodeList interface
  NS_DECL_NSIDOMNODELIST
  
  void DropReference()
  {
    mNode = nsnull;
  }

private:
  // The node whose children make up the list (weak reference)
  nsINode* mNode;
};

/**
 * A tearoff class for nsGenericElement to implement additional interfaces
 */
class nsNode3Tearoff : public nsIDOM3Node
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

  NS_DECL_NSIDOM3NODE

  NS_DECL_CYCLE_COLLECTION_CLASS(nsNode3Tearoff)

  nsNode3Tearoff(nsIContent *aContent) : mContent(aContent)
  {
  }

  /**
   * Determines whether two nodes are equal.
   *
   * @param aContent1 The first node to compare.
   * @param aContent2 The second node to compare.
   *
   * @return PR_TRUE if the nodes are equal.
   */
  static PRBool AreNodesEqual(nsIContent* aContent1,
                              nsIContent* aContent2);

protected:
  virtual ~nsNode3Tearoff() {};

private:
  nsCOMPtr<nsIContent> mContent;
};

/**
 * Yet another tearoff class for nsGenericElement
 * to implement additional interfaces
 */
class nsNSElementTearoff : public nsIDOMNSElement
{
public:
  NS_DECL_ISUPPORTS

  NS_DECL_NSIDOMNSELEMENT

  nsNSElementTearoff(nsIContent *aContent) : mContent(aContent)
  {
  }
  
private:
  nsCOMPtr<nsIContent> mContent;
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
 * nsIDOMEventReceiver and nsIDOMEventTarget
 *
 * Use the method nsDOMEventRTTearoff::Create() to create one of these babies.
 * @see nsDOMEventRTTearoff::Create
 */

class nsDOMEventRTTearoff : public nsIDOMEventReceiver,
                            public nsIDOM3EventTarget,
                            public nsIDOMNSEventTarget
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
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

  // nsIDOMEventTarget
  NS_DECL_NSIDOMEVENTTARGET

  // nsIDOM3EventTarget
  NS_DECL_NSIDOM3EVENTTARGET

  // nsIDOMEventReceiver
  NS_IMETHOD AddEventListenerByIID(nsIDOMEventListener *aListener,
                                   const nsIID& aIID);
  NS_IMETHOD RemoveEventListenerByIID(nsIDOMEventListener *aListener,
                                      const nsIID& aIID);
  NS_IMETHOD GetListenerManager(PRBool aCreateIfNotFound,
                                nsIEventListenerManager** aResult);
  NS_IMETHOD HandleEvent(nsIDOMEvent *aEvent);
  NS_IMETHOD GetSystemEventGroup(nsIDOMEventGroup** aGroup);

  // nsIDOMNSEventTarget
  NS_DECL_NSIDOMNSEVENTTARGET

  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsDOMEventRTTearoff,
                                           nsIDOMEventTarget)

private:
  /**
   * Strong reference back to the content object from where an instance of this
   * class was 'torn off'
   */
  nsCOMPtr<nsIContent> mContent;
};

/**
 * Class used to detect unexpected mutations. To use the class create an
 * nsMutationGuard on the stack before unexpected mutations could occur.
 * You can then at any time call Mutated to check if any unexpected mutations
 * have occured.
 *
 * When a guard is instantiated sMutationCount is set to 300. It is then
 * decremented by every mutation (capped at 0). This means that we can only
 * detect 300 mutations during the lifetime of a single guard, however that
 * should be more then we ever care about as we usually only care if more then
 * one mutation has occured.
 *
 * When the guard goes out of scope it will adjust sMutationCount so that over
 * the lifetime of the guard the guard itself has not affected sMutationCount,
 * while mutations that happened while the guard was alive still will. This
 * allows a guard to be instantiated even if there is another guard higher up
 * on the callstack watching for mutations.
 *
 * The only thing that has to be avoided is for an outer guard to be used
 * while an inner guard is alive. This can be avoided by only ever
 * instantiating a single guard per scope and only using the guard in the
 * current scope.
 */
class nsMutationGuard {
public:
  nsMutationGuard()
  {
    mDelta = eMaxMutations - sMutationCount;
    sMutationCount = eMaxMutations;
  }
  ~nsMutationGuard()
  {
    sMutationCount =
      mDelta > sMutationCount ? 0 : sMutationCount - mDelta;
  }

  /**
   * Returns true if any unexpected mutations have occured. You can pass in
   * an 8-bit ignore count to ignore a number of expected mutations.
   */
  PRBool Mutated(PRUint8 aIgnoreCount)
  {
    return sMutationCount < NS_STATIC_CAST(PRUint32, eMaxMutations - aIgnoreCount);
  }

  // This function should be called whenever a mutation that we want to keep
  // track of happen. For now this is only done when children are added or
  // removed, but we might do it for attribute changes too in the future.
  static void DidMutate()
  {
    if (sMutationCount) {
      --sMutationCount;
    }
  }

private:
  // mDelta is the amount sMutationCount was adjusted when the guard was
  // initialized. It is needed so that we can undo that adjustment once
  // the guard dies.
  PRUint32 mDelta;

  // The value 300 is not important, as long as it is bigger then anything
  // ever passed to Mutated().
  enum { eMaxMutations = 300 };

  
  // sMutationCount is a global mutation counter which is decreased by one at
  // every mutation. It is capped at 0 to avoid wrapping.
  // It's value is always between 0 and 300, inclusive.
  static PRUint32 sMutationCount;
};

/**
 * A generic base class for DOM elements, implementing many nsIContent,
 * nsIDOMNode and nsIDOMElement methods.
 */
class nsGenericElement : public nsIContent
{
public:
  nsGenericElement(nsINodeInfo *aNodeInfo);
  virtual ~nsGenericElement();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

  /**
   * Called during QueryInterface to give the binding manager a chance to
   * get an interface for this element.
   */
  nsresult PostQueryInterface(REFNSIID aIID, void** aInstancePtr);

  // nsINode interface methods
  virtual PRUint32 GetChildCount() const;
  virtual nsIContent *GetChildAt(PRUint32 aIndex) const;
  virtual PRInt32 IndexOf(nsINode* aPossibleChild) const;
  virtual nsresult InsertChildAt(nsIContent* aKid, PRUint32 aIndex,
                                 PRBool aNotify);
  virtual nsresult RemoveChildAt(PRUint32 aIndex, PRBool aNotify);
  virtual nsresult PreHandleEvent(nsEventChainPreVisitor& aVisitor);
  virtual nsresult PostHandleEvent(nsEventChainPostVisitor& aVisitor);
  virtual nsresult DispatchDOMEvent(nsEvent* aEvent, nsIDOMEvent* aDOMEvent,
                                    nsPresContext* aPresContext,
                                    nsEventStatus* aEventStatus);
  NS_IMETHOD GetListenerManager(PRBool aCreateIfNotFound,
                                nsIEventListenerManager** aResult);

  // nsIContent interface methods
  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              PRBool aCompileEventHandlers);
  virtual void UnbindFromTree(PRBool aDeep = PR_TRUE,
                              PRBool aNullParent = PR_TRUE);
  virtual nsIAtom *GetIDAttributeName() const;
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
  virtual void SetFocus(nsPresContext* aContext);
  virtual nsIContent *GetBindingParent() const;
  virtual PRBool IsNodeOfType(PRUint32 aFlags) const;
  virtual already_AddRefed<nsIURI> GetBaseURI() const;
  virtual PRBool IsLink(nsIURI** aURI) const;
  virtual void SetMayHaveFrame(PRBool aMayHaveFrame);
  virtual PRBool MayHaveFrame() const;

  virtual PRUint32 GetScriptTypeID() const;
  virtual nsresult SetScriptTypeID(PRUint32 aLang);

#ifdef DEBUG
  virtual void List(FILE* out, PRInt32 aIndent) const
  {
    List(out, aIndent, EmptyCString());
  }
  virtual void DumpContent(FILE* out, PRInt32 aIndent, PRBool aDumpAll) const;
  void List(FILE* out, PRInt32 aIndent, const nsCString& aPrefix) const;
  void ListAttributes(FILE* out) const;
#endif

  virtual nsIAtom* GetID() const;
  virtual const nsAttrValue* GetClasses() const;
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
  NS_IMETHOD GetChildNodes(nsIDOMNodeList** aChildNodes);
  NS_IMETHOD HasChildNodes(PRBool* aHasChildNodes);
  NS_IMETHOD GetFirstChild(nsIDOMNode** aFirstChild);
  NS_IMETHOD GetLastChild(nsIDOMNode** aLastChild);
  NS_IMETHOD InsertBefore(nsIDOMNode* aNewChild, nsIDOMNode* aRefChild,
                          nsIDOMNode** aReturn);
  NS_IMETHOD ReplaceChild(nsIDOMNode* aNewChild, nsIDOMNode* aOldChild,
                          nsIDOMNode** aReturn);
  NS_IMETHOD RemoveChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn);
  NS_IMETHOD AppendChild(nsIDOMNode* aNewChild, nsIDOMNode** aReturn)
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
   * Trigger a link with uri aLinkURI.  If aClick is false, this triggers a
   * mouseover on the link, otherwise it triggers a load, after doing a
   * security check.  The node principal of |this| is used for the security
   * check.
   *
   * @param aPresContext the pres context.
   * @param aLinkURI the URI of the link
   * @param aTargetSpec the target (like target=, may be empty)
   * @param aClick whether this was a click or not (if false, it assumes you
   *        just hovered over the link)
   * @param aIsUserTriggered whether the user triggered the link.
   *        This would be false for loads from auto XLinks or from the
   *        click() method if we ever implement it.
   */
  nsresult TriggerLink(nsPresContext* aPresContext,
                       nsIURI* aLinkURI,
                       const nsAFlatString& aTargetSpec,
                       PRBool aClick,
                       PRBool aIsUserTriggered);
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

  static nsresult InternalGetFeature(nsISupports* aObject,
                                     const nsAString& aFeature,
                                     const nsAString& aVersion,
                                     nsISupports** aReturn);
  
  static already_AddRefed<nsIDOMNSFeatureFactory>
    GetDOMFeatureFactory(const nsAString& aFeature, const nsAString& aVersion);

  static PRBool ShouldFocus(nsIContent *aContent);

  static PRBool ShouldBlur(nsIContent *aContent);

  /**
   * Actual implementation of the DOM InsertBefore and ReplaceChild methods.
   * Shared by nsDocument. When called from nsDocument, aParent will be null.
   *
   * @param aReplace  True if aNewChild should replace aRefChild. False if
   *                  aNewChild should be inserted before aRefChild.
   * @param aNewChild The child to insert
   * @param aRefChild The child to insert before or replace
   * @param aParent The parent to use for the new child
   * @param aDocument The document to use for the new child.
   *                  Must be non-null, if aParent is null and must match
   *                  aParent->GetCurrentDoc() if aParent is not null.
   * @param aReturn [out] the child we insert
   */
  static nsresult doReplaceOrInsertBefore(PRBool aReplace, nsIDOMNode* aNewChild, nsIDOMNode* aRefChild,
                                          nsIContent* aParent, nsIDocument* aDocument,
                                          nsIDOMNode** aReturn);

  /**
   * Actual implementation of the DOM RemoveChild method.  Shared by
   * nsDocument.  When called from nsDocument, aParent will be null.
   *
   * @param aOldChild The child to remove
   * @param aParent The parent to use for the new child
   * @param aDocument The document to use for the new child.
   *                  Must be non-null if aParent is null and must match
   *                  aParent->GetCurrentDoc() if aParent is not null.
   * @param aReturn [out] the child we remove
   */
  static nsresult doRemoveChild(nsIDOMNode* aOldChild,
                                nsIContent* aParent, nsIDocument* aDocument,
                                nsIDOMNode** aReturn);

  /**
   * Most of the implementation of the nsINode InsertChildAt method.  Shared by
   * nsDocument.  When called from nsDocument, aParent will be null.
   *
   * @param aKid The child to insert.
   * @param aIndex The index to insert at.
   * @param aNotify Whether to notify.
   * @param aParent The parent to use for the new child.
   * @param aDocument The document to use for the notifications.  Must be
   *                  non-null if aParent is null (in which case aKid is being
   *                  inserted as its child) and must match
   *                  aParent->GetCurrentDoc() if aParent is not null.
   * @param aChildArray The child array to work with
   */
  static nsresult doInsertChildAt(nsIContent* aKid, PRUint32 aIndex,
                                  PRBool aNotify, nsIContent* aParent,
                                  nsIDocument* aDocument,
                                  nsAttrAndChildArray& aChildArray);

  /**
   * Most of the implementation of the nsINode RemoveChildAt method.  Shared by
   * nsDocument.  When called from nsDocument, aParent will be null.
   *
   * @param aIndex The index to remove at.
   * @param aNotify Whether to notify.
   * @param aKid The kid at aIndex.  Must not be null.
   * @param aParent The parent we're removing from.
   * @param aDocument The document to use for the notifications.  Must be
   *                  non-null if aParent is null (in which case aKid is being
   *                  removed as its child) and must match
   *                  aParent->GetCurrentDoc() if aParent is not null.
   * @param aChildArray The child array to work with
   */
  static nsresult doRemoveChildAt(PRUint32 aIndex, PRBool aNotify,
                                  nsIContent* aKid, nsIContent* aParent,
                                  nsIDocument* aDocument,
                                  nsAttrAndChildArray& aChildArray);

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
   * Get the primary frame for this content without flushing (see
   * GetPrimaryFrameFor)
   *
   * @return the primary frame 
   */
  nsIFrame* GetPrimaryFrame();

  /**
   * Get the primary frame for this content with flushing (see
   * GetPrimaryFrameFor).
   *
   * @param aType the kind of flush to do, typically Flush_Frames or
   *              Flush_Layout
   * @return the primary frame
   */
  nsIFrame* GetPrimaryFrame(mozFlushType aType);

  /**
   * Get the primary frame for a piece of content without flushing.
   *
   * @param aContent the content to get the primary frame for
   * @param aDocument the document for this content
   * @return the primary frame
   */
  static nsIFrame* GetPrimaryFrameFor(nsIContent* aContent,
                                      nsIDocument* aDocument);

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

  NS_DECL_CYCLE_COLLECTION_CLASS(nsGenericElement)

protected:
  /**
   * Set attribute and (if needed) notify documentobservers and fire off
   * mutation events.
   *
   * @param aNamespaceID  namespace of attribute
   * @param aAttribute    local-name of attribute
   * @param aPrefix       aPrefix of attribute
   * @param aOldValue     previous value of attribute. Only needed if
   *                      aFireMutation is true.
   * @param aParsedValue  parsed new value of attribute
   * @param aModification is this a attribute-modification or addition. Only
   *                      needed if aFireMutation or aNotify is true.
   * @param aFireMutation should mutation-events be fired?
   * @param aNotify       should we notify document-observers?
   */
  nsresult SetAttrAndNotify(PRInt32 aNamespaceID,
                            nsIAtom* aName,
                            nsIAtom* aPrefix,
                            const nsAString& aOldValue,
                            nsAttrValue& aParsedValue,
                            PRBool aModification,
                            PRBool aFireMutation,
                            PRBool aNotify);

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
   * we're actually doing an attr set and will be called before ParseAttribute
   * and hence before we've set the new value.
   *
   * @param aNamespaceID the namespace of the attr being set
   * @param aName the localname of the attribute being set
   * @param aValue the value it's being set to.  If null, the attr is being
   *        removed.
   * // XXXbz we don't actually call this method when we're removing attrs yet.
   *          But we will eventually.
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
   * SetAndTakeAttr (that is, after we have actually set the attr).
   *
   * @param aNamespaceID the namespace of the attr being set
   * @param aName the localname of the attribute being set
   * @param aValue the value it's being set to.  If null, the attr is being
   *        removed.
   * // XXXbz we don't actually call this method when we're removing attrs yet.
   *          But we will eventually.
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
   * Get the attr info for the given namespace ID and attribute name.  The
   * namespace ID must not be kNameSpaceID_Unknown and the name must not be
   * null.  Note that this can only return info on attributes that actually
   * live on this element (and is only virtual to handle XUL prototypes).  That
   * is, this should only be called from methods that only care about attrs
   * that effectively live in mAttrsAndChildren.
   */
  virtual nsAttrInfo GetAttrInfo(PRInt32 aNamespaceID, nsIAtom* aName) const;

  /**
   * Copy attributes and state to another element
   * @param aDest the object to copy to
   */
  nsresult CopyInnerTo(nsGenericElement* aDest) const;

  /**
   * Internal hook for converting an attribute name-string to an atomized name
   */
  virtual const nsAttrName* InternalGetExistingAttrNameFromQName(const nsAString& aStr) const;

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
    nsRefPtr<nsDOMCSSDeclaration> mStyle;

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
     * Weak reference to this node
     */
    nsNodeWeakReference* mWeakReference;
  };

protected:
  // Override from nsINode
  virtual nsINode::nsSlots* CreateSlots();

  nsDOMSlots *GetDOMSlots()
  {
    return NS_STATIC_CAST(nsDOMSlots*, GetSlots());
  }

  nsDOMSlots *GetExistingDOMSlots() const
  {
    return NS_STATIC_CAST(nsDOMSlots*, GetExistingSlots());
  }

  /**
   * GetContentsAsText will take all the textnodes that are children
   * of |this| and concatenate the text in them into aText.  It
   * completely ignores any non-text-node children of |this|; in
   * particular it does not descend into any children of |this| that
   * happen to be container elements.
   *
   * @param aText the resulting text [OUT]
   */
  void GetContentsAsText(nsAString& aText);

  /**
   * Unified function to carry out event default actions for links of all types
   * (HTML links, XLinks, SVG "XLinks", etc.)
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
                                                                            \
  _elementName *it = new _elementName(aNodeInfo);                           \
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
                                                                            \
  _elementName *it = new _elementName(aNodeInfo);                           \
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

#endif /* nsGenericElement_h___ */
