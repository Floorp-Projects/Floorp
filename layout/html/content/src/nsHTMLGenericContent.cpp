/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */
#include "nsHTMLGenericContent.h"

#include "nsIAtom.h"
#include "nsIContentDelegate.h"
#include "nsICSSParser.h"
#include "nsIDocument.h"
#include "nsIDOMAttribute.h"
#include "nsIDOMEventReceiver.h"
#include "nsIDOMNamedNodeMap.h"
#include "nsIEventListenerManager.h"
#include "nsIHTMLAttributes.h"
#include "nsIHTMLContent.h"
#include "nsIScriptContextOwner.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptObjectOwner.h"
#include "nsISizeOfHandler.h"
#include "nsIStyleRule.h"
#include "nsISupportsArray.h"
#include "nsXIFConverter.h"
#include "nsFrame.h"

#include "nsString.h"
#include "nsHTMLAtoms.h"
#include "nsDOMEventsIIDs.h"
#include "prprf.h"

#include "nsIEventStateManager.h"
#include "nsDOMEvent.h"
#include "nsIPrivateDOMEvent.h"

// XXX todo: add in missing out-of-memory checks

NS_DEFINE_IID(kIDOMNodeIID, NS_IDOMNODE_IID);
NS_DEFINE_IID(kIDOMElementIID, NS_IDOMELEMENT_IID);
NS_DEFINE_IID(kIDOMHTMLElementIID, NS_IDOMHTMLELEMENT_IID);
NS_DEFINE_IID(kIDOMEventReceiverIID, NS_IDOMEVENTRECEIVER_IID);
NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
NS_DEFINE_IID(kIContentIID, NS_ICONTENT_IID);
//NS_DEFINE_IID(kIHTMLContentIID, NS_IHTMLCONTENT_IID);

static NS_DEFINE_IID(kIContentDelegateIID, NS_ICONTENTDELEGATE_IID);
static NS_DEFINE_IID(kIDOMAttributeIID, NS_IDOMATTRIBUTE_IID);
static NS_DEFINE_IID(kIDOMNamedNodeMapIID, NS_IDOMNAMEDNODEMAP_IID);
static NS_DEFINE_IID(kIPrivateDOMEventIID, NS_IPRIVATEDOMEVENT_IID);
static NS_DEFINE_IID(kIStyleRuleIID, NS_ISTYLE_RULE_IID);

static nsIContentDelegate* gContentDelegate;

/**
 * THE html content delegate. There is exactly one instance of this
 * class and it's used for all html content. It just turns around
 * and asks the content object to create the frame.
 */
class ZContentDelegate : public nsIContentDelegate {
public:
  ZContentDelegate();
  NS_DECL_ISUPPORTS
  NS_IMETHOD CreateFrame(nsIPresContext* aPresContext,
                         nsIContent* aContent,
                         nsIFrame* aParentFrame,
                         nsIStyleContext* aStyleContext,
                         nsIFrame*& aResult);
protected:
  ~ZContentDelegate();
};

// Attribute helper class used to wrap up an attribute with a dom
// object that implements nsIDOMAttribute and nsIDOMNode and
// nsIScriptObjectOwner
class DOMAttribute : public nsIDOMAttribute, public nsIScriptObjectOwner {
public:
  DOMAttribute(const nsString &aName, const nsString &aValue);
  ~DOMAttribute();

  NS_DECL_ISUPPORTS

  NS_IMETHOD GetScriptObject(nsIScriptContext* aContext, void** aScriptObject);
  NS_IMETHOD ResetScriptObject();

  // nsIDOMAttribute interface
  NS_IMETHOD GetSpecified(PRBool* aSpecified);
  NS_IMETHOD SetSpecified(PRBool aSpecified);
  NS_IMETHOD GetName(nsString& aReturn);
  NS_IMETHOD GetValue(nsString& aReturn);

  // nsIDOMNode interface
  NS_IMETHOD GetNodeName(nsString& aNodeName);
  NS_IMETHOD GetNodeValue(nsString& aNodeValue);
  NS_IMETHOD SetNodeValue(const nsString& aNodeValue);
  NS_IMETHOD GetNodeType(PRInt32* aNodeType);
  NS_IMETHOD GetParentNode(nsIDOMNode** aParentNode);
  NS_IMETHOD GetChildNodes(nsIDOMNodeList** aChildNodes);
  NS_IMETHOD GetHasChildNodes(PRBool* aHasChildNodes);
  NS_IMETHOD GetFirstChild(nsIDOMNode** aFirstChild);
  NS_IMETHOD GetLastChild(nsIDOMNode** aLastChild);
  NS_IMETHOD GetPreviousSibling(nsIDOMNode** aPreviousSibling);
  NS_IMETHOD GetNextSibling(nsIDOMNode** aNextSibling);
  NS_IMETHOD GetAttributes(nsIDOMNamedNodeMap** aAttributes);
  NS_IMETHOD InsertBefore(nsIDOMNode* aNewChild, nsIDOMNode* aRefChild,
                          nsIDOMNode** aReturn);
  NS_IMETHOD ReplaceChild(nsIDOMNode* aNewChild, nsIDOMNode* aOldChild,
                          nsIDOMNode** aReturn);
  NS_IMETHOD RemoveChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn);
  NS_IMETHOD AppendChild(nsIDOMNode* aNewChild, nsIDOMNode** aReturn);
  NS_IMETHOD CloneNode(nsIDOMNode** aReturn);
  NS_IMETHOD Equals(nsIDOMNode* aNode, PRBool aDeep, PRBool* aReturn);

private:
  nsString mName;
  nsString mValue;
  void* mScriptObject;
};

// Another helper class that implements the nsIDOMNamedNodeMap interface.
class DOMAttributeMap : public nsIDOMNamedNodeMap,
                        public nsIScriptObjectOwner
{
public:
  DOMAttributeMap(nsIHTMLContent &aContent);
  virtual ~DOMAttributeMap();

  NS_DECL_ISUPPORTS

  NS_IMETHOD GetScriptObject(nsIScriptContext* aContext, void** aScriptObject);
  NS_IMETHOD ResetScriptObject();

  // nsIDOMNamedNodeMap interface
  NS_IMETHOD GetLength(PRUint32* aSize);
  NS_IMETHOD GetNamedItem(const nsString& aName, nsIDOMNode** aReturn);
  NS_IMETHOD SetNamedItem(nsIDOMNode* aNode);
  NS_IMETHOD RemoveNamedItem(const nsString& aName, nsIDOMNode** aReturn);
  NS_IMETHOD Item(PRUint32 aIndex, nsIDOMNode** aReturn);

private:
  nsIHTMLContent& mContent;
  void* mScriptObject;
};

//----------------------------------------------------------------------

ZContentDelegate::ZContentDelegate()
{
  NS_INIT_REFCNT();
}

NS_IMPL_ISUPPORTS(ZContentDelegate, kIContentDelegateIID);

ZContentDelegate::~ZContentDelegate()
{
}

NS_METHOD
ZContentDelegate::CreateFrame(nsIPresContext* aPresContext,
                             nsIContent* aContent,
                             nsIFrame* aParentFrame,
                             nsIStyleContext* aStyleContext,
                             nsIFrame*& aResult)
{
  NS_PRECONDITION(nsnull != aContent, "null ptr");

  // Make sure the content is html content
  nsIHTMLContent* hc;
  nsIFrame* frame = nsnull;
  nsresult rv = aContent->QueryInterface(kIHTMLContentIID, (void**) &hc);
  if (NS_OK != rv) {
    // This means that *somehow* somebody which is not an html
    // content object got ahold of this delegate and tried to
    // create a frame with it. Give them back an nsFrame.
    rv = nsFrame::NewFrame(&frame, aContent, aParentFrame);
    if (NS_OK == rv) {
      frame->SetStyleContext(aPresContext, aStyleContext);
    }
  }
  else {
    // Ask the content object to create the frame
    rv = hc->CreateFrame(aPresContext, aParentFrame, aStyleContext, frame);
    NS_RELEASE(hc);
  }
  aResult = frame;
  return rv;
}

//----------------------------------------------------------------------

DOMAttribute::DOMAttribute(const nsString& aName, const nsString& aValue)
  : mName(aName), mValue(aValue)
{
  mRefCnt = 1;
  mScriptObject = nsnull;
}

DOMAttribute::~DOMAttribute()
{
}

nsresult
DOMAttribute::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kIDOMAttributeIID)) {
    nsIDOMAttribute* tmp = this;
    *aInstancePtr = (void*)tmp;
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kIScriptObjectOwnerIID)) {
    nsIScriptObjectOwner* tmp = this;
    *aInstancePtr = (void*)tmp;
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    nsIDOMAttribute* tmp1 = this;
    nsISupports* tmp2 = tmp1;
    *aInstancePtr = (void*)tmp2;
    AddRef();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

NS_IMPL_ADDREF(DOMAttribute)

NS_IMPL_RELEASE(DOMAttribute)

nsresult
DOMAttribute::GetScriptObject(nsIScriptContext *aContext,
                                void** aScriptObject)
{
  nsresult res = NS_OK;
  if (nsnull == mScriptObject) {
    res = NS_NewScriptAttribute(aContext, this, nsnull,
                                (void **)&mScriptObject);
  }
  *aScriptObject = mScriptObject;
  return res;
}

nsresult
DOMAttribute::ResetScriptObject()
{
  mScriptObject = nsnull;
  return NS_OK;
}

nsresult
DOMAttribute::GetName(nsString& aName)
{
  aName = mName;
  return NS_OK;
}

nsresult
DOMAttribute::GetValue(nsString& aValue)
{
  aValue = mValue;
  return NS_OK;
}

nsresult
DOMAttribute::GetSpecified(PRBool* aSpecified)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
DOMAttribute::SetSpecified(PRBool specified)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
DOMAttribute::GetNodeName(nsString& aNodeName)
{
  return GetName(aNodeName);
}

NS_IMETHODIMP
DOMAttribute::GetNodeValue(nsString& aNodeValue)
{
  return GetValue(aNodeValue);
}

NS_IMETHODIMP
DOMAttribute::SetNodeValue(const nsString& aNodeValue)
{
  // You can't actually do this, but we'll fail silently
  return NS_OK;
}

NS_IMETHODIMP
DOMAttribute::GetNodeType(PRInt32* aNodeType)
{
  *aNodeType = (PRInt32)nsIDOMNode::ATTRIBUTE;
  return NS_OK;
}

NS_IMETHODIMP
DOMAttribute::GetParentNode(nsIDOMNode** aParentNode)
{
  *aParentNode = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
DOMAttribute::GetChildNodes(nsIDOMNodeList** aChildNodes)
{
  *aChildNodes = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
DOMAttribute::GetHasChildNodes(PRBool* aHasChildNodes)
{
  *aHasChildNodes = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
DOMAttribute::GetFirstChild(nsIDOMNode** aFirstChild)
{
  *aFirstChild = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
DOMAttribute::GetLastChild(nsIDOMNode** aLastChild)
{
  *aLastChild = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
DOMAttribute::GetPreviousSibling(nsIDOMNode** aPreviousSibling)
{
  *aPreviousSibling = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
DOMAttribute::GetNextSibling(nsIDOMNode** aNextSibling)
{
  *aNextSibling = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
DOMAttribute::GetAttributes(nsIDOMNamedNodeMap** aAttributes)
{
  *aAttributes = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
DOMAttribute::InsertBefore(nsIDOMNode* aNewChild, nsIDOMNode* aRefChild, nsIDOMNode** aReturn)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
DOMAttribute::ReplaceChild(nsIDOMNode* aNewChild, nsIDOMNode* aOldChild, nsIDOMNode** aReturn)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
DOMAttribute::RemoveChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
DOMAttribute::AppendChild(nsIDOMNode* aNewChild, nsIDOMNode** aReturn)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
DOMAttribute::CloneNode(nsIDOMNode** aReturn)
{
  DOMAttribute* newAttr = new DOMAttribute(mName, mValue);
  if (nsnull == newAttr) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  *aReturn = newAttr;
  return NS_OK;
}

NS_IMETHODIMP
DOMAttribute::Equals(nsIDOMNode* aNode, PRBool aDeep, PRBool* aReturn)
{
  // XXX TBI
  return NS_OK;
}

//----------------------------------------------------------------------

DOMAttributeMap::DOMAttributeMap(nsIHTMLContent& aContent)
  : mContent(aContent)
{
  mRefCnt = 1;
  mContent.AddRef();
  mScriptObject = nsnull;
}

DOMAttributeMap::~DOMAttributeMap()
{
  mContent.Release();
}

nsresult
DOMAttributeMap::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kIDOMNamedNodeMapIID)) {
    nsIDOMNamedNodeMap* tmp = this;
    *aInstancePtr = (void*)tmp;
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kIScriptObjectOwnerIID)) {
    nsIScriptObjectOwner* tmp = this;
    *aInstancePtr = (void*)tmp;
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    nsIDOMNamedNodeMap* tmp1 = this;
    nsISupports* tmp2 = tmp1;
    *aInstancePtr = (void*)tmp2;
    AddRef();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

NS_IMPL_ADDREF(DOMAttributeMap)

NS_IMPL_RELEASE(DOMAttributeMap)

nsresult
DOMAttributeMap::GetScriptObject(nsIScriptContext *aContext,
                                   void** aScriptObject)
{
  nsresult res = NS_OK;
  if (nsnull == mScriptObject) {
    res = NS_NewScriptNamedNodeMap(aContext, this, nsnull,
                                   (void**)&mScriptObject);
  }
  *aScriptObject = mScriptObject;
  return res;
}

nsresult
DOMAttributeMap::ResetScriptObject()
{
  mScriptObject = nsnull;
  return NS_OK;
}

nsresult
DOMAttributeMap::GetNamedItem(const nsString &aAttrName,
                                nsIDOMNode** aAttribute)
{
  nsAutoString value;
  mContent.GetAttribute(aAttrName, value);
  *aAttribute  = (nsIDOMNode *) new DOMAttribute(aAttrName, value);
  return NS_OK;
}

nsresult
DOMAttributeMap::SetNamedItem(nsIDOMNode *aNode)
{
  nsIDOMAttribute *attribute;
  nsAutoString name, value;
  nsresult err;

  if (NS_OK != (err = aNode->QueryInterface(kIDOMAttributeIID,
                                            (void **)&attribute))) {
    return err;
  }

  attribute->GetName(name);
  attribute->GetValue(value);
  NS_RELEASE(attribute);

  mContent.SetAttribute(name, value);
  return NS_OK;
}

NS_IMETHODIMP
DOMAttributeMap::RemoveNamedItem(const nsString& aName, nsIDOMNode** aReturn)
{
  nsresult res = GetNamedItem(aName, aReturn);
  if (NS_OK == res) {
    nsAutoString upper;
    aName.ToUpperCase(upper);
    nsIAtom* attr = NS_NewAtom(upper);
    mContent.UnsetAttribute(attr);
  }

  return res;
}

nsresult
DOMAttributeMap::Item(PRUint32 aIndex, nsIDOMNode** aReturn)
{
  nsresult res = NS_ERROR_FAILURE;
  nsAutoString name, value;
  nsISupportsArray *attributes = nsnull;
  if (NS_OK == NS_NewISupportsArray(&attributes)) {
    PRInt32 count = mContent.GetAllAttributeNames(attributes);
    if (count > 0) {
      if ((PRInt32)aIndex < count) {
        nsISupports *att = attributes->ElementAt(aIndex);
        static NS_DEFINE_IID(kIAtom, NS_IATOM_IID);
        nsIAtom *atName = nsnull;
        if (nsnull != att && NS_OK == att->QueryInterface(kIAtom, (void**)&atName)) {
          atName->ToString(name);
          if (eContentAttr_NotThere != mContent.GetAttribute(name, value)) {
            *aReturn = (nsIDOMNode *)new DOMAttribute(name, value);
            res = NS_OK;
          }
          NS_RELEASE(atName);
        }
      }
    }
    NS_RELEASE(attributes);
  }

  return res;
}

nsresult
DOMAttributeMap::GetLength(PRUint32 *aLength)
{
  *aLength = mContent.GetAttributeCount();
  return NS_OK;
}

//----------------------------------------------------------------------

nsHTMLGenericContent::nsHTMLGenericContent()
{
  mDocument = nsnull;
  mParent = nsnull;
  mAttributes = nsnull;
  mTag = nsnull;
  mContent = nsnull;
  mScriptObject = nsnull;
  mListenerManager = nsnull;

  // Create shared content delegate if this is the first html content
  // object being created.
  if (nsnull == gContentDelegate) {
    gContentDelegate = new ZContentDelegate();
  }

  // Add a reference to the shared content delegate object
  NS_ADDREF(gContentDelegate);
}

nsHTMLGenericContent::~nsHTMLGenericContent()
{
  NS_IF_RELEASE(mAttributes);
  NS_IF_RELEASE(mTag);
  NS_IF_RELEASE(mListenerManager);
  // XXX what about mScriptObject? it's now safe to GC it...

  NS_PRECONDITION(nsnull != gContentDelegate, "null content delegate");
  if (nsnull != gContentDelegate) {
    // Remove our reference to the shared content delegate object.  If
    // the last reference just went away, null out gContentDelegate.
    nsrefcnt rc = gContentDelegate->Release();
    if (0 == rc) {
      gContentDelegate = nsnull;
    }
  }
}

void
nsHTMLGenericContent::Init(nsIHTMLContent* aOuterContentObject,
                           nsIAtom* aTag)
{
  NS_ASSERTION((nsnull == mContent) && (nsnull != aOuterContentObject),
               "null ptr");
  mContent = aOuterContentObject;
  mTag = aTag;
  NS_IF_ADDREF(aTag);
}

nsresult
nsHTMLGenericContent::GetNodeName(nsString& aNodeName)
{
  return GetTagName(aNodeName);
}

nsresult
nsHTMLGenericContent::GetNodeValue(nsString& aNodeValue)
{
  aNodeValue.Truncate();
  return NS_OK;
}

nsresult
nsHTMLGenericContent::SetNodeValue(const nsString& aNodeValue)
{
  return NS_OK;
}

nsresult
nsHTMLGenericContent::GetNodeType(PRInt32* aNodeType)
{
  *aNodeType = nsIDOMNode::ELEMENT;
  return NS_OK;
}

nsresult
nsHTMLGenericContent::GetParentNode(nsIDOMNode** aParentNode)
{
  if (nsnull != mParent) {
    nsresult res = mParent->QueryInterface(kIDOMNodeIID, (void**)aParentNode);
    NS_ASSERTION(NS_OK == res, "Must be a DOM Node");
    return res;
  }
  else {
    *aParentNode = nsnull;
  }
  return NS_OK;
}

nsresult
nsHTMLGenericContent::GetPreviousSibling(nsIDOMNode** aNode)
{
  if (nsnull != mParent) {
    PRInt32 pos = mParent->IndexOf(mContent);
    if (pos > -1) {
      nsIContent* prev = mParent->ChildAt(--pos);
      if (nsnull != prev) {
        nsresult res = prev->QueryInterface(kIDOMNodeIID, (void**)aNode);
        NS_ASSERTION(NS_OK == res, "Must be a DOM Node");
        NS_RELEASE(prev); // balance the AddRef in ChildAt()
        return res;
      }
    }
  }
  *aNode = nsnull;
  return NS_OK;
}

nsresult
nsHTMLGenericContent::GetNextSibling(nsIDOMNode** aNextSibling)
{
  if (nsnull != mParent) {
    PRInt32 pos = mParent->IndexOf(mContent);
    if (pos > -1 ) {
      nsIContent* prev = mParent->ChildAt(++pos);
      if (nsnull != prev) {
        nsresult res = prev->QueryInterface(kIDOMNodeIID,(void**)aNextSibling);
        NS_ASSERTION(NS_OK == res, "Must be a DOM Node");
        NS_RELEASE(prev); // balance the AddRef in ChildAt()
        return res;
      }
    }
  }
  *aNextSibling = nsnull;
  return NS_OK;
}

nsresult
nsHTMLGenericContent::GetAttributes(nsIDOMNamedNodeMap** aAttributes)
{
  NS_PRECONDITION(nsnull != aAttributes, "null pointer argument");
  if (nsnull != mAttributes) {
    // XXX Should we create a new one every time or should we
    // cache one after we create it? If we find that this is
    // something that's called often, we might need to do the
    // latter.
    *aAttributes = new DOMAttributeMap(*mContent);
  }
  else {
    *aAttributes = nsnull;
  }
  return NS_OK;
}

nsresult
nsHTMLGenericContent::GetTagName(nsString& aTagName)
{
  aTagName.Truncate();
  if (nsnull != mTag) {
    mTag->ToString(aTagName);
  }
  return NS_OK;
}

nsresult
nsHTMLGenericContent::GetDOMAttribute(const nsString& aName, nsString& aReturn)
{
  GetAttribute(aName, aReturn);
  return NS_OK;
}

nsresult
nsHTMLGenericContent::SetDOMAttribute(const nsString& aName,
                                      const nsString& aValue)
{
  SetAttribute(aName, aValue);
  return NS_OK;
}

nsresult
nsHTMLGenericContent::RemoveAttribute(const nsString& aName)
{
  nsAutoString upper;
  aName.ToUpperCase(upper);
  nsIAtom* attr = NS_NewAtom(upper);
  UnsetAttribute(attr);
  NS_RELEASE(attr);
  return NS_OK;
}

nsresult
nsHTMLGenericContent::GetAttributeNode(const nsString& aName,
                                       nsIDOMAttribute** aReturn)
{
  nsAutoString value;
  if (eContentAttr_NotThere != GetAttribute(aName, value)) {
    *aReturn = new DOMAttribute(aName, value);
  }
  return NS_OK;
}

nsresult
nsHTMLGenericContent::SetAttributeNode(nsIDOMAttribute* aAttribute)
{
  NS_PRECONDITION(nsnull != aAttribute, "null attribute");

  nsresult res = NS_ERROR_FAILURE;

  if (nsnull != aAttribute) {
    nsAutoString name, value;
    res = aAttribute->GetName(name);
    if (NS_OK == res) {
      res = aAttribute->GetValue(value);
      if (NS_OK == res) {
        SetAttribute(name, value);
      }
    }
  }
  return res;
}

nsresult
nsHTMLGenericContent::RemoveAttributeNode(nsIDOMAttribute* aAttribute)
{
  NS_PRECONDITION(nsnull != aAttribute, "null attribute");

  nsresult res = NS_ERROR_FAILURE;

  if (nsnull != aAttribute) {
    nsAutoString name;
    res = aAttribute->GetName(name);
    if (NS_OK == res) {
      nsAutoString upper;
      name.ToUpperCase(upper);
      nsIAtom* attr = NS_NewAtom(upper);
      UnsetAttribute(attr);
    }
  }

  return res;
}

nsresult
nsHTMLGenericContent::GetElementsByTagName(const nsString& aTagname,
                                           nsIDOMNodeList** aReturn)
{
  return NS_ERROR_NOT_IMPLEMENTED;/* XXX */
}

nsresult
nsHTMLGenericContent::Normalize()
{
  return NS_ERROR_NOT_IMPLEMENTED;/* XXX */
}

nsresult
nsHTMLGenericContent::GetId(nsString& aId)
{
  GetAttribute(nsHTMLAtoms::id, aId);
  return NS_OK;
}

nsresult
nsHTMLGenericContent::SetId(const nsString& aId)
{
  SetAttribute(nsHTMLAtoms::id, aId);
  return NS_OK;
}

nsresult
nsHTMLGenericContent::GetTitle(nsString& aTitle)
{
  GetAttribute(nsHTMLAtoms::title, aTitle);
  return NS_OK;
}

nsresult
nsHTMLGenericContent::SetTitle(const nsString& aTitle)
{
  SetAttribute(nsHTMLAtoms::title, aTitle);
  return NS_OK;
}

nsresult
nsHTMLGenericContent::GetLang(nsString& aLang)
{
  GetAttribute(nsHTMLAtoms::lang, aLang);
  return NS_OK;
}

nsresult
nsHTMLGenericContent::SetLang(const nsString& aLang)
{
  SetAttribute(nsHTMLAtoms::lang, aLang);
  return NS_OK;
}

nsresult
nsHTMLGenericContent::GetDir(nsString& aDir)
{
  GetAttribute(nsHTMLAtoms::dir, aDir);
  return NS_OK;
}

nsresult
nsHTMLGenericContent::SetDir(const nsString& aDir)
{
  SetAttribute(nsHTMLAtoms::dir, aDir);
  return NS_OK;
}

nsresult
nsHTMLGenericContent::GetClassName(nsString& aClassName)
{
  GetAttribute(nsHTMLAtoms::kClass, aClassName);
  return NS_OK;
}

nsresult
nsHTMLGenericContent::SetClassName(const nsString& aClassName)
{
  SetAttribute(nsHTMLAtoms::kClass, aClassName);
  return NS_OK;
}

nsresult
nsHTMLGenericContent::GetDocument(nsIDocument*& aResult) const
{
  aResult = mDocument;
  NS_IF_ADDREF(mDocument);
  return NS_OK;
}

void
nsHTMLGenericContent::SetDocument(nsIDocument* aDocument)
{
  mDocument = aDocument;

  // Once the element is added to the doc tree we need to check if
  // event handler were registered on it.  Unfortunately, this means
  // doing a GetAttribute for every type of handler.
  if (nsnull != mAttributes) {
    nsHTMLValue val;
    if (eContentAttr_HasValue == mAttributes->GetAttribute(nsHTMLAtoms::onclick, val))
      AddScriptEventListener(nsHTMLAtoms::onclick, val, kIDOMMouseListenerIID);
    if (eContentAttr_HasValue == mAttributes->GetAttribute(nsHTMLAtoms::ondblclick, val))
      AddScriptEventListener(nsHTMLAtoms::onclick, val, kIDOMMouseListenerIID);
    if (eContentAttr_HasValue == mAttributes->GetAttribute(nsHTMLAtoms::onmousedown, val))
      AddScriptEventListener(nsHTMLAtoms::onmousedown, val, kIDOMMouseListenerIID);
    if (eContentAttr_HasValue == mAttributes->GetAttribute(nsHTMLAtoms::onmouseup, val))
      AddScriptEventListener(nsHTMLAtoms::onmouseup, val, kIDOMMouseListenerIID);
    if (eContentAttr_HasValue == mAttributes->GetAttribute(nsHTMLAtoms::onmouseover, val))
      AddScriptEventListener(nsHTMLAtoms::onmouseover, val, kIDOMMouseListenerIID);
    if (eContentAttr_HasValue == mAttributes->GetAttribute(nsHTMLAtoms::onmouseout, val))
      AddScriptEventListener(nsHTMLAtoms::onmouseout, val, kIDOMMouseListenerIID);
    if (eContentAttr_HasValue == mAttributes->GetAttribute(nsHTMLAtoms::onkeydown, val))
      AddScriptEventListener(nsHTMLAtoms::onkeydown, val, kIDOMKeyListenerIID);
    if (eContentAttr_HasValue == mAttributes->GetAttribute(nsHTMLAtoms::onkeyup, val))
      AddScriptEventListener(nsHTMLAtoms::onkeyup, val, kIDOMKeyListenerIID);
    if (eContentAttr_HasValue == mAttributes->GetAttribute(nsHTMLAtoms::onkeypress, val))
      AddScriptEventListener(nsHTMLAtoms::onkeypress, val, kIDOMKeyListenerIID);
    if (eContentAttr_HasValue == mAttributes->GetAttribute(nsHTMLAtoms::onmousemove, val))
      AddScriptEventListener(nsHTMLAtoms::onmousemove, val, kIDOMMouseMotionListenerIID);
    if (eContentAttr_HasValue == mAttributes->GetAttribute(nsHTMLAtoms::onload, val))
      AddScriptEventListener(nsHTMLAtoms::onload, val, kIDOMLoadListenerIID);
    if (eContentAttr_HasValue == mAttributes->GetAttribute(nsHTMLAtoms::onunload, val))
      AddScriptEventListener(nsHTMLAtoms::onunload, val, kIDOMLoadListenerIID);
    if (eContentAttr_HasValue == mAttributes->GetAttribute(nsHTMLAtoms::onabort, val))
      AddScriptEventListener(nsHTMLAtoms::onabort, val, kIDOMLoadListenerIID);
    if (eContentAttr_HasValue == mAttributes->GetAttribute(nsHTMLAtoms::onerror, val))
      AddScriptEventListener(nsHTMLAtoms::onerror, val, kIDOMLoadListenerIID);
    if (eContentAttr_HasValue == mAttributes->GetAttribute(nsHTMLAtoms::onfocus, val))
      AddScriptEventListener(nsHTMLAtoms::onfocus, val, kIDOMFocusListenerIID);
    if (eContentAttr_HasValue == mAttributes->GetAttribute(nsHTMLAtoms::onblur, val))
      AddScriptEventListener(nsHTMLAtoms::onblur, val, kIDOMFocusListenerIID);
  }
}

nsIContent*
nsHTMLGenericContent::GetParent() const
{
  NS_IF_ADDREF(mParent);
  return mParent;
}

void
nsHTMLGenericContent::SetParent(nsIContent* aParent)
{
  mParent = aParent;
}

nsresult
nsHTMLGenericContent::IsSynthetic(PRBool& aResult)
{
  return PR_FALSE;
}

nsIAtom*
nsHTMLGenericContent::GetTag() const
{
  NS_IF_ADDREF(mTag);
  return mTag;
}

//void
//nsHTMLTagContent::SizeOfWithoutThis(nsISizeOfHandler* aHandler) const
//{
//  if (!aHandler->HaveSeen(mTag)) {
//    mTag->SizeOf(aHandler);
//  }
//  if (!aHandler->HaveSeen(mAttributes)) {
//    mAttributes->SizeOf(aHandler);
//  }
//}

nsresult
nsHTMLGenericContent::HandleDOMEvent(nsIPresContext& aPresContext,
                                     nsEvent* aEvent,
                                     nsIDOMEvent** aDOMEvent,
                                     PRUint32 aFlags,
                                     nsEventStatus& aEventStatus)
{
  nsresult ret = NS_OK;
  
  nsIDOMEvent* domEvent = nsnull;
  if (DOM_EVENT_INIT == aFlags) {
    nsIEventStateManager *manager;
    if (NS_OK == aPresContext.GetEventStateManager(&manager)) {
      manager->SetEventTarget(mContent);
      NS_RELEASE(manager);
    }
    aDOMEvent = &domEvent;
  }
  
  //Capturing stage
  
  //Local handling stage
  if (nsnull != mListenerManager) {
    mListenerManager->HandleEvent(aPresContext, aEvent, aDOMEvent, aEventStatus);
  }

  //Bubbling stage
  if (DOM_EVENT_CAPTURE != aFlags && mParent != nsnull) {
    ret = mParent->HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                                  DOM_EVENT_BUBBLE, aEventStatus);
  }

  if (DOM_EVENT_INIT == aFlags) {
    // We're leaving the DOM event loop so if we created a DOM event,
    // release here.
    if (nsnull != *aDOMEvent) {
      if (0 != (*aDOMEvent)->Release()) {
        // Okay, so someone in the DOM loop (a listener, JS object)
        // still has a ref to the DOM Event but the internal data
        // hasn't been malloc'd.  Force a copy of the data here so the
        // DOM Event is still valid.
        nsIPrivateDOMEvent *privateEvent;
        if (NS_OK == (*aDOMEvent)->QueryInterface(kIPrivateDOMEventIID, (void**)&privateEvent)) {
          privateEvent->DuplicatePrivateData();
          NS_RELEASE(privateEvent);
        }
      }
    }
    aDOMEvent = nsnull;
  }

  if (NS_OK == ret && nsEventStatus_eIgnore == aEventStatus) {
    switch (aEvent->message) {
    case NS_MOUSE_LEFT_BUTTON_DOWN:
      if (mTag == nsHTMLAtoms::a) {
        nsIEventStateManager *stateManager;
        if (NS_OK == aPresContext.GetEventStateManager(&stateManager)) {
          stateManager->SetActiveLink(mContent);
          NS_RELEASE(stateManager);
        }
        aEventStatus = nsEventStatus_eConsumeNoDefault; 
      }
      break;

    case NS_MOUSE_LEFT_BUTTON_UP:
      if (mTag == nsHTMLAtoms::a) {
        nsIEventStateManager *stateManager;
        nsIContent *activeLink;
        if (NS_OK == aPresContext.GetEventStateManager(&stateManager)) {
          stateManager->GetActiveLink(&activeLink);
          NS_RELEASE(stateManager);
        }

        if (activeLink == mContent) {
          nsEventStatus status;
          nsMouseEvent event;
          event.eventStructType = NS_MOUSE_EVENT;
          event.message = NS_MOUSE_LEFT_CLICK;
          HandleDOMEvent(aPresContext, &event, nsnull, DOM_EVENT_INIT, status);

          if (nsEventStatus_eConsumeNoDefault != status) {
            nsAutoString base, href, target;
            GetAttribute(nsString(NS_HTML_BASE_HREF), base);
            GetAttribute(nsString("href"), href);
            GetAttribute(nsString("target"), target);
            if (target.Length() == 0) {
              GetAttribute(nsString(NS_HTML_BASE_TARGET), target);
            }
//XXX            TriggerLink(aPresContext, base, href, target, PR_TRUE);
            aEventStatus = nsEventStatus_eConsumeNoDefault; 
          }
        }
      }
      break;

    case NS_MOUSE_RIGHT_BUTTON_DOWN:
      // XXX Bring up a contextual menu provided by the application
      break;

    case NS_MOUSE_ENTER:
    //mouse enter doesn't work yet.  Use move until then.
      if (mTag == nsHTMLAtoms::a) {
        nsAutoString base, href, target;
        GetAttribute(nsString(NS_HTML_BASE_HREF), base);
        GetAttribute(nsString("href"), href);
        GetAttribute(nsString("target"), target);
        if (target.Length() == 0) {
          GetAttribute(nsString(NS_HTML_BASE_TARGET), target);
        }
//XXX        TriggerLink(aPresContext, base, href, target, PR_FALSE);
        aEventStatus = nsEventStatus_eConsumeDoDefault; 
      }
      break;

      // XXX this doesn't seem to do anything yet
    case NS_MOUSE_EXIT:
      if (mTag == nsHTMLAtoms::a) {
        nsAutoString empty;
//XXX        TriggerLink(aPresContext, empty, empty, empty, PR_FALSE);
        aEventStatus = nsEventStatus_eConsumeDoDefault; 
      }
      break;

    default:
      break;
    }
  }
  return ret;
}

void
nsHTMLGenericContent::SetAttribute(const nsString& aName,
                                   const nsString& aValue)
{
  nsAutoString upper;
  aName.ToUpperCase(upper);
  nsIAtom* attr = NS_NewAtom(upper);
  SetAttribute(attr, aValue);
  NS_RELEASE(attr);
}

void
nsHTMLGenericContent::SetAttribute(nsIAtom* aAttribute,
                                   const nsString& aValue)
{
  if (nsnull == mAttributes) {
    NS_NewHTMLAttributes(&mAttributes, mContent);
  }
  if (nsnull != mAttributes) {
    nsHTMLValue hval;
    if (eContentAttr_HasValue ==
        mContent->StringToAttribute(aAttribute, aValue, hval)) {
      mAttributes->SetAttribute(aAttribute, hval);
    }
    else if (nsHTMLAtoms::style == aAttribute) {
      // XXX the style sheet language is a document property that
      // should be used to lookup the style sheet parser to parse the
      // attribute.
      nsICSSParser* css;
      nsresult rv = NS_NewCSSParser(&css);
      if (NS_OK != rv) {
        return;
      }
      nsIStyleRule* rule;
      rv = css->ParseDeclarations(aValue, nsnull, rule);
      if ((NS_OK == rv) && (nsnull != rule)) {
        mAttributes->SetAttribute(aAttribute, nsHTMLValue(rule));
        NS_RELEASE(rule);
      }
      NS_RELEASE(css);
    }
    else {
      mAttributes->SetAttribute(aAttribute, aValue);
    }
  }
}

void
nsHTMLGenericContent::SetAttribute(nsIAtom* aAttribute,
                                   const nsHTMLValue& aValue)
{
  if (nsnull == mAttributes) {
    NS_NewHTMLAttributes(&mAttributes, mContent);
  }
  if (nsnull != mAttributes) {
    mAttributes->SetAttribute(aAttribute, aValue);
  }
}

void
nsHTMLGenericContent::UnsetAttribute(nsIAtom* aAttribute)
{
  if (nsnull != mAttributes) {
    PRInt32 count = mAttributes->UnsetAttribute(aAttribute);
    if (0 == count) {
      NS_RELEASE(mAttributes);
    }
  }
}

nsContentAttr
nsHTMLGenericContent::GetAttribute(const nsString& aName,
                                   nsString& aResult) const
{
  nsAutoString upper;
  aName.ToUpperCase(upper);
  nsIAtom* attr = NS_NewAtom(upper);
  nsContentAttr result = GetAttribute(attr, aResult);
  NS_RELEASE(attr);
  return result;
}


nsContentAttr
nsHTMLGenericContent::GetAttribute(nsIAtom *aAttribute,
                                   nsString &aResult) const
{
  nsHTMLValue value;
  nsContentAttr result = GetAttribute(aAttribute, value);

  char cbuf[20];
  nscolor color;
  if (eContentAttr_HasValue == result) {
    // Try subclass conversion routine first
    if (eContentAttr_HasValue ==
        mContent->AttributeToString(aAttribute, value, aResult)) {
      return result;
    }

    // Provide default conversions for most everything
    switch (value.GetUnit()) {
    case eHTMLUnit_Empty:
      aResult.Truncate();
      break;

    case eHTMLUnit_String:
    case eHTMLUnit_Null:
      value.GetStringValue(aResult);
      break;

    case eHTMLUnit_Integer:
      aResult.Truncate();
      aResult.Append(value.GetIntValue(), 10);
      break;

    case eHTMLUnit_Pixel:
      aResult.Truncate();
      aResult.Append(value.GetPixelValue(), 10);
      break;

    case eHTMLUnit_Percent:
      aResult.Truncate(0);
      aResult.Append(PRInt32(value.GetPercentValue() * 100.0f), 10);
      aResult.Append('%');
      break;

    case eHTMLUnit_Color:
      color = nscolor(value.GetColorValue());
      PR_snprintf(cbuf, sizeof(cbuf), "#%02x%02x%02x",
                  NS_GET_R(color), NS_GET_G(color), NS_GET_B(color));
      aResult.Truncate(0);
      aResult.Append(cbuf);
      break;

    default:
    case eHTMLUnit_Enumerated:
      NS_NOTREACHED("no default enumerated value to string conversion");
      result = eContentAttr_NotThere;
      break;
    }
  }
  return result;
}

nsContentAttr
nsHTMLGenericContent::GetAttribute(nsIAtom* aAttribute,
                                   nsHTMLValue& aValue) const
{
  if (nsnull != mAttributes) {
    return mAttributes->GetAttribute(aAttribute, aValue);
  }
  aValue.Reset();
  return eContentAttr_NotThere;
}

PRInt32
nsHTMLGenericContent::GetAllAttributeNames(nsISupportsArray* aArray) const
{
  if (nsnull != mAttributes) {
    return mAttributes->GetAllAttributeNames(aArray);
  }
  return 0;
}

PRInt32
nsHTMLGenericContent::GetAttributeCount(void) const
{
  if (nsnull != mAttributes) {
    return mAttributes->Count();
  }
  return 0;
}

void
nsHTMLGenericContent::SetID(nsIAtom* aID)
{
  if (nsnull != aID) {
    if (nsnull == mAttributes) {
      NS_NewHTMLAttributes(&mAttributes, mContent);
    }
    if (nsnull != mAttributes) {
      mAttributes->SetID(aID);
    }
  }
  else {
    if (nsnull != mAttributes) {
      PRInt32 count = mAttributes->SetID(nsnull);
      if (0 == count) {
        NS_RELEASE(mAttributes);
      }
    }
  }
}

nsIAtom*
nsHTMLGenericContent::GetID(void) const
{
  if (nsnull != mAttributes) {
    return mAttributes->GetID();
  }
  return nsnull;
}

void
nsHTMLGenericContent::SetClass(nsIAtom* aClass)
{
  if (nsnull != aClass) {
    if (nsnull == mAttributes) {
      NS_NewHTMLAttributes(&mAttributes, mContent);
    }
    if (nsnull != mAttributes) {
      mAttributes->SetClass(aClass);
    }
  }
  else {
    if (nsnull != mAttributes) {
      PRInt32 count = mAttributes->SetClass(nsnull);
      if (0 == count) {
        NS_RELEASE(mAttributes);
      }
    }
  }
}

nsIAtom*
nsHTMLGenericContent::GetClass(void) const
{
  if (nsnull != mAttributes) {
    return mAttributes->GetClass();
  }
  return nsnull;
}

nsIStyleRule*
nsHTMLGenericContent::GetStyleRule(void)
{
  nsIStyleRule* result = nsnull;

  if (nsnull != mAttributes) {
    mAttributes->QueryInterface(kIStyleRuleIID, (void**)&result);
  }
  return result;
}

nsIContentDelegate*
nsHTMLGenericContent::GetDelegate(nsIPresContext* aCX)
{
  gContentDelegate->AddRef();
  return gContentDelegate;
}

void
nsHTMLGenericContent::ListAttributes(FILE* out) const
{
  nsISupportsArray* attrs;
  if (NS_OK == NS_NewISupportsArray(&attrs)) {
    GetAllAttributeNames(attrs);
    PRInt32 count = attrs->Count();
    PRInt32 index;
    for (index = 0; index < count; index++) {
      // name
      nsIAtom* attr = (nsIAtom*)attrs->ElementAt(index);
      nsAutoString buffer;
      attr->ToString(buffer);

      // value
      nsAutoString value;
      GetAttribute(buffer, value);
      buffer.Append("=");
      buffer.Append(value);

      fputs(" ", out);
      fputs(buffer, out);
      NS_RELEASE(attr);
    }
    NS_RELEASE(attrs);
  }
}

void
nsHTMLGenericContent::List(FILE* out, PRInt32 aIndent) const
{
  NS_PRECONDITION(nsnull != mDocument, "bad content");

  PRInt32 index;
  for (index = aIndent; --index >= 0; ) fputs("  ", out);

  nsIAtom* tag = GetTag();
  if (tag != nsnull) {
    nsAutoString buf;
    tag->ToString(buf);
    fputs(buf, out);
    NS_RELEASE(tag);
  }

  ListAttributes(out);

//  fprintf(out, " RefCount=%d<\n", mRefCnt);

  if (mContent->CanContainChildren()) {
    PRInt32 kids = mContent->ChildCount();
    for (index = 0; index < kids; index++) {
      nsIContent* kid = mContent->ChildAt(index);
      kid->List(out, aIndent + 1);
      NS_RELEASE(kid);
    }
  }

  for (index = aIndent; --index >= 0; ) fputs("  ", out);
  fputs(">\n", out);
}

void
nsHTMLGenericContent::ToHTML(FILE* out) const
{
  nsAutoString tmp;
  ToHTMLString(tmp);
  fputs(tmp, out);
}
// XXX i18n: this is wrong (?) because we need to know the outgoing
// character set (I think)
static void
QuoteForHTML(const nsString& aValue, nsString& aResult)
{
  aResult.Truncate();
  const PRUnichar* cp = aValue.GetUnicode();
  const PRUnichar* end = aValue.GetUnicode() + aValue.Length();
  aResult.Append('"');
  while (cp < end) {
    PRUnichar ch = *cp++;
    if ((ch >= 0x20) && (ch <= 0x7f)) {
      if (ch == '\"') {
        aResult.Append("&quot;");
      }
      else {
        aResult.Append(ch);
      }
    }
    else {
      aResult.Append("&#");
      aResult.Append((PRInt32) ch, 10);
      aResult.Append(';');
    }
  }
  aResult.Append('"');
}

void
nsHTMLGenericContent::ToHTMLString(nsString& aBuf) const
{
  aBuf.Truncate(0);
  aBuf.Append('<');

  if (nsnull != mTag) {
    nsAutoString tmp;
    mTag->ToString(tmp);
    aBuf.Append(tmp);
  } else {
    aBuf.Append("?NULL");
  }

  if (nsnull != mAttributes) {
    nsISupportsArray* attrs;
    nsresult rv = NS_NewISupportsArray(&attrs);
    if (NS_OK == rv) {
      mAttributes->GetAllAttributeNames(attrs);
      PRInt32 i, n = attrs->Count();
      nsAutoString name, value, quotedValue;
      for (i = 0; i < n; i++) {
        nsIAtom* atom = (nsIAtom*) attrs->ElementAt(i);
        atom->ToString(name);
        aBuf.Append(' ');
        aBuf.Append(name);
        value.Truncate();
        GetAttribute(name, value);
        if (value.Length() > 0) {
          aBuf.Append('=');
          QuoteForHTML(value, quotedValue);
          aBuf.Append(quotedValue);
        }
      }
      NS_RELEASE(attrs);
    }
  }

  aBuf.Append('>');
}

//----------------------------------------------------------------------

// XXX this is REALLY temporary code

extern nsresult NS_NewHRFrame(nsIContent* aContent, nsIFrame* aParentFrame,
                              nsIFrame*& aNewFrame);

nsresult
nsHTMLGenericContent::CreateFrame(nsIPresContext*  aPresContext,
                                  nsIFrame*        aParentFrame,
                                  nsIStyleContext* aStyleContext,
                                  nsIFrame*&       aResult)
{
  nsIFrame* frame = nsnull;
  nsresult rv = NS_OK;
  if (mTag == nsHTMLAtoms::hr) {
    rv = NS_NewHRFrame(mContent, aParentFrame, frame);
  }

  if (nsnull == frame) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  frame->SetStyleContext(aPresContext, aStyleContext);
  aResult = frame;
  return NS_OK;
}

//----------------------------------------------------------------------

// nsIScriptObjectOwner implementation

nsresult
nsHTMLGenericContent::GetScriptObject(nsIScriptContext* aContext,
                                      void** aScriptObject)
{
  nsresult res = NS_OK;
  if (nsnull == mScriptObject) {
    nsIDOMElement* ele = nsnull;
    mContent->QueryInterface(kIDOMElementIID, (void**) &ele);
    res = NS_NewScriptElement(aContext, ele, mParent, (void**)&mScriptObject);
    NS_RELEASE(ele);
  }
  *aScriptObject = mScriptObject;
  return res;
}

nsresult
nsHTMLGenericContent::ResetScriptObject()
{
  mScriptObject = nsnull;
  return NS_OK;
}

//----------------------------------------------------------------------

// nsIDOMEventReceiver implementation

nsresult
nsHTMLGenericContent::GetListenerManager(nsIEventListenerManager** aResult)
{
  if (nsnull != mListenerManager) {
    NS_ADDREF(mListenerManager);
    *aResult = mListenerManager;
    return NS_OK;
  }
  nsresult rv = NS_NewEventListenerManager(aResult);
  if (NS_OK == rv) {
    mListenerManager = *aResult;
    NS_ADDREF(mListenerManager);
  }
  return rv;
}

nsresult
nsHTMLGenericContent::GetNewListenerManager(nsIEventListenerManager** aResult)
{
  return NS_NewEventListenerManager(aResult);
} 

nsresult
nsHTMLGenericContent::AddEventListener(nsIDOMEventListener* aListener,
                                       const nsIID& aIID)
{
  nsIEventListenerManager *manager;

  if (NS_OK == GetListenerManager(&manager)) {
    manager->AddEventListener(aListener, aIID);
    NS_RELEASE(manager);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

nsresult
nsHTMLGenericContent::RemoveEventListener(nsIDOMEventListener* aListener,
                                          const nsIID& aIID)
{
  if (nsnull != mListenerManager) {
    mListenerManager->RemoveEventListener(aListener, aIID);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

//----------------------------------------------------------------------

nsresult
nsHTMLGenericContent::AddScriptEventListener(nsIAtom* aAttribute,
                                             nsHTMLValue& aValue,
                                             REFNSIID aIID)
{
  nsresult ret = NS_OK;
  nsIScriptContext* context;
  nsIScriptContextOwner* owner;

  if (nsnull != mDocument) {
    owner = mDocument->GetScriptContextOwner();
    if (NS_OK == owner->GetScriptContext(&context)) {
      if (nsHTMLAtoms::body == mTag || nsHTMLAtoms::frameset == mTag) {
        nsIDOMEventReceiver *receiver;
        nsIScriptGlobalObject *global = context->GetGlobalObject();

        if (nsnull != global && NS_OK == global->QueryInterface(kIDOMEventReceiverIID, (void**)&receiver)) {
          nsIEventListenerManager *manager;
          if (NS_OK == receiver->GetListenerManager(&manager)) {
            nsIScriptObjectOwner *mObjectOwner;
            if (NS_OK == global->QueryInterface(kIScriptObjectOwnerIID, (void**)&mObjectOwner)) {
              nsString value;
              aValue.GetStringValue(value);
              ret = manager->AddScriptEventListener(context, mObjectOwner, aAttribute, value, aIID);
              NS_RELEASE(mObjectOwner);
            }
            NS_RELEASE(manager);
          }
          NS_RELEASE(receiver);
        }
        NS_IF_RELEASE(global);
      }
      else {
        nsIEventListenerManager *manager;
        if (NS_OK == GetListenerManager(&manager)) {
          nsString value;
          aValue.GetStringValue(value);
          nsIScriptObjectOwner* owner;
          if (NS_OK == mContent->QueryInterface(kIScriptObjectOwnerIID,
                                                (void**) &owner)) {
            ret = manager->AddScriptEventListener(context, owner,
                                                  aAttribute, value, aIID);
            NS_RELEASE(owner);
          }
          NS_RELEASE(manager);
        }
      }
      NS_RELEASE(context);
    }
    NS_RELEASE(owner);
  }
  return ret;
}

nsContentAttr
nsHTMLGenericContent::AttributeToString(nsIAtom* aAttribute,
                                        nsHTMLValue& aValue,
                                        nsString& aResult) const
{
  if (nsHTMLAtoms::style == aAttribute) {
    if (eHTMLUnit_ISupports == aValue.GetUnit()) {
      nsIStyleRule* rule = (nsIStyleRule*) aValue.GetISupportsValue();
      // rule->ToString(str);
      aResult = "XXX style rule ToString goes here";
      return eContentAttr_HasValue;
    }
  }
  aResult.Truncate();
  return eContentAttr_NotThere;
}

PRBool
nsHTMLGenericContent::ParseEnumValue(const nsString& aValue,
                                     EnumTable* aTable,
                                     nsHTMLValue& aResult)
{
  while (nsnull != aTable->tag) {
    if (aValue.EqualsIgnoreCase(aTable->tag)) {
      aResult.SetIntValue(aTable->value, eHTMLUnit_Enumerated);
      return PR_TRUE;
    }
    aTable++;
  }
  return PR_FALSE;
}

PRBool
nsHTMLGenericContent::EnumValueToString(const nsHTMLValue& aValue,
                                        EnumTable* aTable,
                                        nsString& aResult)
{
  aResult.Truncate(0);
  if (aValue.GetUnit() == eHTMLUnit_Enumerated) {
    PRInt32 v = aValue.GetIntValue();
    while (nsnull != aTable->tag) {
      if (aTable->value == v) {
        aResult.Append(aTable->tag);
        return PR_TRUE;
      }
      aTable++;
    }
  }
  return PR_FALSE;
}

PRBool
nsHTMLGenericContent::ParseValueOrPercent(const nsString& aString,
                                          nsHTMLValue& aResult, 
                                          nsHTMLUnit aValueUnit)
{ // XXX should vave min/max values?
  nsAutoString tmp(aString);
  tmp.CompressWhitespace(PR_TRUE, PR_TRUE);
  PRInt32 ec, val = tmp.ToInteger(&ec);
  if (NS_OK == ec) {
    if (tmp.Last() == '%') {/* XXX not 100% compatible with ebina's code */
      if (val < 0) val = 0;
      if (val > 100) val = 100;
      aResult.SetPercentValue(float(val)/100.0f);
    } else {
      if (eHTMLUnit_Pixel == aValueUnit) {
        aResult.SetPixelValue(val);
      }
      else {
        aResult.SetIntValue(val, aValueUnit);
      }
    }
    return PR_TRUE;
  }

  // Illegal values are mapped to empty
  aResult.SetEmptyValue();
  return PR_FALSE;
}

/* used to parse attribute values that could be either:
 *   integer  (n), 
 *   percent  (n%),
 *   or proportional (n*)
 */
void
nsHTMLGenericContent::ParseValueOrPercentOrProportional(const nsString& aString,
                                                        nsHTMLValue& aResult, 
                                                        nsHTMLUnit aValueUnit)
{ // XXX should have min/max values?
  nsAutoString tmp(aString);
  tmp.CompressWhitespace(PR_TRUE, PR_TRUE);
  PRInt32 ec, val = tmp.ToInteger(&ec);
  if (tmp.Last() == '%') {/* XXX not 100% compatible with ebina's code */
    if (val < 0) val = 0;
    if (val > 100) val = 100;
    aResult.SetPercentValue(float(val)/100.0f);
	} else if (tmp.Last() == '*') {
    if (val < 0) val = 0;
    aResult.SetIntValue(val, eHTMLUnit_Proportional);	// proportional values are integers
  } else {
    if (eHTMLUnit_Pixel == aValueUnit) {
      aResult.SetPixelValue(val);
    }
    else {
      aResult.SetIntValue(val, aValueUnit);
    }
  }
}

PRBool
nsHTMLGenericContent::ValueOrPercentToString(const nsHTMLValue& aValue,
                                             nsString& aResult)
{
  aResult.Truncate(0);
  switch (aValue.GetUnit()) {
  case eHTMLUnit_Integer:
    aResult.Append(aValue.GetIntValue(), 10);
    return PR_TRUE;
  case eHTMLUnit_Pixel:
    aResult.Append(aValue.GetPixelValue(), 10);
    return PR_TRUE;
  case eHTMLUnit_Percent:
    aResult.Append(PRInt32(aValue.GetPercentValue() * 100.0f), 10);
    aResult.Append('%');
    return PR_TRUE;
  }
  return PR_FALSE;
}

PRBool
nsHTMLGenericContent::ParseValue(const nsString& aString, PRInt32 aMin,
                                 nsHTMLValue& aResult, nsHTMLUnit aValueUnit)
{
  PRInt32 ec, val = aString.ToInteger(&ec);
  if (NS_OK == ec) {
    if (val < aMin) val = aMin;
    if (eHTMLUnit_Pixel == aValueUnit) {
      aResult.SetPixelValue(val);
    }
    else {
      aResult.SetIntValue(val, aValueUnit);
    }
    return PR_TRUE;
  }

  // Illegal values are mapped to empty
  aResult.SetEmptyValue();
  return PR_FALSE;
}

PRBool
nsHTMLGenericContent::ParseValue(const nsString& aString, PRInt32 aMin,
                                 PRInt32 aMax,
                                 nsHTMLValue& aResult, nsHTMLUnit aValueUnit)
{
  PRInt32 ec, val = aString.ToInteger(&ec);
  if (NS_OK == ec) {
    if (val < aMin) val = aMin;
    if (val > aMax) val = aMax;
    if (eHTMLUnit_Pixel == aValueUnit) {
      aResult.SetPixelValue(val);
    }
    else {
      aResult.SetIntValue(val, aValueUnit);
    }
    return PR_TRUE;
  }

  // Illegal values are mapped to empty
  aResult.SetEmptyValue();
  return PR_FALSE;
}

//----------------------------------------------------------------------

nsHTMLGenericLeafContent::nsHTMLGenericLeafContent()
{
}

nsHTMLGenericLeafContent::~nsHTMLGenericLeafContent()
{
}

nsresult
nsHTMLGenericLeafContent::CopyInnerTo(nsIHTMLContent* aSrcContent,
                                      nsHTMLGenericLeafContent* aDst)
{
  aDst->mContent = aSrcContent;
  // XXX should the node's document be set?
  // XXX copy attributes not yet impelemented
  return NS_OK;
}

nsresult
nsHTMLGenericLeafContent::Equals(nsIDOMNode* aNode, PRBool aDeep,
                                 PRBool* aReturn)
{
  // XXX not yet implemented
  *aReturn = PR_FALSE;
  return NS_OK;
}

void
nsHTMLGenericLeafContent::BeginConvertToXIF(nsXIFConverter& aConverter) const
{
  if (nsnull != mTag)
  {
    nsAutoString name;
    mTag->ToString(name);
    aConverter.BeginLeaf(name);    
  }

  // Add all attributes to the convert
  if (nsnull != mAttributes) 
  {
    nsISupportsArray* attrs;
    nsresult rv = NS_NewISupportsArray(&attrs);
    if (NS_OK == rv) 
    {
      mAttributes->GetAllAttributeNames(attrs);
      PRInt32 i, n = attrs->Count();
      nsAutoString name, value;
      for (i = 0; i < n; i++) 
      {
        nsIAtom* atom = (nsIAtom*) attrs->ElementAt(i);
        atom->ToString(name);

        value.Truncate();
        GetAttribute(name, value);
        
        aConverter.AddHTMLAttribute(name,value);
      }
      NS_RELEASE(attrs);
    }
  }
}

void
nsHTMLGenericLeafContent::ConvertContentToXIF(nsXIFConverter& aConverter) const
{
}

void
nsHTMLGenericLeafContent::FinishConvertToXIF(nsXIFConverter& aConverter) const
{
  if (nsnull != mTag)
  {
    nsAutoString name;
    mTag->ToString(name);
    aConverter.EndLeaf(name);    
  }
}

// XXX not really implemented (yet)
nsresult
nsHTMLGenericLeafContent::SizeOf(nsISizeOfHandler* aHandler) const
{
  aHandler->Add(sizeof(*this));
  return NS_OK;
}
