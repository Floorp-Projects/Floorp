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

class nsIDOMAttr;
class nsIDOMEventListener;
class nsIFrame;
class nsISupportsArray;
class nsDOMCSSDeclaration;
class nsIDOMCSSStyleDeclaration;
class nsDOMAttributeMap;
class nsIURI;
class nsINodeInfo;

// Class that holds the child list of a content element and also
// implements the nsIDOMNodeList interface.
class nsChildContentList : public nsGenericDOMNodeList 
{
public:
  nsChildContentList(nsIContent *aContent);
  virtual ~nsChildContentList();

  // nsIDOMNodeList interface
  NS_DECL_NSIDOMNODELIST
  
  void DropReference();

private:
  nsIContent *mContent;
};

class nsCheapVoidArray {
public:
  nsCheapVoidArray();
  ~nsCheapVoidArray();

  PRInt32 Count() const;
  void* ElementAt(PRInt32 aIndex) const;
  PRInt32 IndexOf(void* aPossibleElement) const;
  PRBool InsertElementAt(void* aElement, PRInt32 aIndex);
  PRBool ReplaceElementAt(void* aElement, PRInt32 aIndex);
  PRBool AppendElement(void* aElement);
  PRBool RemoveElement(void* aElement);
  PRBool RemoveElementAt(PRInt32 aIndex);
  void Compact();
  void Clear();

  void* operator[](PRInt32 aIndex) const { return ElementAt(aIndex); }

private:
  typedef unsigned long PtrBits;

  PRBool HasSingleChild() const
  {
    return (mChildren && (PtrBits(mChildren) & 0x1));
  }
  void* GetSingleChild() const
  {
    return (mChildren ? ((void*)(PtrBits(mChildren) & ~0x1)) : nsnull);
  }
  void SetSingleChild(void *aChild);
  nsVoidArray* GetChildVector() const
  {
    return (nsVoidArray*)mChildren;
  }
  nsVoidArray* SwitchToVector();

  // A tagged pointer that's either a pointer to a single child
  // or a pointer to a vector of multiple children. This is a space
  // optimization since a large number of containers have only a 
  // single child.
  void *mChildren;  
};

// There are a set of DOM- and scripting-specific instance variables
// that may only be instantiated when a content object is accessed
// through the DOM. Rather than burn actual slots in the content
// objects for each of these instance variables, we put them off
// in a side structure that's only allocated when the content is
// accessed through the DOM.
typedef struct {
  nsChildContentList *mChildNodes;
  nsDOMCSSDeclaration *mStyle;
  nsDOMAttributeMap* mAttributeMap;
  nsVoidArray *mRangeList;
  nsIEventListenerManager* mListenerManager;
  nsIContent* mBindingParent; // The nearest enclosing content node with a binding
                              // that created us. [Weak]
} nsDOMSlots;


class nsNode3Tearoff : public nsIDOM3Node
{
  NS_DECL_ISUPPORTS

  NS_DECL_NSIDOM3NODE

  nsNode3Tearoff(nsIContent *aContent) : mContent(aContent)
  {
    NS_INIT_ISUPPORTS();
  }
  virtual ~nsNode3Tearoff() {};

private:
  nsCOMPtr<nsIContent> mContent;
};


// nsDOMEventRTTearoff is a tearoff class used by nsGenericElement and
// nsGenericDOMDataNode classes for implemeting the interfaces
// nsIDOMEventReceiver and nsIDOMEventTarget

#define NS_EVENT_TEAROFF_CACHE_SIZE 4

class nsDOMEventRTTearoff : public nsIDOMEventReceiver
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

  // This method gets called by Release() when it's time to delete the
  // this object, in stead of always deleting the object we'll put the
  // object in the cache if unless the cache is already full.
  void LastRelease();

  nsresult GetEventReceiver(nsIDOMEventReceiver **aReceiver);

public:
  virtual ~nsDOMEventRTTearoff();

  // Use this static method to create instances of this tearoff class.
  static nsDOMEventRTTearoff *Create(nsIContent *aContent);

  static void Shutdown();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIDOMEventTarget
  NS_DECL_NSIDOMEVENTTARGET

  // nsIDOMEventReceiver
  NS_IMETHOD AddEventListenerByIID(nsIDOMEventListener *aListener,
                                   const nsIID& aIID);
  NS_IMETHOD RemoveEventListenerByIID(nsIDOMEventListener *aListener,
                                      const nsIID& aIID);
  NS_IMETHOD GetListenerManager(nsIEventListenerManager** aResult);
  NS_IMETHOD HandleEvent(nsIDOMEvent *aEvent);

private:
  // Strong reference back to the content object from where an
  // instance of this class was 'torn off'
  nsCOMPtr<nsIContent> mContent;
};


class nsGenericElement : public nsIHTMLContent
{
public:
  nsGenericElement();
  virtual ~nsGenericElement();

  NS_DECL_ISUPPORTS

  nsresult Init(nsINodeInfo *aNodeInfo);

  // Free globals, to be called from module destructor
  static void Shutdown();

  // nsIContent interface methods
  NS_IMETHOD GetDocument(nsIDocument*& aResult) const;
  NS_IMETHOD SetDocument(nsIDocument* aDocument, PRBool aDeep,
                         PRBool aCompileEventHandlers);
  NS_IMETHOD GetParent(nsIContent*& aResult) const;
  NS_IMETHOD SetParent(nsIContent* aParent);
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
  // NS_IMETHOD NormalizeAttrString(const nsAReadableString& aStr,
  //                                nsINodeInfo*& aNodeInfo);
  // NS_IMETHOD SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName, 
  //                    const nsAReadableString& aValue,
  //                    PRBool aNotify);
  // NS_IMETHOD SetAttr(nsINodeInfo* aNodeInfo,
  //                    const nsAReadableString& aValue,
  //                    PRBool aNotify);
  // NS_IMETHOD GetAttr(PRInt32 aNameSpaceID, nsIAtom* aName, 
  //                    nsAWritableString& aResult) const;
  // NS_IMETHOD GetAttr(PRInt32 aNameSpaceID, nsIAtom* aName, 
  //                    nsIAtom*& aPrefix,
  //                    nsAWritableString& aResult) const;
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
  NS_IMETHOD RangeAdd(nsIDOMRange& aRange);
  NS_IMETHOD RangeRemove(nsIDOMRange& aRange);
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


  // nsIStyledContent interface methods
  NS_IMETHOD GetID(nsIAtom*& aResult) const;
  NS_IMETHOD GetClasses(nsVoidArray& aArray) const;
  NS_IMETHOD HasClass(nsIAtom* aClass, PRBool aCaseSensitive) const;
  NS_IMETHOD WalkContentStyleRules(nsRuleWalker* aRuleWalker);
  NS_IMETHOD WalkInlineStyleRules(nsRuleWalker* aRuleWalker);
  NS_IMETHOD GetMappedAttributeImpact(const nsIAtom* aAttribute, PRInt32 aModType, 
                                      PRInt32& aHint) const;

  // nsIXMLContent interface methods
  NS_IMETHOD SetContainingNameSpace(nsINameSpace* aNameSpace);
  NS_IMETHOD GetContainingNameSpace(nsINameSpace*& aNameSpace) const;
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
                               nsAWritableString& aResult) const;
  NS_IMETHOD StringToAttribute(nsIAtom* aAttribute,
                               const nsAReadableString& aValue,
                               nsHTMLValue& aResult);
  NS_IMETHOD GetBaseURL(nsIURI*& aBaseURL) const;
  NS_IMETHOD GetBaseTarget(nsAWritableString& aBaseTarget) const;

  // nsIDOMNode method implementation
  NS_IMETHOD GetNodeName(nsAWritableString& aNodeName);
  NS_IMETHOD GetLocalName(nsAWritableString& aLocalName);
  NS_IMETHOD GetNodeValue(nsAWritableString& aNodeValue);
  NS_IMETHOD SetNodeValue(const nsAReadableString& aNodeValue);
  NS_IMETHOD GetNodeType(PRUint16* aNodeType);
  NS_IMETHOD GetParentNode(nsIDOMNode** aParentNode);
  NS_IMETHOD GetAttributes(nsIDOMNamedNodeMap** aAttributes);
  NS_IMETHOD GetPreviousSibling(nsIDOMNode** aPreviousSibling);
  NS_IMETHOD GetNextSibling(nsIDOMNode** aNextSibling);
  NS_IMETHOD GetOwnerDocument(nsIDOMDocument** aOwnerDocument);
  NS_IMETHOD GetNamespaceURI(nsAWritableString& aNamespaceURI);
  NS_IMETHOD GetPrefix(nsAWritableString& aPrefix);
  NS_IMETHOD SetPrefix(const nsAReadableString& aPrefix);
  NS_IMETHOD Normalize();
  NS_IMETHOD IsSupported(const nsAReadableString& aFeature,
                         const nsAReadableString& aVersion, PRBool* aReturn);
  NS_IMETHOD HasAttributes(PRBool* aHasAttributes);

  // nsIDOMElement method implementation
  NS_IMETHOD GetTagName(nsAWritableString& aTagName);
  NS_IMETHOD GetAttribute(const nsAReadableString& aName,
                          nsAWritableString& aReturn);
  NS_IMETHOD SetAttribute(const nsAReadableString& aName,
                          const nsAReadableString& aValue);
  NS_IMETHOD RemoveAttribute(const nsAReadableString& aName);
  NS_IMETHOD GetAttributeNode(const nsAReadableString& aName,
                              nsIDOMAttr** aReturn);
  NS_IMETHOD SetAttributeNode(nsIDOMAttr* aNewAttr, nsIDOMAttr** aReturn);
  NS_IMETHOD RemoveAttributeNode(nsIDOMAttr* aOldAttr, nsIDOMAttr** aReturn);
  NS_IMETHOD GetElementsByTagName(const nsAReadableString& aTagname,
                                  nsIDOMNodeList** aReturn);
  NS_IMETHOD GetAttributeNS(const nsAReadableString& aNamespaceURI,
                            const nsAReadableString& aLocalName,
                            nsAWritableString& aReturn);
  NS_IMETHOD SetAttributeNS(const nsAReadableString& aNamespaceURI,
                            const nsAReadableString& aQualifiedName,
                            const nsAReadableString& aValue);
  NS_IMETHOD RemoveAttributeNS(const nsAReadableString& aNamespaceURI,
                               const nsAReadableString& aLocalName);
  NS_IMETHOD GetAttributeNodeNS(const nsAReadableString& aNamespaceURI,
                                const nsAReadableString& aLocalName,
                                nsIDOMAttr** aReturn);
  NS_IMETHOD SetAttributeNodeNS(nsIDOMAttr* aNewAttr, nsIDOMAttr** aReturn);
  NS_IMETHOD GetElementsByTagNameNS(const nsAReadableString& aNamespaceURI,
                                    const nsAReadableString& aLocalName,
                                    nsIDOMNodeList** aReturn);
  NS_IMETHOD HasAttribute(const nsAReadableString& aName, PRBool* aReturn);
  NS_IMETHOD HasAttributeNS(const nsAReadableString& aNamespaceURI,
                            const nsAReadableString& aLocalName,
                            PRBool* aReturn);

  // Generic DOMNode implementations
  nsresult  doInsertBefore(nsIDOMNode* aNewChild, nsIDOMNode* aRefChild,
                           nsIDOMNode** aReturn);
  nsresult  doReplaceChild(nsIDOMNode* aNewChild, nsIDOMNode* aOldChild,
                           nsIDOMNode** aReturn);
  nsresult  doRemoveChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn);

  //----------------------------------------

  nsresult RenderFrame(nsIPresContext*);

  nsresult AddScriptEventListener(nsIAtom* aAttribute,
                                  const nsAReadableString& aValue);

  nsresult TriggerLink(nsIPresContext* aPresContext,
                       nsLinkVerb aVerb,
                       nsIURI* aBaseURL,
                       const nsString& aURLSpec,
                       const nsString& aTargetSpec,
                       PRBool aClick);

  nsresult JoinTextNodes(nsIContent* aFirst,
                         nsIContent* aSecond);

  static void SetDocumentInChildrenOf(nsIContent* aContent, 
				      nsIDocument* aDocument, PRBool aCompileEventHandlers);

  static nsresult InternalIsSupported(const nsAReadableString& aFeature,
                                      const nsAReadableString& aVersion,
                                      PRBool* aReturn);

  static PRBool HasMutationListeners(nsIContent* aContent,
                                     PRUint32 aType);

protected:
#ifdef DEBUG
  virtual PRUint32 BaseSizeOf(nsISizeOfHandler *aSizer) const;
#endif

  nsDOMSlots *GetDOMSlots();
  void MaybeClearDOMSlots();

  nsIDocument* mDocument;                   // WEAK
  nsIContent* mParent;                      // WEAK
  
  nsINodeInfo* mNodeInfo;                   // OWNER
  nsDOMSlots *mDOMSlots;                    // OWNER
  PRUint32 mContentID;
};

class nsGenericContainerElement : public nsGenericElement {
public:
  nsGenericContainerElement();
  virtual ~nsGenericContainerElement();

  NS_IMETHOD CopyInnerTo(nsIContent* aSrcContent,
                       nsGenericContainerElement* aDest,
                       PRBool aDeep);

  // nsIDOMElement methods
  NS_METHOD GetAttribute(const nsAReadableString& aName,
                        nsAWritableString& aReturn) 
  {
    return nsGenericElement::GetAttribute(aName, aReturn);
  }
  NS_METHOD SetAttribute(const nsAReadableString& aName,
                        const nsAReadableString& aValue)
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
  NS_IMETHOD NormalizeAttrString(const nsAReadableString& aStr,
                                 nsINodeInfo*& aNodeInfo);
  NS_IMETHOD SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                          const nsAReadableString& aValue,
                          PRBool aNotify);
  NS_IMETHOD SetAttr(nsINodeInfo* aNodeInfo,
                     const nsAReadableString& aValue,
                     PRBool aNotify);
  NS_IMETHOD GetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                     nsAWritableString& aResult) const;
  NS_IMETHOD GetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                     nsIAtom*& aPrefix, nsAWritableString& aResult) const;
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

  nsVoidArray* mAttributes;
  nsCheapVoidArray mChildren;
};


// Internal non-public interface

// IID for the nsIDocumentFragment interface
#define NS_IDOCUMENTFRAGMENT_IID      \
{ 0xd8fb2853, 0xf6d6, 0x4499, \
  {0x9c, 0x60, 0x6c, 0xa2, 0x75, 0x35, 0x09, 0xeb} }

// nsIDocumentFragment interface
class nsIDocumentFragment : public nsIDOMDocumentFragment
{
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IDOCUMENTFRAGMENT_IID)

  // These methods are supposed to be used when *all* children of a
  // document fragment are moved at once into a new parent w/o
  // changing the relationship between the children. If the moving
  // operation fails and some children were moved to a new parent and
  // some weren't, ReconnectChildren() should be called to remove the
  // children from their possible new parent and re-insert the
  // children into the document fragment. Once the operation is
  // complete and all children are successfully moved into their new
  // parent DropChildReferences() should be called so that the
  // document fragment will loose its references to the children.

  NS_IMETHOD DisconnectChildren() = 0;
  NS_IMETHOD ReconnectChildren() = 0;
  NS_IMETHOD DropChildReferences() = 0;
};


#define NS_FORWARD_NSIDOMNODE_NO_CLONENODE(_to)  \
  NS_IMETHOD    GetNodeName(nsAWritableString& aNodeName) { return _to GetNodeName(aNodeName); } \
  NS_IMETHOD    GetNodeValue(nsAWritableString& aNodeValue) { return _to GetNodeValue(aNodeValue); } \
  NS_IMETHOD    SetNodeValue(const nsAReadableString& aNodeValue) { return _to SetNodeValue(aNodeValue); } \
  NS_IMETHOD    GetNodeType(PRUint16* aNodeType) { return _to GetNodeType(aNodeType); } \
  NS_IMETHOD    GetParentNode(nsIDOMNode** aParentNode) { return _to GetParentNode(aParentNode); } \
  NS_IMETHOD    GetChildNodes(nsIDOMNodeList** aChildNodes) { return _to GetChildNodes(aChildNodes); } \
  NS_IMETHOD    GetFirstChild(nsIDOMNode** aFirstChild) { return _to GetFirstChild(aFirstChild); } \
  NS_IMETHOD    GetLastChild(nsIDOMNode** aLastChild) { return _to GetLastChild(aLastChild); } \
  NS_IMETHOD    GetPreviousSibling(nsIDOMNode** aPreviousSibling) { return _to GetPreviousSibling(aPreviousSibling); } \
  NS_IMETHOD    GetNextSibling(nsIDOMNode** aNextSibling) { return _to GetNextSibling(aNextSibling); } \
  NS_IMETHOD    GetAttributes(nsIDOMNamedNodeMap** aAttributes) { return _to GetAttributes(aAttributes); } \
  NS_IMETHOD    GetOwnerDocument(nsIDOMDocument** aOwnerDocument) { return _to GetOwnerDocument(aOwnerDocument); } \
  NS_IMETHOD    GetNamespaceURI(nsAWritableString& aNamespaceURI) { return _to GetNamespaceURI(aNamespaceURI); } \
  NS_IMETHOD    GetPrefix(nsAWritableString& aPrefix) { return _to GetPrefix(aPrefix); } \
  NS_IMETHOD    SetPrefix(const nsAReadableString& aPrefix) { return _to SetPrefix(aPrefix); } \
  NS_IMETHOD    GetLocalName(nsAWritableString& aLocalName) { return _to GetLocalName(aLocalName); } \
  NS_IMETHOD    InsertBefore(nsIDOMNode* aNewChild, nsIDOMNode* aRefChild, nsIDOMNode** aReturn) { return _to InsertBefore(aNewChild, aRefChild, aReturn); }  \
  NS_IMETHOD    ReplaceChild(nsIDOMNode* aNewChild, nsIDOMNode* aOldChild, nsIDOMNode** aReturn) { return _to ReplaceChild(aNewChild, aOldChild, aReturn); }  \
  NS_IMETHOD    RemoveChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn) { return _to RemoveChild(aOldChild, aReturn); }  \
  NS_IMETHOD    AppendChild(nsIDOMNode* aNewChild, nsIDOMNode** aReturn) { return _to AppendChild(aNewChild, aReturn); }  \
  NS_IMETHOD    HasChildNodes(PRBool* aReturn) { return _to HasChildNodes(aReturn); }  \
  NS_IMETHOD    CloneNode(PRBool aDeep, nsIDOMNode** aReturn);  \
  NS_IMETHOD    Normalize() { return _to Normalize(); }  \
  NS_IMETHOD    IsSupported(const nsAReadableString& aFeature, const nsAReadableString& aVersion, PRBool* aReturn) { return _to IsSupported(aFeature, aVersion, aReturn); }  \
  NS_IMETHOD    HasAttributes(PRBool* aReturn) { return _to HasAttributes(aReturn); }

#define NS_INTERFACE_MAP_ENTRY_TEAROFF(_iid, _tearoff)                        \
  if (aIID.Equals(NS_GET_IID(_iid))) {                                        \
    foundInterface = new _tearoff;                                            \
    NS_ENSURE_TRUE(foundInterface, NS_ERROR_OUT_OF_MEMORY);                   \
  } else

#endif /* nsGenericElement_h___ */
