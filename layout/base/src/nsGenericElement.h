/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#ifndef nsGenericElement_h___
#define nsGenericElement_h___

#include "nsCOMPtr.h"
#include "nsIContent.h"
#include "nsIDOMAttr.h"
#include "nsIDOMNamedNodeMap.h"
#include "nsIDOMElement.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMLinkStyle.h"
#include "nsIStyleSheetLinkingElement.h"
#include "nsICSSStyleSheet.h"
#include "nsICSSLoaderObserver.h"
#include "nsVoidArray.h"
#include "nsIScriptObjectOwner.h"
#include "nsIJSScriptObject.h"
#include "nsILinkHandler.h"
#include "nsGenericDOMNodeList.h"
#include "nsIEventListenerManager.h"
#include "nsINodeInfo.h"
#include "nsIParser.h"

class nsIDOMAttr;
class nsIDOMEventListener;
class nsIFrame;
class nsISupportsArray;
class nsIDOMScriptObjectFactory;
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
  NS_DECL_IDOMNODELIST
  
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

private:
  typedef unsigned long PtrBits;

  PRBool HasSingleChild() const { return (mChildren && (PtrBits(mChildren) & 0x1)); }
  void* GetSingleChild() const { return (mChildren ? ((void*)(PtrBits(mChildren) & ~0x1)) : nsnull); }
  void SetSingleChild(void *aChild);
  nsVoidArray* GetChildVector() const { return (nsVoidArray*)mChildren; }
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
  void *mScriptObject;
  nsChildContentList *mChildNodes;
  nsDOMCSSDeclaration *mStyle;
  nsDOMAttributeMap* mAttributeMap;
  nsVoidArray *mRangeList;
  nsIContent* mCapturer;
  nsIEventListenerManager* mListenerManager;
  nsIContent* mBindingParent; // The nearest enclosing content node with a binding
                              // that created us. [Weak]
} nsDOMSlots;

class nsGenericElement {
public:
  nsGenericElement();
  ~nsGenericElement();

  void Init(nsIContent* aOuterContentObject, nsINodeInfo *aNodeInfo);

  // Implementation for nsIDOMNode
  nsresult    GetNodeValue(nsAWritableString& aNodeValue);
  nsresult    SetNodeValue(const nsAReadableString& aNodeValue);
  nsresult    GetNodeType(PRUint16* aNodeType);
  nsresult    GetParentNode(nsIDOMNode** aParentNode);
  nsresult    GetAttributes(nsIDOMNamedNodeMap** aAttributes);
  nsresult    GetPreviousSibling(nsIDOMNode** aPreviousSibling);
  nsresult    GetNextSibling(nsIDOMNode** aNextSibling);
  nsresult    GetOwnerDocument(nsIDOMDocument** aOwnerDocument);
  nsresult    GetNamespaceURI(nsAWritableString& aNamespaceURI);
  nsresult    GetPrefix(nsAWritableString& aPrefix);
  nsresult    SetPrefix(const nsAReadableString& aPrefix);
  nsresult    Normalize();
  nsresult    IsSupported(const nsAReadableString& aFeature,
                          const nsAReadableString& aVersion, PRBool* aReturn);
  nsresult    HasAttributes(PRBool* aHasAttributes);

  // Implementation for nsIDOMElement
  nsresult    GetTagName(nsAWritableString& aTagName);
  nsresult    GetAttribute(const nsAReadableString& aName, nsAWritableString& aReturn);
  nsresult    SetAttribute(const nsAReadableString& aName, const nsAReadableString& aValue);
  nsresult    RemoveAttribute(const nsAReadableString& aName);
  nsresult    GetAttributeNode(const nsAReadableString& aName,
                               nsIDOMAttr** aReturn);
  nsresult    SetAttributeNode(nsIDOMAttr* aNewAttr, nsIDOMAttr** aReturn);
  nsresult    RemoveAttributeNode(nsIDOMAttr* aOldAttr, nsIDOMAttr** aReturn);
  nsresult    GetElementsByTagName(const nsAReadableString& aTagname,
                                   nsIDOMNodeList** aReturn);
  nsresult    GetAttributeNS(const nsAReadableString& aNamespaceURI,
                             const nsAReadableString& aLocalName, nsAWritableString& aReturn);
  nsresult    SetAttributeNS(const nsAReadableString& aNamespaceURI,
                             const nsAReadableString& aQualifiedName,
                             const nsAReadableString& aValue);
  nsresult    RemoveAttributeNS(const nsAReadableString& aNamespaceURI,
                                const nsAReadableString& aLocalName);
  nsresult    GetAttributeNodeNS(const nsAReadableString& aNamespaceURI,
                                 const nsAReadableString& aLocalName,
                                 nsIDOMAttr** aReturn);
  nsresult    SetAttributeNodeNS(nsIDOMAttr* aNewAttr, nsIDOMAttr** aReturn);
  nsresult    GetElementsByTagNameNS(const nsAReadableString& aNamespaceURI,
                                     const nsAReadableString& aLocalName,
                                     nsIDOMNodeList** aReturn);
  nsresult    HasAttribute(const nsAReadableString& aName, PRBool* aReturn);
  nsresult    HasAttributeNS(const nsAReadableString& aNamespaceURI,
                             const nsAReadableString& aLocalName, PRBool* aReturn);

  // nsIScriptObjectOwner interface
  nsresult GetScriptObject(nsIScriptContext* aContext, void** aScriptObject);
  nsresult SetScriptObject(void *aScriptObject);

  // Implementation for nsIContent
  nsresult GetDocument(nsIDocument*& aResult) const;
  nsresult SetDocument(nsIDocument* aDocument, PRBool aDeep, PRBool aCompileEventHandlers);
  nsresult GetParent(nsIContent*& aResult) const;
  nsresult SetParent(nsIContent* aParent);
  nsresult IsSynthetic(PRBool& aResult) {
    aResult = PR_FALSE;
    return NS_OK;
  }
  nsresult GetNameSpaceID(PRInt32& aNameSpaceID) const;
  nsresult GetTag(nsIAtom*& aResult) const;
  nsresult GetNodeInfo(nsINodeInfo*& aResult) const;
  nsresult HandleDOMEvent(nsIPresContext* aPresContext,
                          nsEvent* aEvent,
                          nsIDOMEvent** aDOMEvent,
                          PRUint32 aFlags,
                          nsEventStatus* aEventStatus);

  nsresult GetContentID(PRUint32* aID) {
    *aID = mContentID;
    return NS_OK;
  }
  nsresult SetContentID(PRUint32 aID) {
    mContentID = aID;
    return NS_OK;
  }

  nsresult RangeAdd(nsIDOMRange& aRange);
  nsresult RangeRemove(nsIDOMRange& aRange);
  nsresult GetRangeList(nsVoidArray*& aResult) const;
  nsresult SetFocus(nsIPresContext* aContext);
  nsresult RemoveFocus(nsIPresContext* aContext);
  nsresult GetBindingParent(nsIContent** aContent);
  nsresult SetBindingParent(nsIContent* aParent);

  nsresult SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult,
                  size_t aInstanceSize) const;
  
  // Implementation for nsIJSScriptObject
  PRBool    AddProperty(JSContext *aContext, JSObject *aObj, 
                        jsval aID, jsval *aVp);
  PRBool    DeleteProperty(JSContext *aContext, JSObject *aObj, 
                        jsval aID, jsval *aVp);
  PRBool    GetProperty(JSContext *aContext, JSObject *aObj, 
                        jsval aID, jsval *aVp);
  PRBool    SetProperty(JSContext *aContext, JSObject *aObj, 
                        jsval aID, jsval *aVp);
  PRBool    EnumerateProperty(JSContext *aContext, JSObject *aObj);
  PRBool    Resolve(JSContext *aContext, JSObject *aObj, jsval aID);
  PRBool    Convert(JSContext *aContext, JSObject *aObj, jsval aID);
  void      Finalize(JSContext *aContext, JSObject *aObj);

  // Generic DOMNode implementations
  nsresult  doInsertBefore(nsIDOMNode* aNewChild, nsIDOMNode* aRefChild,
                           nsIDOMNode** aReturn);
  nsresult  doReplaceChild(nsIDOMNode* aNewChild, nsIDOMNode* aOldChild,
                           nsIDOMNode** aReturn);
  nsresult  doRemoveChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn);

  // Implementation for nsISupports
  nsresult QueryInterface(REFNSIID aIID,
                          void** aInstancePtr);
  nsrefcnt AddRef(void);
  nsrefcnt Release(void);

  //----------------------------------------

  nsresult GetListenerManager(nsIEventListenerManager** aInstancePtrResult);

  nsresult RenderFrame(nsIPresContext*);

  nsresult AddScriptEventListener(nsIAtom* aAttribute,
                                  const nsAReadableString& aValue,
                                  REFNSIID aIID);

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

  static nsresult GetScriptObjectFactory(nsIDOMScriptObjectFactory **aFactory);

  static nsIDOMScriptObjectFactory *gScriptObjectFactory;

  static nsIAtom* CutNameSpacePrefix(nsString& aString);

  static nsresult InternalIsSupported(const nsAReadableString& aFeature,
                                      const nsAReadableString& aVersion,
                                      PRBool* aReturn);

  nsDOMSlots *GetDOMSlots();
  void MaybeClearDOMSlots();

  // Up pointer to the real content object that we are
  // supporting. Sometimes there is work that we just can't do
  // ourselves, so this is needed to ask the real object to do the
  // work.
  nsIContent* mContent;                     // WEAK

  nsIDocument* mDocument;                   // WEAK
  nsIContent* mParent;                      // WEAK
  
  nsINodeInfo* mNodeInfo;                   // OWNER
  nsDOMSlots *mDOMSlots;                    // OWNER
  PRUint32 mContentID;
};

class nsGenericContainerElement : public nsGenericElement {
public:
  nsGenericContainerElement();
  ~nsGenericContainerElement();

  nsresult CopyInnerTo(nsIContent* aSrcContent,
                       nsGenericContainerElement* aDest,
                       PRBool aDeep);

  // Remainder of nsIDOMHTMLElement (and nsIDOMNode)
  nsresult    GetAttribute(const nsAReadableString& aName, nsAWritableString& aReturn) 
  {
    return nsGenericElement::GetAttribute(aName, aReturn);
  }
  nsresult    SetAttribute(const nsAReadableString& aName, const nsAReadableString& aValue)
  {
    return nsGenericElement::SetAttribute(aName, aValue);
  }
  nsresult    GetChildNodes(nsIDOMNodeList** aChildNodes);
  nsresult    HasChildNodes(PRBool* aHasChildNodes);
  nsresult    GetFirstChild(nsIDOMNode** aFirstChild);
  nsresult    GetLastChild(nsIDOMNode** aLastChild);
  
  nsresult    InsertBefore(nsIDOMNode* aNewChild, nsIDOMNode* aRefChild,
                           nsIDOMNode** aReturn)
  {
    return nsGenericElement::doInsertBefore(aNewChild, aRefChild, aReturn);
  }
  nsresult    ReplaceChild(nsIDOMNode* aNewChild, nsIDOMNode* aOldChild,
                           nsIDOMNode** aReturn)
  {
    return nsGenericElement::doReplaceChild(aNewChild, aOldChild, aReturn);
  }
  nsresult    RemoveChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn)
  {
    return nsGenericElement::doRemoveChild(aOldChild, aReturn);
  }
  nsresult    AppendChild(nsIDOMNode* aNewChild, nsIDOMNode** aReturn)
  {
    return nsGenericElement::doInsertBefore(aNewChild, nsnull, aReturn);
  }

  // Remainder of nsIContent
  nsresult NormalizeAttributeString(const nsAReadableString& aStr,
                                    nsINodeInfo*& aNodeInfo);
  nsresult SetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName, 
                        const nsAReadableString& aValue,
                        PRBool aNotify);
  nsresult SetAttribute(nsINodeInfo* aNodeInfo, const nsAReadableString& aValue,
                        PRBool aNotify);
  nsresult GetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName, 
                        nsAWritableString& aResult) const;
  nsresult GetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName, 
                        nsIAtom*& aPrefix, nsAWritableString& aResult) const;
  nsresult UnsetAttribute(PRInt32 aNameSpaceID, nsIAtom* aAttribute, 
                          PRBool aNotify);
  nsresult GetAttributeNameAt(PRInt32 aIndex,
                              PRInt32& aNameSpaceID, 
                              nsIAtom*& aName,
                              nsIAtom*& aPrefix) const;
  nsresult GetAttributeCount(PRInt32& aResult) const;
  nsresult List(FILE* out, PRInt32 aIndent) const;
  nsresult DumpContent(FILE* out, PRInt32 aIndent,PRBool aDumpAll) const;
  nsresult CanContainChildren(PRBool& aResult) const;
  nsresult ChildCount(PRInt32& aResult) const;
  nsresult ChildAt(PRInt32 aIndex, nsIContent*& aResult) const;
  nsresult IndexOf(nsIContent* aPossibleChild, PRInt32& aResult) const;
  nsresult InsertChildAt(nsIContent* aKid, PRInt32 aIndex, PRBool aNotify);
  nsresult ReplaceChildAt(nsIContent* aKid, PRInt32 aIndex, PRBool aNotify);
  nsresult AppendChildTo(nsIContent* aKid, PRBool aNotify);
  nsresult RemoveChildAt(PRInt32 aIndex, PRBool aNotify);
  nsresult BeginConvertToXIF(nsIXIFConverter * aConverter) const;
  nsresult ConvertContentToXIF(nsIXIFConverter * aConverter) const;
  nsresult FinishConvertToXIF(nsIXIFConverter * aConverter) const;
  nsresult SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult,
                  size_t aInstanceSize) const;

  void ListAttributes(FILE* out) const;

  nsVoidArray* mAttributes;
  nsCheapVoidArray mChildren;
};


//----------------------------------------------------------------------

/**
 * Mostly implement the nsIDOMNode API by forwarding the methods to a
 * generic content object (either nsGenericHTMLLeafElement or
 * nsGenericHTMLContainerContent)
 *
 * Note that classes using this macro will need to implement:
 *       NS_IMETHOD CloneNode(PRBool aDeep, nsIDOMNode** aReturn);
 */
#define NS_IMPL_IDOMNODE_USING_GENERIC(_g)                              \
  NS_IMETHOD GetNodeName(nsAWritableString& aNodeName) {                \
    return _g.GetNodeName(aNodeName);                                   \
  }                                                                     \
  NS_IMETHOD GetNodeValue(nsAWritableString& aNodeValue) {              \
    return _g.GetNodeValue(aNodeValue);                                 \
  }                                                                     \
  NS_IMETHOD SetNodeValue(const nsAReadableString& aNodeValue) {        \
    return _g.SetNodeValue(aNodeValue);                                 \
  }                                                                     \
  NS_IMETHOD GetNodeType(PRUint16* aNodeType) {                         \
    return _g.GetNodeType(aNodeType);                                   \
  }                                                                     \
  NS_IMETHOD GetParentNode(nsIDOMNode** aParentNode) {                  \
    return _g.GetParentNode(aParentNode);                               \
  }                                                                     \
  NS_IMETHOD GetChildNodes(nsIDOMNodeList** aChildNodes) {              \
    return _g.GetChildNodes(aChildNodes);                               \
  }                                                                     \
  NS_IMETHOD HasChildNodes(PRBool* aHasChildNodes) {                    \
    return _g.HasChildNodes(aHasChildNodes);                            \
  }                                                                     \
  NS_IMETHOD HasAttributes(PRBool* aHasAttributes) {                    \
    return _g.HasAttributes(aHasAttributes);                            \
  }                                                                     \
  NS_IMETHOD GetFirstChild(nsIDOMNode** aFirstChild) {                  \
    return _g.GetFirstChild(aFirstChild);                               \
  }                                                                     \
  NS_IMETHOD GetLastChild(nsIDOMNode** aLastChild) {                    \
    return _g.GetLastChild(aLastChild);                                 \
  }                                                                     \
  NS_IMETHOD GetPreviousSibling(nsIDOMNode** aPreviousSibling) {        \
    return _g.GetPreviousSibling(aPreviousSibling);                     \
  }                                                                     \
  NS_IMETHOD GetNextSibling(nsIDOMNode** aNextSibling) {                \
    return _g.GetNextSibling(aNextSibling);                             \
  }                                                                     \
  NS_IMETHOD GetAttributes(nsIDOMNamedNodeMap** aAttributes) {          \
    return _g.GetAttributes(aAttributes);                               \
  }                                                                     \
  NS_IMETHOD GetNamespaceURI(nsAWritableString& aNamespaceURI) {        \
    return _g.GetNamespaceURI(aNamespaceURI);                           \
  }                                                                     \
  NS_IMETHOD GetPrefix(nsAWritableString& aPrefix) {                    \
    return _g.GetPrefix(aPrefix);                                       \
  }                                                                     \
  NS_IMETHOD SetPrefix(const nsAReadableString& aPrefix) {              \
    return _g.SetPrefix(aPrefix);                                       \
  }                                                                     \
  NS_IMETHOD GetLocalName(nsAWritableString& aLocalName) {              \
    return _g.GetLocalName(aLocalName);                                 \
  }                                                                     \
  NS_IMETHOD InsertBefore(nsIDOMNode* aNewChild, nsIDOMNode* aRefChild, \
                             nsIDOMNode** aReturn) {                    \
    return _g.InsertBefore(aNewChild, aRefChild, aReturn);              \
  }                                                                     \
  NS_IMETHOD AppendChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn) { \
    return _g.AppendChild(aOldChild, aReturn);                          \
  }                                                                     \
  NS_IMETHOD ReplaceChild(nsIDOMNode* aNewChild, nsIDOMNode* aOldChild, \
                             nsIDOMNode** aReturn) {                    \
    return _g.ReplaceChild(aNewChild, aOldChild, aReturn);              \
  }                                                                     \
  NS_IMETHOD RemoveChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn) { \
    return _g.RemoveChild(aOldChild, aReturn);                          \
  }                                                                     \
  NS_IMETHOD GetOwnerDocument(nsIDOMDocument** aOwnerDocument) {        \
    return _g.GetOwnerDocument(aOwnerDocument);                         \
  }                                                                     \
  NS_IMETHOD CloneNode(PRBool aDeep, nsIDOMNode** aReturn);             \
  NS_IMETHOD Normalize() {                                              \
    return _g.Normalize();                                              \
  }                                                                     \
  NS_IMETHOD IsSupported(const nsAReadableString& aFeature,             \
                      const nsAReadableString& aVersion, PRBool* aReturn) { \
    return _g.IsSupported(aFeature, aVersion, aReturn);                 \
  }

/**
 * Implement the nsIDOMElement API by forwarding the methods to a
 * generic content object (either nsGenericHTMLLeafElement or
 * nsGenericHTMLContainerContent)
 */
#define NS_IMPL_IDOMELEMENT_USING_GENERIC(_g)                                 \
  NS_IMETHOD GetTagName(nsAWritableString& aTagName) {                        \
    return _g.GetTagName(aTagName);                                           \
  }                                                                           \
  NS_IMETHOD GetAttribute(const nsAReadableString& aName, nsAWritableString& aReturn) {         \
    return _g.GetAttribute(aName, aReturn);                                   \
  }                                                                           \
  NS_IMETHOD SetAttribute(const nsAReadableString& aName, const nsAReadableString& aValue) {    \
    return _g.SetAttribute(aName, aValue);                                    \
  }                                                                           \
  NS_IMETHOD RemoveAttribute(const nsAReadableString& aName) {                \
    return _g.RemoveAttribute(aName);                                         \
  }                                                                           \
  NS_IMETHOD GetAttributeNode(const nsAReadableString& aName,                 \
                              nsIDOMAttr** aReturn) {                         \
    return _g.GetAttributeNode(aName, aReturn);                               \
  }                                                                           \
  NS_IMETHOD SetAttributeNode(nsIDOMAttr* aNewAttr, nsIDOMAttr** aReturn) {   \
    return _g.SetAttributeNode(aNewAttr, aReturn);                            \
  }                                                                           \
  NS_IMETHOD RemoveAttributeNode(nsIDOMAttr* aOldAttr, nsIDOMAttr** aReturn) {\
    return _g.RemoveAttributeNode(aOldAttr, aReturn);                         \
  }                                                                           \
  NS_IMETHOD GetElementsByTagName(const nsAReadableString& aTagname,          \
                                  nsIDOMNodeList** aReturn) {                 \
    return _g.GetElementsByTagName(aTagname, aReturn);                        \
  }                                                                           \
  NS_IMETHOD GetAttributeNS(const nsAReadableString& aNamespaceURI,           \
                            const nsAReadableString& aLocalName, nsAWritableString& aReturn) {  \
    return _g.GetAttributeNS(aNamespaceURI, aLocalName, aReturn);             \
  }                                                                           \
  NS_IMETHOD SetAttributeNS(const nsAReadableString& aNamespaceURI,           \
                            const nsAReadableString& aQualifiedName,          \
                            const nsAReadableString& aValue) {                \
    return _g.SetAttributeNS(aNamespaceURI, aQualifiedName, aValue);          \
  }                                                                           \
  NS_IMETHOD RemoveAttributeNS(const nsAReadableString& aNamespaceURI,        \
                               const nsAReadableString& aLocalName) {         \
    return _g.RemoveAttributeNS(aNamespaceURI, aLocalName);                   \
  }                                                                           \
  NS_IMETHOD GetAttributeNodeNS(const nsAReadableString& aNamespaceURI,       \
                                const nsAReadableString& aLocalName,          \
                                nsIDOMAttr** aReturn) {                       \
    return _g.GetAttributeNodeNS(aNamespaceURI, aLocalName, aReturn);         \
  }                                                                           \
  NS_IMETHOD SetAttributeNodeNS(nsIDOMAttr* aNewAttr, nsIDOMAttr** aReturn) { \
    return _g.SetAttributeNodeNS(aNewAttr, aReturn);                          \
  }                                                                           \
  NS_IMETHOD GetElementsByTagNameNS(const nsAReadableString& aNamespaceURI,   \
                                    const nsAReadableString& aLocalName,      \
                                    nsIDOMNodeList** aReturn) {               \
    return _g.GetElementsByTagNameNS(aNamespaceURI, aLocalName, aReturn);     \
  }                                                                           \
  NS_IMETHOD HasAttribute(const nsAReadableString& aName, PRBool* aReturn) {  \
    return _g.HasAttribute(aName, aReturn);                                   \
  }                                                                           \
  NS_IMETHOD HasAttributeNS(const nsAReadableString& aNamespaceURI,           \
                            const nsAReadableString& aLocalName, PRBool* aReturn) {    \
    return _g.HasAttributeNS(aNamespaceURI, aLocalName, aReturn);             \
  }

/**
 * Implement the nsIDOMEventReceiver API by forwarding the methods to a
 * generic content object (either nsGenericHTMLLeafElement or
 * nsGenericHTMLContainerContent)
 */
#define NS_IMPL_IDOMEVENTRECEIVER_USING_GENERIC(_g)                             \
  NS_IMETHOD AddEventListenerByIID(nsIDOMEventListener *aListener,              \
                                   const nsIID& aIID) {                         \
    return _g.AddEventListenerByIID(aListener, aIID);                           \
  }                                                                             \
  NS_IMETHOD RemoveEventListenerByIID(nsIDOMEventListener *aListener,           \
                                      const nsIID& aIID) {                      \
    return _g.RemoveEventListenerByIID(aListener, aIID);                        \
  }                                                                             \
  NS_IMETHOD GetListenerManager(nsIEventListenerManager** aResult) {            \
    return _g.GetListenerManager(aResult);                                      \
  }                                                                             \
  NS_IMETHOD GetNewListenerManager(nsIEventListenerManager** aResult) {         \
    return _g.GetNewListenerManager(aResult);                                   \
  }                                                                             \
  NS_IMETHOD HandleEvent(nsIDOMEvent *aEvent) {                                 \
    return _g.HandleEvent(aEvent);                                              \
  }                                                                             \
  NS_IMETHOD AddEventListener(const nsAReadableString& aType,                            \
                              nsIDOMEventListener* aListener,                   \
                              PRBool aUseCapture) {                             \
    return _g.AddEventListener(aType, aListener, aUseCapture);    \
  }                                                                             \
  NS_IMETHOD RemoveEventListener(const nsAReadableString& aType,                         \
                                 nsIDOMEventListener* aListener,                \
                                 PRBool aUseCapture) {                          \
    return _g.RemoveEventListener(aType, aListener, aUseCapture); \
  }                                                                     

#define NS_IMPL_ICONTENT_USING_GENERIC(_g)                                 \
  NS_IMETHOD GetDocument(nsIDocument*& aResult) const {                    \
    return _g.GetDocument(aResult);                                        \
  }                                                                        \
  NS_IMETHOD SetDocument(nsIDocument* aDocument, PRBool aDeep, PRBool aCompileEventHandlers) {           \
    return _g.SetDocument(aDocument, aDeep, aCompileEventHandlers);                               \
  }                                                                        \
  NS_IMETHOD GetParent(nsIContent*& aResult) const {                       \
    return _g.GetParent(aResult);                                          \
  }                                                                        \
  NS_IMETHOD SetParent(nsIContent* aParent) {                              \
    return _g.SetParent(aParent);                                          \
  }                                                                        \
  NS_IMETHOD CanContainChildren(PRBool& aResult) const {                   \
    return _g.CanContainChildren(aResult);                                 \
  }                                                                        \
  NS_IMETHOD ChildCount(PRInt32& aResult) const {                          \
    return _g.ChildCount(aResult);                                         \
  }                                                                        \
  NS_IMETHOD ChildAt(PRInt32 aIndex, nsIContent*& aResult) const {         \
    return _g.ChildAt(aIndex, aResult);                                    \
  }                                                                        \
  NS_IMETHOD IndexOf(nsIContent* aPossibleChild, PRInt32& aResult) const { \
    return _g.IndexOf(aPossibleChild, aResult);                            \
  }                                                                        \
  NS_IMETHOD InsertChildAt(nsIContent* aKid, PRInt32 aIndex,               \
                           PRBool aNotify) {                               \
    return _g.InsertChildAt(aKid, aIndex, aNotify);                        \
  }                                                                        \
  NS_IMETHOD ReplaceChildAt(nsIContent* aKid, PRInt32 aIndex,              \
                            PRBool aNotify) {                              \
    return _g.ReplaceChildAt(aKid, aIndex, aNotify);                       \
  }                                                                        \
  NS_IMETHOD AppendChildTo(nsIContent* aKid, PRBool aNotify) {             \
    return _g.AppendChildTo(aKid, aNotify);                                \
  }                                                                        \
  NS_IMETHOD RemoveChildAt(PRInt32 aIndex, PRBool aNotify) {               \
    return _g.RemoveChildAt(aIndex, aNotify);                              \
  }                                                                        \
  NS_IMETHOD IsSynthetic(PRBool& aResult) {                                \
    return _g.IsSynthetic(aResult);                                        \
  }                                                                        \
  NS_IMETHOD GetNameSpaceID(PRInt32& aResult) const {                      \
    return _g.GetNameSpaceID(aResult);                                     \
  }                                                                        \
  NS_IMETHOD GetTag(nsIAtom*& aResult) const {                             \
    return _g.GetTag(aResult);                                             \
  }                                                                        \
  NS_IMETHOD GetNodeInfo(nsINodeInfo*& aResult) const {                    \
    return _g.GetNodeInfo(aResult);                                        \
  }                                                                        \
  NS_IMETHOD NormalizeAttributeString(const nsAReadableString& aStr,       \
                                      nsINodeInfo*& aNodeInfo) {           \
    return _g.NormalizeAttributeString(aStr, aNodeInfo);                   \
  }                                                                        \
  NS_IMETHOD SetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName,            \
                          const nsAReadableString& aValue, PRBool aNotify) {        \
    return _g.SetAttribute(aNameSpaceID, aName, aValue, aNotify);          \
  }                                                                        \
  NS_IMETHOD SetAttribute(nsINodeInfo* aNodeInfo,                          \
                          const nsAReadableString& aValue, PRBool aNotify) {        \
    return _g.SetAttribute(aNodeInfo, aValue, aNotify);                    \
  }                                                                        \
  NS_IMETHOD GetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName,            \
                          nsAWritableString& aResult) const {                       \
    return _g.GetAttribute(aNameSpaceID, aName, aResult);                  \
  }                                                                        \
  NS_IMETHOD GetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName,            \
                          nsIAtom*& aPrefix, nsAWritableString& aResult) const {    \
    return _g.GetAttribute(aNameSpaceID, aName, aPrefix, aResult);         \
  }                                                                        \
  NS_IMETHOD UnsetAttribute(PRInt32 aNameSpaceID, nsIAtom* aAttribute,     \
                            PRBool aNotify) {                              \
    return _g.UnsetAttribute(aNameSpaceID, aAttribute, aNotify);           \
  }                                                                        \
  NS_IMETHOD GetAttributeNameAt(PRInt32 aIndex,                            \
                                PRInt32& aNameSpaceID,                     \
                                nsIAtom*& aName,                           \
                                nsIAtom*& aPrefix) const {                 \
    return _g.GetAttributeNameAt(aIndex, aNameSpaceID, aName, aPrefix);    \
  }                                                                        \
  NS_IMETHOD GetAttributeCount(PRInt32& aResult) const {                   \
    return _g.GetAttributeCount(aResult);                                  \
  }                                                                        \
  NS_IMETHOD List(FILE* out, PRInt32 aIndent) const {                      \
    return _g.List(out, aIndent);                                          \
  }                                                                        \
  NS_IMETHOD DumpContent(FILE* out,                                        \
                         PRInt32 aIndent,                                  \
                         PRBool aDumpAll) const {                          \
    return _g.DumpContent(out, aIndent,aDumpAll);                          \
  }                                                                        \
  NS_IMETHOD BeginConvertToXIF(nsIXIFConverter * aConverter) const {       \
    return _g.BeginConvertToXIF(aConverter);                               \
  }                                                                        \
  NS_IMETHOD ConvertContentToXIF(nsIXIFConverter * aConverter) const {     \
    return _g.ConvertContentToXIF(aConverter);                             \
  }                                                                        \
  NS_IMETHOD FinishConvertToXIF(nsIXIFConverter * aConverter) const {      \
    return _g.FinishConvertToXIF(aConverter);                              \
  }                                                                        \
  NS_IMETHOD HandleDOMEvent(nsIPresContext* aPresContext,                  \
                            nsEvent* aEvent,                               \
                            nsIDOMEvent** aDOMEvent,                       \
                            PRUint32 aFlags,                               \
                            nsEventStatus* aEventStatus);                  \
  NS_IMETHOD GetContentID(PRUint32* aID) {                                 \
    return _g.GetContentID(aID);                                           \
  }                                                                        \
  NS_IMETHOD SetContentID(PRUint32 aID) {                                  \
    return _g.SetContentID(aID);                                           \
  }                                                                        \
  NS_IMETHOD RangeAdd(nsIDOMRange& aRange) {                               \
    return _g.RangeAdd(aRange);                                            \
  }                                                                        \
  NS_IMETHOD RangeRemove(nsIDOMRange& aRange) {                            \
    return _g.RangeRemove(aRange);                                         \
  }                                                                        \
  NS_IMETHOD GetRangeList(nsVoidArray*& aResult) const {                   \
    return _g.GetRangeList(aResult);                                       \
  }                                                                        \
  NS_IMETHOD SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const;    \
  NS_IMETHOD SetFocus(nsIPresContext* aPresContext) {                      \
    return _g.SetFocus(aPresContext);                                      \
  }                                                                        \
  NS_IMETHOD RemoveFocus(nsIPresContext* aPresContext) {                   \
    return _g.RemoveFocus(aPresContext);                                   \
  } \
  NS_IMETHOD GetBindingParent(nsIContent** aContent) { \
    return _g.GetBindingParent(aContent); \
  } \
  \
  NS_IMETHOD SetBindingParent(nsIContent* aParent) { \
    return _g.SetBindingParent(aParent); \
  }  

#define NS_IMPL_ICONTENT_NO_SETPARENT_USING_GENERIC(_g)                    \
  NS_IMETHOD GetDocument(nsIDocument*& aResult) const {                    \
    return _g.GetDocument(aResult);                                        \
  }                                                                        \
  NS_IMETHOD SetDocument(nsIDocument* aDocument, PRBool aDeep, PRBool aCompileEventHandlers) {           \
    return _g.SetDocument(aDocument, aDeep, aCompileEventHandlers);                               \
  }                                                                        \
  NS_IMETHOD GetParent(nsIContent*& aResult) const {                       \
    return _g.GetParent(aResult);                                          \
  }                                                                        \
  NS_IMETHOD SetParent(nsIContent* aParent);                               \
  NS_IMETHOD CanContainChildren(PRBool& aResult) const {                   \
    return _g.CanContainChildren(aResult);                                 \
  }                                                                        \
  NS_IMETHOD ChildCount(PRInt32& aResult) const {                          \
    return _g.ChildCount(aResult);                                         \
  }                                                                        \
  NS_IMETHOD ChildAt(PRInt32 aIndex, nsIContent*& aResult) const {         \
    return _g.ChildAt(aIndex, aResult);                                    \
  }                                                                        \
  NS_IMETHOD IndexOf(nsIContent* aPossibleChild, PRInt32& aResult) const { \
    return _g.IndexOf(aPossibleChild, aResult);                            \
  }                                                                        \
  NS_IMETHOD InsertChildAt(nsIContent* aKid, PRInt32 aIndex,               \
                           PRBool aNotify) {                               \
    return _g.InsertChildAt(aKid, aIndex, aNotify);                        \
  }                                                                        \
  NS_IMETHOD ReplaceChildAt(nsIContent* aKid, PRInt32 aIndex,              \
                            PRBool aNotify) {                              \
    return _g.ReplaceChildAt(aKid, aIndex, aNotify);                       \
  }                                                                        \
  NS_IMETHOD AppendChildTo(nsIContent* aKid, PRBool aNotify) {             \
    return _g.AppendChildTo(aKid, aNotify);                                \
  }                                                                        \
  NS_IMETHOD RemoveChildAt(PRInt32 aIndex, PRBool aNotify) {               \
    return _g.RemoveChildAt(aIndex, aNotify);                              \
  }                                                                        \
  NS_IMETHOD IsSynthetic(PRBool& aResult) {                                \
    return _g.IsSynthetic(aResult);                                        \
  }                                                                        \
  NS_IMETHOD GetNameSpaceID(PRInt32& aResult) const {                      \
    return _g.GetNameSpaceID(aResult);                                     \
  }                                                                        \
  NS_IMETHOD GetTag(nsIAtom*& aResult) const {                             \
    return _g.GetTag(aResult);                                             \
  }                                                                        \
  NS_IMETHOD GetNodeInfo(nsINodeInfo*& aResult) const {                    \
    return _g.GetNodeInfo(aResult);                                        \
  }                                                                        \
  NS_IMETHOD NormalizeAttributeString(const nsAReadableString& aStr,       \
                                      nsINodeInfo*& aNodeInfo) {           \
    return _g.NormalizeAttributeString(aStr, aNodeInfo);                   \
  }                                                                        \
  NS_IMETHOD SetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName,            \
                          const nsAReadableString& aValue, PRBool aNotify) { \
    return _g.SetAttribute(aNameSpaceID, aName, aValue, aNotify);          \
  }                                                                        \
  NS_IMETHOD SetAttribute(nsINodeInfo* aNodeInfo,                          \
                          const nsAReadableString& aValue, PRBool aNotify) { \
    return _g.SetAttribute(aNodeInfo, aValue, aNotify);                    \
  }                                                                        \
  NS_IMETHOD GetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName,            \
                          nsAWritableString& aResult) const {              \
    return _g.GetAttribute(aNameSpaceID, aName, aResult);                  \
  }                                                                        \
  NS_IMETHOD GetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName,            \
                          nsIAtom*& aPrefix, nsAWritableString& aResult) const {    \
    return _g.GetAttribute(aNameSpaceID, aName, aPrefix, aResult);         \
  }                                                                        \
  NS_IMETHOD UnsetAttribute(PRInt32 aNameSpaceID, nsIAtom* aAttribute,     \
                            PRBool aNotify) {                              \
    return _g.UnsetAttribute(aNameSpaceID, aAttribute, aNotify);           \
  }                                                                        \
  NS_IMETHOD GetAttributeNameAt(PRInt32 aIndex,                            \
                                PRInt32& aNameSpaceID,                     \
                                nsIAtom*& aName,                           \
                                nsIAtom*& aPrefix) const {                 \
    return _g.GetAttributeNameAt(aIndex, aNameSpaceID, aName, aPrefix);    \
  }                                                                        \
  NS_IMETHOD GetAttributeCount(PRInt32& aResult) const {                   \
    return _g.GetAttributeCount(aResult);                                  \
  }                                                                        \
  NS_IMETHOD List(FILE* out, PRInt32 aIndent) const {                      \
    return _g.List(out, aIndent);                                          \
  }                                                                        \
  NS_IMETHOD DumpContent(FILE* out,                                        \
                         PRInt32 aIndent,                                  \
                         PRBool aDumpAll) const {                          \
    return _g.DumpContent(out, aIndent,aDumpAll);                          \
  }                                                                        \
  NS_IMETHOD BeginConvertToXIF(nsIXIFConverter * aConverter) const {       \
    return _g.BeginConvertToXIF(aConverter);                               \
  }                                                                        \
  NS_IMETHOD ConvertContentToXIF(nsIXIFConverter * aConverter) const {     \
    return _g.ConvertContentToXIF(aConverter);                             \
  }                                                                        \
  NS_IMETHOD FinishConvertToXIF(nsIXIFConverter * aConverter) const {      \
    return _g.FinishConvertToXIF(aConverter);                              \
  }                                                                        \
  NS_IMETHOD HandleDOMEvent(nsIPresContext* aPresContext,                  \
                            nsEvent* aEvent,                               \
                            nsIDOMEvent** aDOMEvent,                       \
                            PRUint32 aFlags,                               \
                            nsEventStatus* aEventStatus);                  \
  NS_IMETHOD GetContentID(PRUint32* aID) {                                 \
    return _g.GetContentID(aID);                                           \
  }                                                                        \
  NS_IMETHOD SetContentID(PRUint32 aID) {                                  \
    return _g.SetContentID(aID);                                           \
  }                                                                        \
  NS_IMETHOD RangeAdd(nsIDOMRange& aRange) {                               \
    return _g.RangeAdd(aRange);                                            \
  }                                                                        \
  NS_IMETHOD RangeRemove(nsIDOMRange& aRange) {                            \
    return _g.RangeRemove(aRange);                                         \
  }                                                                        \
  NS_IMETHOD GetRangeList(nsVoidArray*& aResult) const {                   \
    return _g.GetRangeList(aResult);                                       \
  }                                                                        \
  NS_IMETHOD SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const;    \
  NS_IMETHOD SetFocus(nsIPresContext* aPresContext) {                      \
    return _g.SetFocus(aPresContext);                                      \
  }                                                                        \
  NS_IMETHOD RemoveFocus(nsIPresContext* aPresContext) {                   \
    return _g.RemoveFocus(aPresContext);                                   \
  } \
  NS_IMETHOD GetBindingParent(nsIContent** aContent) { \
    return _g.GetBindingParent(aContent); \
  } \
  \
  NS_IMETHOD SetBindingParent(nsIContent* aParent) { \
    return _g.SetBindingParent(aParent); \
  }  

#define NS_IMPL_ICONTENT_NO_SETDOCUMENT_USING_GENERIC(_g)                  \
  NS_IMETHOD GetDocument(nsIDocument*& aResult) const {                    \
    return _g.GetDocument(aResult);                                        \
  }                                                                        \
  NS_IMETHOD SetDocument(nsIDocument* aDocument, PRBool aDeep, PRBool aCompileEventHandlers);            \
  NS_IMETHOD GetParent(nsIContent*& aResult) const {                       \
    return _g.GetParent(aResult);                                          \
  }                                                                        \
  NS_IMETHOD SetParent(nsIContent* aParent) {                              \
    return _g.SetParent(aParent);                                          \
  }                                                                        \
  NS_IMETHOD CanContainChildren(PRBool& aResult) const {                   \
    return _g.CanContainChildren(aResult);                                 \
  }                                                                        \
  NS_IMETHOD ChildCount(PRInt32& aResult) const {                          \
    return _g.ChildCount(aResult);                                         \
  }                                                                        \
  NS_IMETHOD ChildAt(PRInt32 aIndex, nsIContent*& aResult) const {         \
    return _g.ChildAt(aIndex, aResult);                                    \
  }                                                                        \
  NS_IMETHOD IndexOf(nsIContent* aPossibleChild, PRInt32& aResult) const { \
    return _g.IndexOf(aPossibleChild, aResult);                            \
  }                                                                        \
  NS_IMETHOD InsertChildAt(nsIContent* aKid, PRInt32 aIndex,               \
                           PRBool aNotify) {                               \
    return _g.InsertChildAt(aKid, aIndex, aNotify);                        \
  }                                                                        \
  NS_IMETHOD ReplaceChildAt(nsIContent* aKid, PRInt32 aIndex,              \
                            PRBool aNotify) {                              \
    return _g.ReplaceChildAt(aKid, aIndex, aNotify);                       \
  }                                                                        \
  NS_IMETHOD AppendChildTo(nsIContent* aKid, PRBool aNotify) {             \
    return _g.AppendChildTo(aKid, aNotify);                                \
  }                                                                        \
  NS_IMETHOD RemoveChildAt(PRInt32 aIndex, PRBool aNotify) {               \
    return _g.RemoveChildAt(aIndex, aNotify);                              \
  }                                                                        \
  NS_IMETHOD IsSynthetic(PRBool& aResult) {                                \
    return _g.IsSynthetic(aResult);                                        \
  }                                                                        \
  NS_IMETHOD GetNameSpaceID(PRInt32& aResult) const {                      \
    return _g.GetNameSpaceID(aResult);                                     \
  }                                                                        \
  NS_IMETHOD GetTag(nsIAtom*& aResult) const {                             \
    return _g.GetTag(aResult);                                             \
  }                                                                        \
  NS_IMETHOD GetNodeInfo(nsINodeInfo*& aResult) const {                    \
    return _g.GetNodeInfo(aResult);                                        \
  }                                                                        \
  NS_IMETHOD NormalizeAttributeString(const nsAReadableString& aStr,       \
                                      nsINodeInfo*& aNodeInfo) {           \
    return _g.NormalizeAttributeString(aStr, aNodeInfo);                   \
  }                                                                        \
  NS_IMETHOD SetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName,            \
                          const nsAReadableString& aValue, PRBool aNotify) { \
    return _g.SetAttribute(aNameSpaceID, aName, aValue, aNotify);          \
  }                                                                        \
  NS_IMETHOD SetAttribute(nsINodeInfo* aNodeInfo,                          \
                          const nsAReadableString& aValue, PRBool aNotify) { \
    return _g.SetAttribute(aNodeInfo, aValue, aNotify);                    \
  }                                                                        \
  NS_IMETHOD GetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName,            \
                          nsAWritableString& aResult) const {              \
    return _g.GetAttribute(aNameSpaceID, aName, aResult);                  \
  }                                                                        \
  NS_IMETHOD GetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName,            \
                          nsIAtom*& aPrefix, nsAWritableString& aResult) const {    \
    return _g.GetAttribute(aNameSpaceID, aName, aPrefix, aResult);         \
  }                                                                        \
  NS_IMETHOD UnsetAttribute(PRInt32 aNameSpaceID, nsIAtom* aAttribute,     \
                            PRBool aNotify) {                              \
    return _g.UnsetAttribute(aNameSpaceID, aAttribute, aNotify);           \
  }                                                                        \
  NS_IMETHOD GetAttributeNameAt(PRInt32 aIndex,                            \
                                PRInt32& aNameSpaceID,                     \
                                nsIAtom*& aName,                           \
                                nsIAtom*& aPrefix) const {                 \
    return _g.GetAttributeNameAt(aIndex, aNameSpaceID, aName, aPrefix);    \
  }                                                                        \
  NS_IMETHOD GetAttributeCount(PRInt32& aResult) const {                   \
    return _g.GetAttributeCount(aResult);                                  \
  }                                                                        \
  NS_IMETHOD List(FILE* out, PRInt32 aIndent) const {                      \
    return _g.List(out, aIndent);                                          \
  }                                                                        \
  NS_IMETHOD DumpContent(FILE* out,                                        \
                         PRInt32 aIndent,                                  \
                         PRBool aDumpAll) const {                          \
    return _g.DumpContent(out, aIndent,aDumpAll);                          \
  }                                                                        \
  NS_IMETHOD BeginConvertToXIF(nsIXIFConverter * aConverter) const {       \
    return _g.BeginConvertToXIF(aConverter);                               \
  }                                                                        \
  NS_IMETHOD ConvertContentToXIF(nsIXIFConverter * aConverter) const {     \
    return _g.ConvertContentToXIF(aConverter);                             \
  }                                                                        \
  NS_IMETHOD FinishConvertToXIF(nsIXIFConverter * aConverter) const {      \
    return _g.FinishConvertToXIF(aConverter);                              \
  }                                                                        \
  NS_IMETHOD HandleDOMEvent(nsIPresContext* aPresContext,                  \
                            nsEvent* aEvent,                               \
                            nsIDOMEvent** aDOMEvent,                       \
                            PRUint32 aFlags,                               \
                            nsEventStatus* aEventStatus);                  \
  NS_IMETHOD GetContentID(PRUint32* aID) {                                 \
    return _g.GetContentID(aID);                                           \
  }                                                                        \
  NS_IMETHOD SetContentID(PRUint32 aID) {                                  \
    return _g.SetContentID(aID);                                           \
  }                                                                        \
  NS_IMETHOD RangeAdd(nsIDOMRange& aRange) {                               \
    return _g.RangeAdd(aRange);                                            \
  }                                                                        \
  NS_IMETHOD RangeRemove(nsIDOMRange& aRange) {                            \
    return _g.RangeRemove(aRange);                                         \
  }                                                                        \
  NS_IMETHOD GetRangeList(nsVoidArray*& aResult) const {                   \
    return _g.GetRangeList(aResult);                                       \
  }                                                                        \
  NS_IMETHOD SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const;    \
  NS_IMETHOD SetFocus(nsIPresContext* aPresContext) {                      \
    return _g.SetFocus(aPresContext);                                      \
  }                                                                        \
  NS_IMETHOD RemoveFocus(nsIPresContext* aPresContext) {                   \
    return _g.RemoveFocus(aPresContext);                                   \
  }  \
  NS_IMETHOD GetBindingParent(nsIContent** aContent) { \
    return _g.GetBindingParent(aContent); \
  } \
  \
  NS_IMETHOD SetBindingParent(nsIContent* aParent) { \
    return _g.SetBindingParent(aParent); \
  }    

#define NS_IMPL_ICONTENT_NO_SETPARENT_NO_SETDOCUMENT_USING_GENERIC(_g)     \
  NS_IMETHOD GetDocument(nsIDocument*& aResult) const {                    \
    return _g.GetDocument(aResult);                                        \
  }                                                                        \
  NS_IMETHOD SetDocument(nsIDocument* aDocument, PRBool aDeep, PRBool aCompileEventHandlers);            \
  NS_IMETHOD GetParent(nsIContent*& aResult) const {                       \
    return _g.GetParent(aResult);                                          \
  }                                                                        \
  NS_IMETHOD SetParent(nsIContent* aParent);                               \
  NS_IMETHOD CanContainChildren(PRBool& aResult) const {                   \
    return _g.CanContainChildren(aResult);                                 \
  }                                                                        \
  NS_IMETHOD ChildCount(PRInt32& aResult) const {                          \
    return _g.ChildCount(aResult);                                         \
  }                                                                        \
  NS_IMETHOD ChildAt(PRInt32 aIndex, nsIContent*& aResult) const {         \
    return _g.ChildAt(aIndex, aResult);                                    \
  }                                                                        \
  NS_IMETHOD IndexOf(nsIContent* aPossibleChild, PRInt32& aResult) const { \
    return _g.IndexOf(aPossibleChild, aResult);                            \
  }                                                                        \
  NS_IMETHOD InsertChildAt(nsIContent* aKid, PRInt32 aIndex,               \
                           PRBool aNotify) {                               \
    return _g.InsertChildAt(aKid, aIndex, aNotify);                        \
  }                                                                        \
  NS_IMETHOD ReplaceChildAt(nsIContent* aKid, PRInt32 aIndex,              \
                            PRBool aNotify) {                              \
    return _g.ReplaceChildAt(aKid, aIndex, aNotify);                       \
  }                                                                        \
  NS_IMETHOD AppendChildTo(nsIContent* aKid, PRBool aNotify) {             \
    return _g.AppendChildTo(aKid, aNotify);                                \
  }                                                                        \
  NS_IMETHOD RemoveChildAt(PRInt32 aIndex, PRBool aNotify) {               \
    return _g.RemoveChildAt(aIndex, aNotify);                              \
  }                                                                        \
  NS_IMETHOD IsSynthetic(PRBool& aResult) {                                \
    return _g.IsSynthetic(aResult);                                        \
  }                                                                        \
  NS_IMETHOD GetNameSpaceID(PRInt32& aResult) const {                      \
    return _g.GetNameSpaceID(aResult);                                     \
  }                                                                        \
  NS_IMETHOD GetTag(nsIAtom*& aResult) const {                             \
    return _g.GetTag(aResult);                                             \
  }                                                                        \
  NS_IMETHOD GetNodeInfo(nsINodeInfo*& aResult) const {                    \
    return _g.GetNodeInfo(aResult);                                        \
  }                                                                        \
  NS_IMETHOD NormalizeAttributeString(const nsAReadableString& aStr,       \
                                      nsINodeInfo*& aNodeInfo) {           \
    return _g.NormalizeAttributeString(aStr, aNodeInfo);                   \
  }                                                                        \
  NS_IMETHOD SetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName,            \
                          const nsAReadableString& aValue, PRBool aNotify) { \
    return _g.SetAttribute(aNameSpaceID, aName, aValue, aNotify);          \
  }                                                                        \
  NS_IMETHOD SetAttribute(nsINodeInfo* aNodeInfo,                          \
                          const nsAReadableString& aValue, PRBool aNotify) { \
    return _g.SetAttribute(aNodeInfo, aValue, aNotify);                    \
  }                                                                        \
  NS_IMETHOD GetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName,            \
                          nsAWritableString& aResult) const {              \
    return _g.GetAttribute(aNameSpaceID, aName, aResult);                  \
  }                                                                        \
  NS_IMETHOD GetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName,            \
                          nsIAtom*& aPrefix, nsAWritableString& aResult) const {    \
    return _g.GetAttribute(aNameSpaceID, aName, aPrefix, aResult);         \
  }                                                                        \
  NS_IMETHOD UnsetAttribute(PRInt32 aNameSpaceID, nsIAtom* aAttribute,     \
                            PRBool aNotify) {                              \
    return _g.UnsetAttribute(aNameSpaceID, aAttribute, aNotify);           \
  }                                                                        \
  NS_IMETHOD GetAttributeNameAt(PRInt32 aIndex,                            \
                                PRInt32& aNameSpaceID,                     \
                                nsIAtom*& aName,                           \
                                nsIAtom*& aPrefix) const {                 \
    return _g.GetAttributeNameAt(aIndex, aNameSpaceID, aName, aPrefix);    \
  }                                                                        \
  NS_IMETHOD GetAttributeCount(PRInt32& aResult) const {                   \
    return _g.GetAttributeCount(aResult);                                  \
  }                                                                        \
  NS_IMETHOD List(FILE* out, PRInt32 aIndent) const {                      \
    return _g.List(out, aIndent);                                          \
  }                                                                        \
  NS_IMETHOD DumpContent(FILE* out,                                        \
                         PRInt32 aIndent,                                  \
                         PRBool aDumpAll) const {                          \
    return _g.DumpContent(out, aIndent,aDumpAll);                          \
  }                                                                        \
  NS_IMETHOD BeginConvertToXIF(nsIXIFConverter * aConverter) const {       \
    return _g.BeginConvertToXIF(aConverter);                               \
  }                                                                        \
  NS_IMETHOD ConvertContentToXIF(nsIXIFConverter * aConverter) const {     \
    return _g.ConvertContentToXIF(aConverter);                             \
  }                                                                        \
  NS_IMETHOD FinishConvertToXIF(nsIXIFConverter * aConverter) const {      \
    return _g.FinishConvertToXIF(aConverter);                              \
  }                                                                        \
  NS_IMETHOD HandleDOMEvent(nsIPresContext* aPresContext,                  \
                            nsEvent* aEvent,                               \
                            nsIDOMEvent** aDOMEvent,                       \
                            PRUint32 aFlags,                               \
                            nsEventStatus* aEventStatus);                  \
  NS_IMETHOD GetContentID(PRUint32* aID) {                                 \
    return _g.GetContentID(aID);                                           \
  }                                                                        \
  NS_IMETHOD SetContentID(PRUint32 aID) {                                  \
    return _g.SetContentID(aID);                                           \
  }                                                                        \
  NS_IMETHOD RangeAdd(nsIDOMRange& aRange) {                               \
    return _g.RangeAdd(aRange);                                            \
  }                                                                        \
  NS_IMETHOD RangeRemove(nsIDOMRange& aRange) {                            \
    return _g.RangeRemove(aRange);                                         \
  }                                                                        \
  NS_IMETHOD GetRangeList(nsVoidArray*& aResult) const {                   \
    return _g.GetRangeList(aResult);                                       \
  }                                                                        \
  NS_IMETHOD SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const;    \
  NS_IMETHOD SetFocus(nsIPresContext* aPresContext) {                      \
    return _g.SetFocus(aPresContext);                                      \
  }                                                                        \
  NS_IMETHOD RemoveFocus(nsIPresContext* aPresContext) {                   \
    return _g.RemoveFocus(aPresContext);                                   \
  }    \
  NS_IMETHOD GetBindingParent(nsIContent** aContent) { \
    return _g.GetBindingParent(aContent); \
  } \
  \
  NS_IMETHOD SetBindingParent(nsIContent* aParent) { \
    return _g.SetBindingParent(aParent); \
  }  
  
#define NS_IMPL_ICONTENT_NO_FOCUS_USING_GENERIC(_g)                        \
  NS_IMETHOD GetDocument(nsIDocument*& aResult) const {                    \
    return _g.GetDocument(aResult);                                        \
  }                                                                        \
  NS_IMETHOD SetDocument(nsIDocument* aDocument, PRBool aDeep, PRBool aCompileEventHandlers) {           \
    return _g.SetDocument(aDocument, aDeep, aCompileEventHandlers);                               \
  }                                                                        \
  NS_IMETHOD GetParent(nsIContent*& aResult) const {                       \
    return _g.GetParent(aResult);                                          \
  }                                                                        \
  NS_IMETHOD SetParent(nsIContent* aParent) {                              \
    return _g.SetParent(aParent);                                          \
  }                                                                        \
  NS_IMETHOD CanContainChildren(PRBool& aResult) const {                   \
    return _g.CanContainChildren(aResult);                                 \
  }                                                                        \
  NS_IMETHOD ChildCount(PRInt32& aResult) const {                          \
    return _g.ChildCount(aResult);                                         \
  }                                                                        \
  NS_IMETHOD ChildAt(PRInt32 aIndex, nsIContent*& aResult) const {         \
    return _g.ChildAt(aIndex, aResult);                                    \
  }                                                                        \
  NS_IMETHOD IndexOf(nsIContent* aPossibleChild, PRInt32& aResult) const { \
    return _g.IndexOf(aPossibleChild, aResult);                            \
  }                                                                        \
  NS_IMETHOD InsertChildAt(nsIContent* aKid, PRInt32 aIndex,               \
                           PRBool aNotify) {                               \
    return _g.InsertChildAt(aKid, aIndex, aNotify);                        \
  }                                                                        \
  NS_IMETHOD ReplaceChildAt(nsIContent* aKid, PRInt32 aIndex,              \
                            PRBool aNotify) {                              \
    return _g.ReplaceChildAt(aKid, aIndex, aNotify);                       \
  }                                                                        \
  NS_IMETHOD AppendChildTo(nsIContent* aKid, PRBool aNotify) {             \
    return _g.AppendChildTo(aKid, aNotify);                                \
  }                                                                        \
  NS_IMETHOD RemoveChildAt(PRInt32 aIndex, PRBool aNotify) {               \
    return _g.RemoveChildAt(aIndex, aNotify);                              \
  }                                                                        \
  NS_IMETHOD IsSynthetic(PRBool& aResult) {                                \
    return _g.IsSynthetic(aResult);                                        \
  }                                                                        \
  NS_IMETHOD GetNameSpaceID(PRInt32& aResult) const {                      \
    return _g.GetNameSpaceID(aResult);                                     \
  }                                                                        \
  NS_IMETHOD GetTag(nsIAtom*& aResult) const {                             \
    return _g.GetTag(aResult);                                             \
  }                                                                        \
  NS_IMETHOD GetNodeInfo(nsINodeInfo*& aResult) const {                    \
    return _g.GetNodeInfo(aResult);                                        \
  }                                                                        \
  NS_IMETHOD NormalizeAttributeString(const nsAReadableString& aStr,       \
                                      nsINodeInfo*& aNodeInfo) {           \
    return _g.NormalizeAttributeString(aStr, aNodeInfo);                   \
  }                                                                        \
  NS_IMETHOD SetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName,            \
                          const nsAReadableString& aValue, PRBool aNotify) { \
    return _g.SetAttribute(aNameSpaceID, aName, aValue, aNotify);          \
  }                                                                        \
  NS_IMETHOD SetAttribute(nsINodeInfo* aNodeInfo,                          \
                          const nsAReadableString& aValue, PRBool aNotify) { \
    return _g.SetAttribute(aNodeInfo, aValue, aNotify);                    \
  }                                                                        \
  NS_IMETHOD GetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName,            \
                          nsAWritableString& aResult) const {              \
    return _g.GetAttribute(aNameSpaceID, aName, aResult);                  \
  }                                                                        \
  NS_IMETHOD GetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName,            \
                          nsIAtom*& aPrefix, nsAWritableString& aResult) const {    \
    return _g.GetAttribute(aNameSpaceID, aName, aPrefix, aResult);         \
  }                                                                        \
  NS_IMETHOD UnsetAttribute(PRInt32 aNameSpaceID, nsIAtom* aAttribute,     \
                            PRBool aNotify) {                              \
    return _g.UnsetAttribute(aNameSpaceID, aAttribute, aNotify);           \
  }                                                                        \
  NS_IMETHOD GetAttributeNameAt(PRInt32 aIndex,                            \
                                PRInt32& aNameSpaceID,                     \
                                nsIAtom*& aName,                           \
                                nsIAtom*& aPrefix) const {                 \
    return _g.GetAttributeNameAt(aIndex, aNameSpaceID, aName, aPrefix);    \
  }                                                                        \
  NS_IMETHOD GetAttributeCount(PRInt32& aResult) const {                   \
    return _g.GetAttributeCount(aResult);                                  \
  }                                                                        \
  NS_IMETHOD List(FILE* out, PRInt32 aIndent) const {                      \
    return _g.List(out, aIndent);                                          \
  }                                                                        \
  NS_IMETHOD DumpContent(FILE* out,                                        \
                         PRInt32 aIndent,                                  \
                         PRBool aDumpAll) const {                          \
    return _g.DumpContent(out, aIndent,aDumpAll);                          \
  }                                                                        \
  NS_IMETHOD BeginConvertToXIF(nsIXIFConverter * aConverter) const {       \
    return _g.BeginConvertToXIF(aConverter);                               \
  }                                                                        \
  NS_IMETHOD ConvertContentToXIF(nsIXIFConverter * aConverter) const {     \
    return _g.ConvertContentToXIF(aConverter);                             \
  }                                                                        \
  NS_IMETHOD FinishConvertToXIF(nsIXIFConverter * aConverter) const {      \
    return _g.FinishConvertToXIF(aConverter);                              \
  }                                                                        \
  NS_IMETHOD HandleDOMEvent(nsIPresContext* aPresContext,                  \
                            nsEvent* aEvent,                               \
                            nsIDOMEvent** aDOMEvent,                       \
                            PRUint32 aFlags,                               \
                            nsEventStatus* aEventStatus);                  \
  NS_IMETHOD GetContentID(PRUint32* aID) {                                 \
    return _g.GetContentID(aID);                                           \
  }                                                                        \
  NS_IMETHOD SetContentID(PRUint32 aID) {                                  \
    return _g.SetContentID(aID);                                           \
  }                                                                        \
  NS_IMETHOD RangeAdd(nsIDOMRange& aRange) {                               \
    return _g.RangeAdd(aRange);                                            \
  }                                                                        \
  NS_IMETHOD RangeRemove(nsIDOMRange& aRange) {                            \
    return _g.RangeRemove(aRange);                                         \
  }                                                                        \
  NS_IMETHOD GetRangeList(nsVoidArray*& aResult) const {                   \
    return _g.GetRangeList(aResult);                                       \
  }                                                                        \
  NS_IMETHOD SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const;    \
  NS_IMETHOD SetFocus(nsIPresContext* aPresContext);                       \
  NS_IMETHOD RemoveFocus(nsIPresContext* aPresContext);  \
  NS_IMETHOD GetBindingParent(nsIContent** aContent) { \
    return _g.GetBindingParent(aContent); \
  } \
  \
  NS_IMETHOD SetBindingParent(nsIContent* aParent) { \
    return _g.SetBindingParent(aParent); \
  }   

#define NS_IMPL_ICONTENT_NO_SETPARENT_NO_SETDOCUMENT_NO_FOCUS_USING_GENERIC(_g)                        \
  NS_IMETHOD GetDocument(nsIDocument*& aResult) const {                    \
    return _g.GetDocument(aResult);                                        \
  }                                                                        \
  NS_IMETHOD SetDocument(nsIDocument* aDocument, PRBool aDeep, PRBool aCompileEventHandlers);            \
  NS_IMETHOD GetParent(nsIContent*& aResult) const {                       \
    return _g.GetParent(aResult);                                          \
  }                                                                        \
  NS_IMETHOD SetParent(nsIContent* aParent);                               \
  NS_IMETHOD CanContainChildren(PRBool& aResult) const {                   \
    return _g.CanContainChildren(aResult);                                 \
  }                                                                        \
  NS_IMETHOD ChildCount(PRInt32& aResult) const {                          \
    return _g.ChildCount(aResult);                                         \
  }                                                                        \
  NS_IMETHOD ChildAt(PRInt32 aIndex, nsIContent*& aResult) const {         \
    return _g.ChildAt(aIndex, aResult);                                    \
  }                                                                        \
  NS_IMETHOD IndexOf(nsIContent* aPossibleChild, PRInt32& aResult) const { \
    return _g.IndexOf(aPossibleChild, aResult);                            \
  }                                                                        \
  NS_IMETHOD InsertChildAt(nsIContent* aKid, PRInt32 aIndex,               \
                           PRBool aNotify) {                               \
    return _g.InsertChildAt(aKid, aIndex, aNotify);                        \
  }                                                                        \
  NS_IMETHOD ReplaceChildAt(nsIContent* aKid, PRInt32 aIndex,              \
                            PRBool aNotify) {                              \
    return _g.ReplaceChildAt(aKid, aIndex, aNotify);                       \
  }                                                                        \
  NS_IMETHOD AppendChildTo(nsIContent* aKid, PRBool aNotify) {             \
    return _g.AppendChildTo(aKid, aNotify);                                \
  }                                                                        \
  NS_IMETHOD RemoveChildAt(PRInt32 aIndex, PRBool aNotify) {               \
    return _g.RemoveChildAt(aIndex, aNotify);                              \
  }                                                                        \
  NS_IMETHOD IsSynthetic(PRBool& aResult) {                                \
    return _g.IsSynthetic(aResult);                                        \
  }                                                                        \
  NS_IMETHOD GetNameSpaceID(PRInt32& aResult) const {                      \
    return _g.GetNameSpaceID(aResult);                                     \
  }                                                                        \
  NS_IMETHOD GetTag(nsIAtom*& aResult) const {                             \
    return _g.GetTag(aResult);                                             \
  }                                                                        \
  NS_IMETHOD GetNodeInfo(nsINodeInfo*& aResult) const {                    \
    return _g.GetNodeInfo(aResult);                                        \
  }                                                                        \
  NS_IMETHOD NormalizeAttributeString(const nsAReadableString& aStr,       \
                                      nsINodeInfo*& aName) {               \
    return _g.NormalizeAttributeString(aStr, aName);                       \
  }                                                                        \
  NS_IMETHOD SetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName,            \
                          const nsAReadableString& aValue, PRBool aNotify) { \
    return _g.SetAttribute(aNameSpaceID, aName, aValue, aNotify);          \
  }                                                                        \
  NS_IMETHOD SetAttribute(nsINodeInfo* aNodeInfo,                          \
                          const nsAReadableString& aValue, PRBool aNotify) { \
    return _g.SetAttribute(aNodeInfo, aValue, aNotify);                    \
  }                                                                        \
  NS_IMETHOD GetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName,            \
                          nsAWritableString& aResult) const {              \
    return _g.GetAttribute(aNameSpaceID, aName, aResult);                  \
  }                                                                        \
  NS_IMETHOD GetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName,            \
                          nsIAtom*& aPrefix, nsAWritableString& aResult) const {    \
    return _g.GetAttribute(aNameSpaceID, aName, aPrefix, aResult);         \
  }                                                                        \
  NS_IMETHOD UnsetAttribute(PRInt32 aNameSpaceID, nsIAtom* aAttribute,     \
                            PRBool aNotify) {                              \
    return _g.UnsetAttribute(aNameSpaceID, aAttribute, aNotify);           \
  }                                                                        \
  NS_IMETHOD GetAttributeNameAt(PRInt32 aIndex,                            \
                                PRInt32& aNameSpaceID,                     \
                                nsIAtom*& aName,                           \
                                nsIAtom*& aPrefix) const {                 \
    return _g.GetAttributeNameAt(aIndex, aNameSpaceID, aName, aPrefix);    \
  }                                                                        \
  NS_IMETHOD GetAttributeCount(PRInt32& aResult) const {                   \
    return _g.GetAttributeCount(aResult);                                  \
  }                                                                        \
  NS_IMETHOD List(FILE* out, PRInt32 aIndent) const {                      \
    return _g.List(out, aIndent);                                          \
  }                                                                        \
  NS_IMETHOD DumpContent(FILE* out,                                        \
                         PRInt32 aIndent,                                  \
                         PRBool aDumpAll) const {                          \
    return _g.DumpContent(out, aIndent,aDumpAll);                          \
  }                                                                        \
  NS_IMETHOD BeginConvertToXIF(nsIXIFConverter * aConverter) const {       \
    return _g.BeginConvertToXIF(aConverter);                               \
  }                                                                        \
  NS_IMETHOD ConvertContentToXIF(nsIXIFConverter * aConverter) const {     \
    return _g.ConvertContentToXIF(aConverter);                             \
  }                                                                        \
  NS_IMETHOD FinishConvertToXIF(nsIXIFConverter * aConverter) const {      \
    return _g.FinishConvertToXIF(aConverter);                              \
  }                                                                        \
  NS_IMETHOD HandleDOMEvent(nsIPresContext* aPresContext,                  \
                            nsEvent* aEvent,                               \
                            nsIDOMEvent** aDOMEvent,                       \
                            PRUint32 aFlags,                               \
                            nsEventStatus* aEventStatus);                  \
  NS_IMETHOD GetContentID(PRUint32* aID) {                                 \
    return _g.GetContentID(aID);                                           \
  }                                                                        \
  NS_IMETHOD SetContentID(PRUint32 aID) {                                  \
    return _g.SetContentID(aID);                                           \
  }                                                                        \
  NS_IMETHOD RangeAdd(nsIDOMRange& aRange) {                               \
    return _g.RangeAdd(aRange);                                            \
  }                                                                        \
  NS_IMETHOD RangeRemove(nsIDOMRange& aRange) {                            \
    return _g.RangeRemove(aRange);                                         \
  }                                                                        \
  NS_IMETHOD GetRangeList(nsVoidArray*& aResult) const {                   \
    return _g.GetRangeList(aResult);                                       \
  }                                                                        \
  NS_IMETHOD SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const;    \
  NS_IMETHOD SetFocus(nsIPresContext* aPresContext);                       \
  NS_IMETHOD RemoveFocus(nsIPresContext* aPresContext);   \
  NS_IMETHOD GetBindingParent(nsIContent** aContent) { \
    return _g.GetBindingParent(aContent); \
  } \
  \
  NS_IMETHOD SetBindingParent(nsIContent* aParent) { \
    return _g.SetBindingParent(aParent); \
  }  

/**
 * Implement the nsIScriptObjectOwner API by forwarding the methods to a
 * generic content object
 */
#define NS_IMPL_ISCRIPTOBJECTOWNER_USING_GENERIC(_g)     \
  NS_IMETHOD GetScriptObject(nsIScriptContext* aContext, \
                             void** aScriptObject) {     \
    return _g.GetScriptObject(aContext, aScriptObject);  \
  }                                                      \
  NS_IMETHOD SetScriptObject(void *aScriptObject) {      \
    return _g.SetScriptObject(aScriptObject);            \
  }

#define NS_IMPL_IJSSCRIPTOBJECT_USING_GENERIC(_g)                       \
  NS_IMPL_ISCRIPTOBJECTOWNER_USING_GENERIC(_g)                          \
  virtual PRBool    AddProperty(JSContext *aContext, JSObject *aObj,    \
                        jsval aID, jsval *aVp) {                        \
    return _g.AddProperty(aContext, aObj, aID, aVp);                    \
  }                                                                     \
  virtual PRBool    DeleteProperty(JSContext *aContext, JSObject *aObj, \
                        jsval aID, jsval *aVp) {                        \
    return _g.DeleteProperty(aContext, aObj, aID, aVp);                 \
  }                                                                     \
  virtual PRBool    GetProperty(JSContext *aContext, JSObject *aObj,    \
                        jsval aID, jsval *aVp) {                        \
    return _g.GetProperty(aContext, aObj, aID, aVp);                    \
  }                                                                     \
  virtual PRBool    SetProperty(JSContext *aContext, JSObject *aObj,    \
                        jsval aID, jsval *aVp) {                        \
    return _g.SetProperty(aContext, aObj, aID, aVp);                    \
  }                                                                     \
  virtual PRBool    EnumerateProperty(JSContext *aContext, JSObject *aObj) { \
    return _g.EnumerateProperty(aContext, aObj);                             \
  }                                                                          \
  virtual PRBool    Resolve(JSContext *aContext, JSObject *aObj, jsval aID) { \
    return _g.EnumerateProperty(aContext, aObj);                              \
  }                                                                           \
  virtual PRBool    Convert(JSContext *aContext, JSObject *aObj, jsval aID) { \
    return _g.EnumerateProperty(aContext, aObj);                              \
  }                                                                           \
  virtual void      Finalize(JSContext *aContext, JSObject *aObj) {     \
    _g.Finalize(aContext, aObj);                                        \
  }

#define NS_IMPL_CONTENT_QUERY_INTERFACE(_id, _iptr, _this, _base) \
  if (_id.Equals(NS_GET_IID(nsISupports))) {                    \
    _base* tmp = _this;                                         \
    nsISupports* tmp2 = tmp;                                    \
    *_iptr = (void*) tmp2;                                      \
    NS_ADDREF_THIS();                                           \
    return NS_OK;                                               \
  }                                                             \
  if (_id.Equals(NS_GET_IID(nsIDOMNode))) {                     \
    nsIDOMNode* tmp = _this;                                    \
    *_iptr = (void*) tmp;                                       \
    NS_ADDREF_THIS();                                           \
    return NS_OK;                                               \
  }                                                             \
  if (_id.Equals(NS_GET_IID(nsIDOMElement))) {                  \
    nsIDOMElement* tmp = _this;                                 \
    *_iptr = (void*) tmp;                                       \
    NS_ADDREF_THIS();                                           \
    return NS_OK;                                               \
  }                                                             \
  if (_id.Equals(NS_GET_IID(nsIDOMEventReceiver))) {            \
    nsCOMPtr<nsIEventListenerManager> man;                      \
    if (NS_SUCCEEDED(mInner.GetListenerManager(getter_AddRefs(man)))){ \
      return man->QueryInterface(NS_GET_IID(nsIDOMEventReceiver), (void**)_iptr); \
    }                                                           \
    return NS_NOINTERFACE;                                      \
  }                                                             \
  if (_id.Equals(NS_GET_IID(nsIDOMEventTarget))) {              \
    nsCOMPtr<nsIEventListenerManager> man;                      \
    if (NS_SUCCEEDED(mInner.GetListenerManager(getter_AddRefs(man)))){ \
      return man->QueryInterface(NS_GET_IID(nsIDOMEventTarget), (void**)_iptr); \
    }                                                           \
    return NS_NOINTERFACE;                                      \
  }                                                             \
  if (_id.Equals(NS_GET_IID(nsIScriptObjectOwner))) {           \
    nsIScriptObjectOwner* tmp = _this;                          \
    *_iptr = (void*) tmp;                                       \
    NS_ADDREF_THIS();                                           \
    return NS_OK;                                               \
  }                                                             \
  if (_id.Equals(NS_GET_IID(nsIContent))) {                     \
    _base* tmp = _this;                                         \
    nsIContent* tmp2 = tmp;                                     \
    *_iptr = (void*) tmp2;                                      \
    NS_ADDREF_THIS();                                           \
    return NS_OK;                                               \
  }                                                             \
  if (_id.Equals(NS_GET_IID(nsIJSScriptObject))) {              \
    nsIJSScriptObject* tmp = _this;                             \
    *_iptr = (void*) tmp;                                       \
    NS_ADDREF_THIS();                                           \
    return NS_OK;                                               \
  }                                                             

#endif /* nsGenericElement_h___ */
