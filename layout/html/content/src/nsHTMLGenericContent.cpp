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
#include "nsIHTMLStyleSheet.h"
#include "nsIHTMLDocument.h"
#include "nsIHTMLContent.h"
#include "nsILinkHandler.h"
#include "nsIScriptContextOwner.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptObjectOwner.h"
#include "nsISizeOfHandler.h"
#include "nsIStyleContext.h"
#include "nsIStyleRule.h"
#include "nsISupportsArray.h"
#include "nsIURL.h"
#include "nsStyleConsts.h"
#include "nsXIFConverter.h"
#include "nsFrame.h"

#include "nsString.h"
#include "nsHTMLAtoms.h"
#include "nsDOMEventsIIDs.h"
#include "nsCSSBlockFrame.h"
#include "nsCSSInlineFrame.h"
#include "nsIEventStateManager.h"
#include "nsDOMEvent.h"
#include "nsIPrivateDOMEvent.h"
#include "prprf.h"

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
static NS_DEFINE_IID(kIHTMLDocumentIID, NS_IHTMLDOCUMENT_IID);

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

  mContent.SetAttribute(name, value, PR_TRUE);
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
    PRInt32 count;
    mContent.GetAllAttributeNames(attributes, count);
    if (count > 0) {
      if ((PRInt32)aIndex < count) {
        nsISupports *att = attributes->ElementAt(aIndex);
        static NS_DEFINE_IID(kIAtom, NS_IATOM_IID);
        nsIAtom *atName = nsnull;
        if (nsnull != att && NS_OK == att->QueryInterface(kIAtom, (void**)&atName)) {
          atName->ToString(name);
          if (NS_CONTENT_ATTR_NOT_THERE != mContent.GetAttribute(name, value)) {
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
  PRInt32 n;
  nsresult rv = mContent.GetAttributeCount(n);
  *aLength = PRUint32(n);
  return rv;
}

//----------------------------------------------------------------------

static nsresult EnsureWritableAttributes(nsIHTMLAttributes*& aAttributes, PRBool aCreate)
{
  nsresult  result = NS_OK;

  if (nsnull == aAttributes) {
    if (PR_TRUE == aCreate) {
      result = NS_NewHTMLAttributes(&aAttributes);
      if (NS_OK == result) {
        aAttributes->AddContentRef();
      }
    }
  }
  else {
    PRInt32 contentRefCount;
    aAttributes->GetContentRefCount(contentRefCount);
    if (1 < contentRefCount) {
      nsIHTMLAttributes*  attrs;
      result = aAttributes->Clone(&attrs);
      if (NS_OK == result) {
        aAttributes->ReleaseContentRef();
        NS_RELEASE(aAttributes);
        aAttributes = attrs;
        aAttributes->AddContentRef();
      }
    }
  }
  return result;
}

static void ReleaseAttributes(nsIHTMLAttributes*& aAttributes)
{
  aAttributes->ReleaseContentRef();
  NS_RELEASE(aAttributes);
}


nsHTMLGenericContent::nsHTMLGenericContent()
{
  mDocument = nsnull;
  mParent = nsnull;
  mAttributes = nsnull;
  mTag = nsnull;
  mContent = nsnull;
  mScriptObject = nsnull;
  mListenerManager = nsnull;
}

nsHTMLGenericContent::~nsHTMLGenericContent()
{
  if (nsnull != mAttributes) {
    ReleaseAttributes(mAttributes);
  }
  NS_IF_RELEASE(mTag);
  NS_IF_RELEASE(mListenerManager);
  // XXX what about mScriptObject? it's now safe to GC it...
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
    PRInt32 pos;
    mParent->IndexOf(mContent, pos);
    if (pos > -1) {
      nsIContent* prev;
      mParent->ChildAt(--pos, prev);
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
    PRInt32 pos;
    mParent->IndexOf(mContent, pos);
    if (pos > -1 ) {
      nsIContent* prev;
      mParent->ChildAt(++pos, prev);
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
  SetAttribute(aName, aValue, PR_TRUE);
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
  if (NS_CONTENT_ATTR_NOT_THERE != GetAttribute(aName, value)) {
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
        SetAttribute(name, value, PR_TRUE);
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
  SetAttr(nsHTMLAtoms::id, aId, eSetAttrNotify_Restart);
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
  SetAttr(nsHTMLAtoms::title, aTitle, eSetAttrNotify_None);
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
  SetAttr(nsHTMLAtoms::lang, aLang, eSetAttrNotify_Reflow);
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
  SetAttr(nsHTMLAtoms::dir, aDir, eSetAttrNotify_Reflow);
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
  SetAttr(nsHTMLAtoms::kClass, aClassName, eSetAttrNotify_Restart);
  return NS_OK;
}

nsresult
nsHTMLGenericContent::GetDocument(nsIDocument*& aResult) const
{
  aResult = mDocument;
  NS_IF_ADDREF(mDocument);
  return NS_OK;
}

static nsIHTMLStyleSheet* GetAttrStyleSheet(nsIDocument* aDocument)
{
  nsIHTMLStyleSheet*  sheet = nsnull;
  nsIHTMLDocument*  htmlDoc;

  if (nsnull != aDocument) {
    if (NS_OK == aDocument->QueryInterface(kIHTMLDocumentIID, (void**)&htmlDoc)) {
      htmlDoc->GetAttributeStyleSheet(&sheet);
    }
  }
  NS_ASSERTION(nsnull != sheet, "can't get attribute style sheet");
  return sheet;
}

nsresult
nsHTMLGenericContent::SetDocument(nsIDocument* aDocument)
{
  mDocument = aDocument;

  // Once the element is added to the doc tree we need to check if
  // event handler were registered on it.  Unfortunately, this means
  // doing a GetAttribute for every type of handler.
  if (nsnull != mAttributes) {
    nsHTMLValue val;
    if (NS_CONTENT_ATTR_HAS_VALUE == mAttributes->GetAttribute(nsHTMLAtoms::onclick, val))
      AddScriptEventListener(nsHTMLAtoms::onclick, val, kIDOMMouseListenerIID);
    if (NS_CONTENT_ATTR_HAS_VALUE == mAttributes->GetAttribute(nsHTMLAtoms::ondblclick, val))
      AddScriptEventListener(nsHTMLAtoms::onclick, val, kIDOMMouseListenerIID);
    if (NS_CONTENT_ATTR_HAS_VALUE == mAttributes->GetAttribute(nsHTMLAtoms::onmousedown, val))
      AddScriptEventListener(nsHTMLAtoms::onmousedown, val, kIDOMMouseListenerIID);
    if (NS_CONTENT_ATTR_HAS_VALUE == mAttributes->GetAttribute(nsHTMLAtoms::onmouseup, val))
      AddScriptEventListener(nsHTMLAtoms::onmouseup, val, kIDOMMouseListenerIID);
    if (NS_CONTENT_ATTR_HAS_VALUE == mAttributes->GetAttribute(nsHTMLAtoms::onmouseover, val))
      AddScriptEventListener(nsHTMLAtoms::onmouseover, val, kIDOMMouseListenerIID);
    if (NS_CONTENT_ATTR_HAS_VALUE == mAttributes->GetAttribute(nsHTMLAtoms::onmouseout, val))
      AddScriptEventListener(nsHTMLAtoms::onmouseout, val, kIDOMMouseListenerIID);
    if (NS_CONTENT_ATTR_HAS_VALUE == mAttributes->GetAttribute(nsHTMLAtoms::onkeydown, val))
      AddScriptEventListener(nsHTMLAtoms::onkeydown, val, kIDOMKeyListenerIID);
    if (NS_CONTENT_ATTR_HAS_VALUE == mAttributes->GetAttribute(nsHTMLAtoms::onkeyup, val))
      AddScriptEventListener(nsHTMLAtoms::onkeyup, val, kIDOMKeyListenerIID);
    if (NS_CONTENT_ATTR_HAS_VALUE == mAttributes->GetAttribute(nsHTMLAtoms::onkeypress, val))
      AddScriptEventListener(nsHTMLAtoms::onkeypress, val, kIDOMKeyListenerIID);
    if (NS_CONTENT_ATTR_HAS_VALUE == mAttributes->GetAttribute(nsHTMLAtoms::onmousemove, val))
      AddScriptEventListener(nsHTMLAtoms::onmousemove, val, kIDOMMouseMotionListenerIID);
    if (NS_CONTENT_ATTR_HAS_VALUE == mAttributes->GetAttribute(nsHTMLAtoms::onload, val))
      AddScriptEventListener(nsHTMLAtoms::onload, val, kIDOMLoadListenerIID);
    if (NS_CONTENT_ATTR_HAS_VALUE == mAttributes->GetAttribute(nsHTMLAtoms::onunload, val))
      AddScriptEventListener(nsHTMLAtoms::onunload, val, kIDOMLoadListenerIID);
    if (NS_CONTENT_ATTR_HAS_VALUE == mAttributes->GetAttribute(nsHTMLAtoms::onabort, val))
      AddScriptEventListener(nsHTMLAtoms::onabort, val, kIDOMLoadListenerIID);
    if (NS_CONTENT_ATTR_HAS_VALUE == mAttributes->GetAttribute(nsHTMLAtoms::onerror, val))
      AddScriptEventListener(nsHTMLAtoms::onerror, val, kIDOMLoadListenerIID);
    if (NS_CONTENT_ATTR_HAS_VALUE == mAttributes->GetAttribute(nsHTMLAtoms::onfocus, val))
      AddScriptEventListener(nsHTMLAtoms::onfocus, val, kIDOMFocusListenerIID);
    if (NS_CONTENT_ATTR_HAS_VALUE == mAttributes->GetAttribute(nsHTMLAtoms::onblur, val))
      AddScriptEventListener(nsHTMLAtoms::onblur, val, kIDOMFocusListenerIID);

    nsIHTMLStyleSheet*  sheet = GetAttrStyleSheet(mDocument);
    sheet->SetAttributesFor(mTag, mAttributes); // sync attributes with sheet
  }

  return NS_OK;
}

nsresult
nsHTMLGenericContent::GetParent(nsIContent*& aResult) const
{
  NS_IF_ADDREF(mParent);
  aResult = mParent;
  return NS_OK;;
}

nsresult
nsHTMLGenericContent::SetParent(nsIContent* aParent)
{
  mParent = aParent;
  return NS_OK;
}

nsresult
nsHTMLGenericContent::IsSynthetic(PRBool& aResult)
{
  return PR_FALSE;
}


nsresult
nsHTMLGenericContent::GetTag(nsIAtom*& aResult) const
{
  NS_IF_ADDREF(mTag);
  aResult = mTag;
  return NS_OK;
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
  return ret;
}

nsresult
nsHTMLGenericContent::SetAttribute(const nsString& aName,
                                   const nsString& aValue,
                                   PRBool aNotify)
{
  nsAutoString upper;
  aName.ToUpperCase(upper);
  nsIAtom* attr = NS_NewAtom(upper);
  nsresult rv = SetAttribute(attr, aValue, aNotify);
  NS_RELEASE(attr);
  return rv;
}

nsresult
nsHTMLGenericContent::SetAttribute(nsIAtom* aAttribute,
                                   const nsString& aValue,
                                   PRBool aNotify)
{
  nsresult  result = NS_OK;
  if (nsHTMLAtoms::style == aAttribute) {
    // XXX the style sheet language is a document property that
    // should be used to lookup the style sheet parser to parse the
    // attribute.
    nsICSSParser* css;
    result = NS_NewCSSParser(&css);
    if (NS_OK != result) {
      return result;
    }
    nsIStyleRule* rule;
    result = css->ParseDeclarations(aValue, nsnull, rule);
    if ((NS_OK == result) && (nsnull != rule)) {
      result = SetAttribute(aAttribute, nsHTMLValue(rule), aNotify);
      NS_RELEASE(rule);
    }
    NS_RELEASE(css);
  }
  else {
    if (nsnull != mDocument) {  // set attr via style sheet
      nsIHTMLStyleSheet*  sheet = GetAttrStyleSheet(mDocument);
      result = sheet->SetAttributeFor(aAttribute, aValue, mTag, mAttributes);
    }
    else {  // manage this ourselves and re-sync when we connect to doc
      result = EnsureWritableAttributes(mAttributes, PR_TRUE);
      if (nsnull != mAttributes) {
        PRInt32   count;
        result = mAttributes->SetAttribute(aAttribute, aValue, count);
        if (0 == count) {
          ReleaseAttributes(mAttributes);
        }
      }
    }
  }
  return result;
}

nsresult
nsHTMLGenericContent::SetAttribute(nsIAtom* aAttribute,
                                   const nsHTMLValue& aValue,
                                   PRBool aNotify)
{
  nsresult  result = NS_OK;
  if (nsnull != mDocument) {  // set attr via style sheet
    nsIHTMLStyleSheet*  sheet = GetAttrStyleSheet(mDocument);
    result = sheet->SetAttributeFor(aAttribute, aValue, mTag, mAttributes);
  }
  else {  // manage this ourselves and re-sync when we connect to doc
    result = EnsureWritableAttributes(mAttributes, PR_TRUE);
    if (nsnull != mAttributes) {
      PRInt32   count;
      result = mAttributes->SetAttribute(aAttribute, aValue, count);
      if (0 == count) {
        ReleaseAttributes(mAttributes);
      }
    }
  }
  return result;
}

nsresult
nsHTMLGenericContent::SetAttr(nsIAtom* aAttribute,
                              const nsString& aValue,
                              nsSetAttrNotify aNotify)
{
  // XXX cheesy code for now
  return SetAttribute(aAttribute, aValue, PR_TRUE);
}

nsresult
nsHTMLGenericContent::SetAttr(nsIAtom* aAttribute,
                              const nsHTMLValue& aValue,
                              nsSetAttrNotify aNotify)
{
  // XXX cheesy code for now
  return SetAttribute(aAttribute, aValue, PR_TRUE);
}

nsresult
nsHTMLGenericContent::UnsetAttribute(nsIAtom* aAttribute)
{
  nsresult result = NS_OK;
  if (nsnull != mDocument) {  // set attr via style sheet
    nsIHTMLStyleSheet*  sheet = GetAttrStyleSheet(mDocument);
    result = sheet->UnsetAttributeFor(aAttribute, mTag, mAttributes);
  }
  else {  // manage this ourselves and re-sync when we connect to doc
    result = EnsureWritableAttributes(mAttributes, PR_FALSE);
    if (nsnull != mAttributes) {
      PRInt32 count;
      result = mAttributes->UnsetAttribute(aAttribute, count);
      if (0 == count) {
        ReleaseAttributes(mAttributes);
      }
    }
  }
  return result;
}

nsresult
nsHTMLGenericContent::GetAttribute(const nsString& aName,
                                   nsString& aResult) const
{
  nsAutoString upper;
  aName.ToUpperCase(upper);
  nsIAtom* attr = NS_NewAtom(upper);
  nsresult result = GetAttribute(attr, aResult);
  NS_RELEASE(attr);
  return result;
}

nsresult
nsHTMLGenericContent::GetAttribute(nsIAtom *aAttribute,
                                   nsString &aResult) const
{
  nsHTMLValue value;
  nsresult result = GetAttribute(aAttribute, value);

  char cbuf[20];
  nscolor color;
  if (NS_CONTENT_ATTR_HAS_VALUE == result) {
    // Try subclass conversion routine first
    if (NS_CONTENT_ATTR_HAS_VALUE ==
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
      result = NS_CONTENT_ATTR_NOT_THERE;
      break;
    }
  }
  return result;
}

nsresult
nsHTMLGenericContent::GetAttribute(nsIAtom* aAttribute,
                                   nsHTMLValue& aValue) const
{
  if (nsnull != mAttributes) {
    return mAttributes->GetAttribute(aAttribute, aValue);
  }
  aValue.Reset();
  return NS_CONTENT_ATTR_NOT_THERE;
}

nsresult
nsHTMLGenericContent::GetAllAttributeNames(nsISupportsArray* aArray,
                                           PRInt32& aCount) const
{
  if (nsnull != mAttributes) {
    return mAttributes->GetAllAttributeNames(aArray, aCount);
  }
  aCount = 0;
  return NS_OK;
}

nsresult
nsHTMLGenericContent::GetAttributeCount(PRInt32& aCount) const
{
  if (nsnull != mAttributes) {
    return mAttributes->Count(aCount);
  }
  aCount = 0;
  return NS_OK;
}

nsresult
nsHTMLGenericContent::SetID(nsIAtom* aID)
{
  nsresult result = NS_OK;
  if (nsnull != mDocument) {  // set attr via style sheet
    nsIHTMLStyleSheet*  sheet = GetAttrStyleSheet(mDocument);
    result = sheet->SetIDFor(aID, mTag, mAttributes);
  }
  else {  // manage this ourselves and re-sync when we connect to doc
    EnsureWritableAttributes(mAttributes, PRBool(nsnull != aID));
    if (nsnull != mAttributes) {
      PRInt32 count;
      result = mAttributes->SetID(aID, count);
      if (0 == count) {
        ReleaseAttributes(mAttributes);
      }
    }
  }
  return result;
}

nsresult
nsHTMLGenericContent::GetID(nsIAtom*& aResult) const
{
  if (nsnull != mAttributes) {
    return mAttributes->GetID(aResult);
  }
  aResult = nsnull;
  return NS_OK;
}

nsresult
nsHTMLGenericContent::SetClass(nsIAtom* aClass)
{
  nsresult result = NS_OK;
  if (nsnull != mDocument) {  // set attr via style sheet
    nsIHTMLStyleSheet*  sheet = GetAttrStyleSheet(mDocument);
    result = sheet->SetClassFor(aClass, mTag, mAttributes);
  }
  else {  // manage this ourselves and re-sync when we connect to doc
    EnsureWritableAttributes(mAttributes, PRBool(nsnull != aClass));
    if (nsnull != mAttributes) {
      PRInt32 count;
      result = mAttributes->SetClass(aClass, count);
      if (0 == count) {
        ReleaseAttributes(mAttributes);
      }
    }
  }
  return result;
}

nsresult
nsHTMLGenericContent::GetClass(nsIAtom*& aResult) const
{
  if (nsnull != mAttributes) {
    return mAttributes->GetClass(aResult);
  }
  aResult = nsnull;
  return NS_OK;
}

nsresult
nsHTMLGenericContent::GetStyleRule(nsIStyleRule*& aResult)
{
  nsIStyleRule* result = nsnull;

  if (nsnull != mAttributes) {
    mAttributes->QueryInterface(kIStyleRuleIID, (void**)&result);
  }
  aResult = result;
  return NS_OK;
}

void
nsHTMLGenericContent::ListAttributes(FILE* out) const
{
  nsISupportsArray* attrs;
  if (NS_OK == NS_NewISupportsArray(&attrs)) {
    PRInt32 index, count;
    GetAllAttributeNames(attrs, count);
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

nsresult
nsHTMLGenericContent::List(FILE* out, PRInt32 aIndent) const
{
  NS_PRECONDITION(nsnull != mDocument, "bad content");

  PRInt32 index;
  for (index = aIndent; --index >= 0; ) fputs("  ", out);

  nsIAtom* tag;
  GetTag(tag);
  if (tag != nsnull) {
    nsAutoString buf;
    tag->ToString(buf);
    fputs(buf, out);
    NS_RELEASE(tag);
  }

  ListAttributes(out);

  nsrefcnt r = mContent->AddRef() - 1;
  mContent->Release();
  fprintf(out, " refcount=%d<", r);

  PRBool canHaveKids;
  mContent->CanContainChildren(canHaveKids);
  if (canHaveKids) {
    fputs("\n", out);
    PRInt32 kids;
    mContent->ChildCount(kids);
    for (index = 0; index < kids; index++) {
      nsIContent* kid;
      mContent->ChildAt(index, kid);
      kid->List(out, aIndent + 1);
      NS_RELEASE(kid);
    }
    for (index = aIndent; --index >= 0; ) fputs("  ", out);
  }
  fputs(">\n", out);

  return NS_OK;
}

nsresult
nsHTMLGenericContent::ToHTML(FILE* out) const
{
  nsAutoString tmp;
  nsresult rv = ToHTMLString(tmp);
  fputs(tmp, out);
  return rv;
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

nsresult
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
      PRInt32 i, n;
      mAttributes->GetAllAttributeNames(attrs, n);
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

nsresult
nsHTMLGenericContent::AttributeToString(nsIAtom* aAttribute,
                                        nsHTMLValue& aValue,
                                        nsString& aResult) const
{
  if (nsHTMLAtoms::style == aAttribute) {
    if (eHTMLUnit_ISupports == aValue.GetUnit()) {
      nsIStyleRule* rule = (nsIStyleRule*) aValue.GetISupportsValue();
      // rule->ToString(str);
      aResult = "XXX style rule ToString goes here";
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  aResult.Truncate();
  return NS_CONTENT_ATTR_NOT_THERE;
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

PRBool
nsHTMLGenericContent::ParseColor(const nsString& aString,
                                 nsHTMLValue& aResult)
{
  if (aString.Length() > 0) {
    nsAutoString  colorStr (aString);
    colorStr.CompressWhitespace();
    char cbuf[40];
    colorStr.ToCString(cbuf, sizeof(cbuf));
    nscolor color;
    if (NS_ColorNameToRGB(cbuf, &color)) {
      aResult.SetStringValue(colorStr);
      return PR_TRUE;
    }
    if (NS_HexToRGB(cbuf, &color)) {
      aResult.SetColorValue(color);
      return PR_TRUE;
    }
  }

  // Illegal values are mapped to empty
  aResult.SetEmptyValue();
  return PR_FALSE;
}

PRBool
nsHTMLGenericContent::ColorToString(const nsHTMLValue& aValue,
                                    nsString& aResult)
{
  if (aValue.GetUnit() == eHTMLUnit_Color) {
    nscolor v = aValue.GetColorValue();
    char buf[10];
    PR_snprintf(buf, sizeof(buf), "#%02x%02x%02x",
                NS_GET_R(v), NS_GET_G(v), NS_GET_B(v));
    aResult.Truncate(0);
    aResult.Append(buf);
    return PR_TRUE;
  }
  if (aValue.GetUnit() == eHTMLUnit_String) {
    aValue.GetStringValue(aResult);
    return PR_TRUE;
  }
  if (aValue.GetUnit() == eHTMLUnit_Empty) {  // was illegal
    aResult.Truncate();
    return PR_TRUE;
  }
  return PR_FALSE;
}

// XXX check all mappings against ebina's usage
static nsHTMLGenericContent::EnumTable kAlignTable[] = {
  { "left", NS_STYLE_TEXT_ALIGN_LEFT },
  { "right", NS_STYLE_TEXT_ALIGN_RIGHT },
  { "texttop", NS_STYLE_VERTICAL_ALIGN_TEXT_TOP },
  { "baseline", NS_STYLE_VERTICAL_ALIGN_BASELINE },
  { "center", NS_STYLE_TEXT_ALIGN_CENTER },
  { "bottom", NS_STYLE_VERTICAL_ALIGN_BOTTOM },
  { "top", NS_STYLE_VERTICAL_ALIGN_TOP },
  { "middle", NS_STYLE_VERTICAL_ALIGN_MIDDLE },
  { "absbottom", NS_STYLE_VERTICAL_ALIGN_BOTTOM },
  { "abscenter", NS_STYLE_VERTICAL_ALIGN_MIDDLE },
  { "absmiddle", NS_STYLE_VERTICAL_ALIGN_MIDDLE },
  { 0 }
};

PRBool
nsHTMLGenericContent::ParseAlignValue(const nsString& aString,
                                      nsHTMLValue& aResult)
{
  return ParseEnumValue(aString, kAlignTable, aResult);
}

PRBool
nsHTMLGenericContent::AlignValueToString(const nsHTMLValue& aValue,
                                         nsString& aResult)
{
  return EnumValueToString(aValue, kAlignTable, aResult);
}

PRBool
nsHTMLGenericContent::ParseImageAttribute(nsIAtom* aAttribute,
                                          const nsString& aString,
                                          nsHTMLValue& aResult)
{
  if ((aAttribute == nsHTMLAtoms::width) ||
      (aAttribute == nsHTMLAtoms::height)) {
    ParseValueOrPercent(aString, aResult, eHTMLUnit_Pixel);
    return PR_TRUE;
  }
  else if ((aAttribute == nsHTMLAtoms::hspace) ||
           (aAttribute == nsHTMLAtoms::vspace) ||
           (aAttribute == nsHTMLAtoms::border)) {
    ParseValue(aString, 0, aResult, eHTMLUnit_Pixel);
    return PR_TRUE;
  }
  return PR_FALSE;
}

PRBool
nsHTMLGenericContent::ImageAttributeToString(nsIAtom* aAttribute,
                                             const nsHTMLValue& aValue,
                                             nsString& aResult)
{
  if ((aAttribute == nsHTMLAtoms::width) ||
      (aAttribute == nsHTMLAtoms::height) ||
      (aAttribute == nsHTMLAtoms::border) ||
      (aAttribute == nsHTMLAtoms::hspace) ||
      (aAttribute == nsHTMLAtoms::vspace)) {
    return ValueOrPercentToString(aValue, aResult);
  }
  return PR_FALSE;
}

void
nsHTMLGenericContent::MapImageAttributesInto(nsIStyleContext* aContext, 
                                             nsIPresContext* aPresContext)
{
  if (nsnull != mAttributes) {
    nsHTMLValue value;

    float p2t = aPresContext->GetPixelsToTwips();
    nsStylePosition* pos = (nsStylePosition*)
      aContext->GetMutableStyleData(eStyleStruct_Position);
    nsStyleSpacing* spacing = (nsStyleSpacing*)
      aContext->GetMutableStyleData(eStyleStruct_Spacing);

    // width: value
    GetAttribute(nsHTMLAtoms::width, value);
    if (value.GetUnit() == eHTMLUnit_Pixel) {
      nscoord twips = NSIntPixelsToTwips(value.GetPixelValue(), p2t);
      pos->mWidth.SetCoordValue(twips);
    }
    else if (value.GetUnit() == eHTMLUnit_Percent) {
      pos->mWidth.SetPercentValue(value.GetPercentValue());
    }

    // height: value
    GetAttribute(nsHTMLAtoms::height, value);
    if (value.GetUnit() == eHTMLUnit_Pixel) {
      nscoord twips = NSIntPixelsToTwips(value.GetPixelValue(), p2t);
      pos->mHeight.SetCoordValue(twips);
    }
    else if (value.GetUnit() == eHTMLUnit_Percent) {
      pos->mHeight.SetPercentValue(value.GetPercentValue());
    }

    // hspace: value
    GetAttribute(nsHTMLAtoms::hspace, value);
    if (value.GetUnit() == eHTMLUnit_Pixel) {
      nscoord twips = NSIntPixelsToTwips(value.GetPixelValue(), p2t);
      spacing->mMargin.SetRight(nsStyleCoord(twips));
    }
    else if (value.GetUnit() == eHTMLUnit_Percent) {
      spacing->mMargin.SetRight(nsStyleCoord(value.GetPercentValue(),
                                             eStyleUnit_Coord));
    }

    // vspace: value
    GetAttribute(nsHTMLAtoms::vspace, value);
    if (value.GetUnit() == eHTMLUnit_Pixel) {
      nscoord twips = NSIntPixelsToTwips(value.GetPixelValue(), p2t);
      spacing->mMargin.SetBottom(nsStyleCoord(twips));
    }
    else if (value.GetUnit() == eHTMLUnit_Percent) {
      spacing->mMargin.SetBottom(nsStyleCoord(value.GetPercentValue(),
                                              eStyleUnit_Coord));
    }
  }
}

void
nsHTMLGenericContent::MapImageAlignAttributeInto(nsIStyleContext* aContext,
                                                 nsIPresContext* aPresContext)
{
  if (nsnull != mAttributes) {
    nsHTMLValue value;
    GetAttribute(nsHTMLAtoms::align, value);
    if (value.GetUnit() == eHTMLUnit_Enumerated) {
      PRUint8 align = value.GetIntValue();
      nsStyleDisplay* display = (nsStyleDisplay*)
        aContext->GetMutableStyleData(eStyleStruct_Display);
      nsStyleText* text = (nsStyleText*)
        aContext->GetMutableStyleData(eStyleStruct_Text);
      nsStyleSpacing* spacing = (nsStyleSpacing*)
        aContext->GetMutableStyleData(eStyleStruct_Spacing);
      float p2t = aPresContext->GetPixelsToTwips();
      nsStyleCoord three(NSIntPixelsToTwips(3, p2t));
      switch (align) {
      case NS_STYLE_TEXT_ALIGN_LEFT:
        display->mFloats = NS_STYLE_FLOAT_LEFT;
        spacing->mMargin.SetLeft(three);
        spacing->mMargin.SetRight(three);
        break;
      case NS_STYLE_TEXT_ALIGN_RIGHT:
        display->mFloats = NS_STYLE_FLOAT_RIGHT;
        spacing->mMargin.SetLeft(three);
        spacing->mMargin.SetRight(three);
        break;
      default:
        text->mVerticalAlign.SetIntValue(align, eStyleUnit_Enumerated);
        break;
      }
    }
  }
}

void
nsHTMLGenericContent::MapImageBorderAttributesInto(nsIStyleContext* aContext, 
                                                   nsIPresContext* aPresContext,
                                                   nscolor aBorderColors[4])
{
  if (nsnull != mAttributes) {
    nsHTMLValue value;

    // border: pixels
    GetAttribute(nsHTMLAtoms::border, value);
    if (value.GetUnit() != eHTMLUnit_Pixel) {
      if (nsnull == aBorderColors) {
        return;
      }
      // If no border is defined and we are forcing a border, force
      // the size to 2 pixels.
      value.SetPixelValue(2);
    }

    float p2t = aPresContext->GetPixelsToTwips();
    nscoord twips = NSIntPixelsToTwips(value.GetPixelValue(), p2t);

    // Fixup border-padding sums: subtract out the old size and then
    // add in the new size.
    nsStyleSpacing* spacing = (nsStyleSpacing*)
      aContext->GetMutableStyleData(eStyleStruct_Spacing);
    nsStyleCoord coord;
    coord.SetCoordValue(twips);
    spacing->mBorder.SetTop(coord);
    spacing->mBorder.SetRight(coord);
    spacing->mBorder.SetBottom(coord);
    spacing->mBorder.SetLeft(coord);
    spacing->mBorderStyle[0] = NS_STYLE_BORDER_STYLE_SOLID;
    spacing->mBorderStyle[1] = NS_STYLE_BORDER_STYLE_SOLID;
    spacing->mBorderStyle[2] = NS_STYLE_BORDER_STYLE_SOLID;
    spacing->mBorderStyle[3] = NS_STYLE_BORDER_STYLE_SOLID;

    // Use supplied colors if provided, otherwise use color for border
    // color
    if (nsnull != aBorderColors) {
      spacing->mBorderColor[0] = aBorderColors[0];
      spacing->mBorderColor[1] = aBorderColors[1];
      spacing->mBorderColor[2] = aBorderColors[2];
      spacing->mBorderColor[3] = aBorderColors[3];
    }
    else {
      // Color is inherited from "color"
      const nsStyleColor* styleColor = (const nsStyleColor*)
        aContext->GetStyleData(eStyleStruct_Color);
      nscolor color = styleColor->mColor;
      spacing->mBorderColor[0] = color;
      spacing->mBorderColor[1] = color;
      spacing->mBorderColor[2] = color;
      spacing->mBorderColor[3] = color;
    }
  }
}

void
nsHTMLGenericContent::TriggerLink(nsIPresContext& aPresContext,
                                  const nsString& aBase,
                                  const nsString& aURLSpec,
                                  const nsString& aTargetSpec,
                                  PRBool aClick)
{
  nsILinkHandler* handler;
  if (NS_OK == aPresContext.GetLinkHandler(&handler) && (nsnull != handler)) {
    // Resolve url to an absolute url
    nsIURL* docURL = nsnull;
    nsIDocument* doc;
    if (NS_OK == GetDocument(doc)) {
      docURL = doc->GetDocumentURL();
      NS_RELEASE(doc);
    }

    nsAutoString absURLSpec;
    if (aURLSpec.Length() > 0) {
      nsresult rv = NS_MakeAbsoluteURL(docURL, aBase, aURLSpec, absURLSpec);
    }
    else {
      absURLSpec = aURLSpec;
    }

    if (nsnull != docURL) {
      NS_RELEASE(docURL);
    }

    // Now pass on absolute url to the click handler
    if (aClick) {
      handler->OnLinkClick(nsnull, absURLSpec, aTargetSpec);
    }
    else {
      handler->OnOverLink(nsnull, absURLSpec, aTargetSpec);
    }
    NS_RELEASE(handler);
  }
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

nsresult
nsHTMLGenericLeafContent::BeginConvertToXIF(nsXIFConverter& aConverter) const
{
  nsresult rv = NS_OK;
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
    rv = NS_NewISupportsArray(&attrs);
    if (NS_OK == rv) 
    {
      PRInt32 i, n;
      mAttributes->GetAllAttributeNames(attrs, n);
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
  return rv;
}

nsresult
nsHTMLGenericLeafContent::ConvertContentToXIF(nsXIFConverter& aConverter) const
{
  return NS_OK;
}

nsresult
nsHTMLGenericLeafContent::FinishConvertToXIF(nsXIFConverter& aConverter) const
{
  if (nsnull != mTag)
  {
    nsAutoString name;
    mTag->ToString(name);
    aConverter.EndLeaf(name);    
  }
  return NS_OK;
}

// XXX not really implemented (yet)
nsresult
nsHTMLGenericLeafContent::SizeOf(nsISizeOfHandler* aHandler) const
{
  aHandler->Add(sizeof(*this));
  return NS_OK;
}

//----------------------------------------------------------------------

nsHTMLGenericContainerContent::nsHTMLGenericContainerContent()
{
}

nsHTMLGenericContainerContent::~nsHTMLGenericContainerContent()
{
  PRInt32 n = mChildren.Count();
  for (PRInt32 i = 0; i < n; i++) {
    nsIContent* kid = (nsIContent*) mChildren.ElementAt(i);
    NS_RELEASE(kid);
  }
//  if (nsnull != mChildNodes) {
//    mChildNodes->ReleaseContent();
//    NS_RELEASE(mChildNodes);
//  }
}

nsresult
nsHTMLGenericContainerContent:: CopyInnerTo(nsIHTMLContent* aSrcContent,
                                            nsHTMLGenericContainerContent* aDst)
{
  aDst->mContent = aSrcContent;
  // XXX should the node's document be set?
  // XXX copy attributes not yet impelemented
  // XXX deep copy?
  return NS_OK;
}

nsresult
nsHTMLGenericContainerContent::Equals(nsIDOMNode* aNode, PRBool aDeep,
                                      PRBool* aReturn)
{
  // XXX not yet implemented
  *aReturn = PR_FALSE;
  return NS_OK;
}

nsresult
nsHTMLGenericContainerContent::GetChildNodes(nsIDOMNodeList** aChildNodes)
{
    *aChildNodes = nsnull;
    return NS_OK;
//  NS_PRECONDITION(nsnull != aChildNodes, "null pointer");
//  if (nsnull == mChildNodes) {
//    mChildNodes = new nsDOMNodeList(this);
//    NS_ADDREF(mChildNodes);
//  }
//  *aChildNodes = mChildNodes;
//  NS_ADDREF(mChildNodes);
//  return NS_OK;
}

nsresult
nsHTMLGenericContainerContent::GetHasChildNodes(PRBool* aReturn)
{
  if (0 != mChildren.Count()) {
    *aReturn = PR_TRUE;
  } 
  else {
    *aReturn = PR_FALSE;
  }
  return NS_OK;
}

nsresult
nsHTMLGenericContainerContent::GetFirstChild(nsIDOMNode** aNode)
{
  nsIContent *child = (nsIContent*) mChildren.ElementAt(0);
  if (nsnull != child) {
    nsresult res = child->QueryInterface(kIDOMNodeIID, (void**)aNode);
    NS_ASSERTION(NS_OK == res, "Must be a DOM Node"); // must be a DOM Node
    return res;
  }
  aNode = nsnull;
  return NS_OK;
}

nsresult
nsHTMLGenericContainerContent::GetLastChild(nsIDOMNode** aNode)
{
  nsIContent *child = (nsIContent*) mChildren.ElementAt(mChildren.Count()-1);
  if (nsnull != child) {
    nsresult res = child->QueryInterface(kIDOMNodeIID, (void**)aNode);
    NS_ASSERTION(NS_OK == res, "Must be a DOM Node"); // must be a DOM Node
    return res;
  }
  aNode = nsnull;
  return NS_OK;
}

static void
SetDocumentInChildrenOf(nsIContent* aContent, nsIDocument* aDocument)
{
  PRInt32 i, n;
  aContent->ChildCount(n);
  for (i = 0; i < n; i++) {
    nsIContent* child;
    aContent->ChildAt(i, child);
    if (nsnull != child) {
      child->SetDocument(aDocument);
      SetDocumentInChildrenOf(child, aDocument);
      NS_RELEASE(child);
    }
  }
}

// XXX It's possible that newChild has already been inserted in the
// tree; if this is the case then we need to remove it from where it
// was before placing it in it's new home

nsresult
nsHTMLGenericContainerContent::InsertBefore(nsIDOMNode* aNewChild,
                                            nsIDOMNode* aRefChild,
                                            nsIDOMNode** aReturn)
{
  if (nsnull == aNewChild) {
    *aReturn = nsnull;
    return NS_OK;/* XXX wrong error value */
  }

  // Get the nsIContent interface for the new content
  nsIContent* newContent = nsnull;
  nsresult res = aNewChild->QueryInterface(kIContentIID, (void**)&newContent);
  NS_ASSERTION(NS_OK == res, "New child must be an nsIContent");
  if (NS_OK == res) {
    if (nsnull == aRefChild) {
      // Append the new child to the end
      SetDocumentInChildrenOf(newContent, mDocument);
      res = AppendChildTo(newContent, PR_TRUE);
    }
    else {
      // Get the index of where to insert the new child
      nsIContent* refContent = nsnull;
      res = aRefChild->QueryInterface(kIContentIID, (void**)&refContent);
      NS_ASSERTION(NS_OK == res, "Ref child must be an nsIContent");
      if (NS_OK == res) {
        PRInt32 pos;
        IndexOf(refContent, pos);
        if (pos >= 0) {
          SetDocumentInChildrenOf(newContent, mDocument);
          res = InsertChildAt(newContent, pos, PR_TRUE);
        }
        NS_RELEASE(refContent);
      }
    }
    NS_RELEASE(newContent);

    *aReturn = aNewChild;
    NS_ADDREF(aNewChild);
  }
  else {
    *aReturn = nsnull;
  }

  return res;
}

nsresult
nsHTMLGenericContainerContent::ReplaceChild(nsIDOMNode* aNewChild,
                                            nsIDOMNode* aOldChild,
                                            nsIDOMNode** aReturn)
{
  nsIContent* content = nsnull;
  *aReturn = nsnull;
  nsresult res = aOldChild->QueryInterface(kIContentIID, (void**)&content);
  NS_ASSERTION(NS_OK == res, "Must be an nsIContent");
  if (NS_OK == res) {
    PRInt32 pos;
    IndexOf(content, pos);
    if (pos >= 0) {
      nsIContent* newContent = nsnull;
      nsresult res = aNewChild->QueryInterface(kIContentIID, (void**)&newContent);
      NS_ASSERTION(NS_OK == res, "Must be an nsIContent");
      if (NS_OK == res) {
        res = ReplaceChildAt(newContent, pos, PR_TRUE);
        NS_RELEASE(newContent);
      }
      *aReturn = aOldChild;
      NS_ADDREF(aOldChild);
    }
    NS_RELEASE(content);
  }
  
  return res;
}

nsresult
nsHTMLGenericContainerContent::RemoveChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn)
{
  nsIContent* content = nsnull;
  *aReturn = nsnull;
  nsresult res = aOldChild->QueryInterface(kIContentIID, (void**)&content);
  NS_ASSERTION(NS_OK == res, "Must be an nsIContent");
  if (NS_OK == res) {
    PRInt32 pos;
    IndexOf(content, pos);
    if (pos >= 0) {
      res = RemoveChildAt(pos, PR_TRUE);
      *aReturn = aOldChild;
      NS_ADDREF(aOldChild);
    }
    NS_RELEASE(content);
  }

  return res;
}

nsresult
nsHTMLGenericContainerContent::AppendChild(nsIDOMNode* aNewChild, nsIDOMNode** aReturn)
{
  return InsertBefore(aNewChild, nsnull, aReturn);
}

nsresult
nsHTMLGenericContainerContent::SizeOf(nsISizeOfHandler* aHandler) const
{
  aHandler->Add(sizeof(*this));
  return NS_OK;
}

nsresult
nsHTMLGenericContainerContent::BeginConvertToXIF(nsXIFConverter& aConverter) const
{
  nsresult rv = NS_OK;
  if (nsnull != mTag)
  {
    nsAutoString name;
    mTag->ToString(name);
    aConverter.BeginContainer(name);
  }

  // Add all attributes to the convert
  if (nsnull != mAttributes) 
  {
    nsISupportsArray* attrs;
    rv = NS_NewISupportsArray(&attrs);
    if (NS_OK == rv) 
    {
      PRInt32 i, n;
      mAttributes->GetAllAttributeNames(attrs, n);
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
  return NS_OK;
}

nsresult
nsHTMLGenericContainerContent::ConvertContentToXIF(nsXIFConverter& aConverter) const
{
  return NS_OK;
}

nsresult
nsHTMLGenericContainerContent::FinishConvertToXIF(nsXIFConverter& aConverter) const
{
  if (nsnull != mTag)
  {
    nsAutoString name;
    mTag->ToString(name);
    aConverter.EndContainer(name);
  }
  return NS_OK;
}

nsresult
nsHTMLGenericContainerContent::Compact()
{
  mChildren.Compact();
  return NS_OK;
}

nsresult
nsHTMLGenericContainerContent::CanContainChildren(PRBool& aResult) const
{
  aResult = PR_TRUE;
  return NS_OK;
}

nsresult
nsHTMLGenericContainerContent::ChildCount(PRInt32& aCount) const
{
  aCount = mChildren.Count();
  return NS_OK;
}

nsresult
nsHTMLGenericContainerContent::ChildAt(PRInt32 aIndex,
                                       nsIContent*& aResult) const
{
  nsIContent *child = (nsIContent*) mChildren.ElementAt(aIndex);
  if (nsnull != child) {
    NS_ADDREF(child);
  }
  aResult = child;
  return NS_OK;
}

nsresult
nsHTMLGenericContainerContent::IndexOf(nsIContent* aPossibleChild,
                                       PRInt32& aIndex) const
{
  NS_PRECONDITION(nsnull != aPossibleChild, "null ptr");
  aIndex = mChildren.IndexOf(aPossibleChild);
  return NS_OK;
}

nsresult
nsHTMLGenericContainerContent::InsertChildAt(nsIContent* aKid,
                                             PRInt32 aIndex,
                                             PRBool aNotify)
{
  NS_PRECONDITION(nsnull != aKid, "null ptr");
  PRBool rv = mChildren.InsertElementAt(aKid, aIndex);/* XXX fix up void array api to use nsresult's*/
  if (rv) {
    NS_ADDREF(aKid);
    aKid->SetParent(mContent);
    nsIDocument* doc = mDocument;
    if (nsnull != doc) {
      aKid->SetDocument(doc);
      if (aNotify) {
        doc->ContentInserted(mContent, aKid, aIndex);
      }
    }
  }
  return NS_OK;
}

nsresult
nsHTMLGenericContainerContent::ReplaceChildAt(nsIContent* aKid,
                                              PRInt32 aIndex,
                                              PRBool aNotify)
{
  NS_PRECONDITION(nsnull != aKid, "null ptr");
  nsIContent* oldKid = (nsIContent*) mChildren.ElementAt(aIndex);
  PRBool rv = mChildren.ReplaceElementAt(aKid, aIndex);
  if (rv) {
    NS_ADDREF(aKid);
    aKid->SetParent(mContent);
    nsIDocument* doc = mDocument;
    if (nsnull != doc) {
      aKid->SetDocument(doc);
      if (aNotify) {
        doc->ContentReplaced(mContent, oldKid, aKid, aIndex);
      }
    }
    oldKid->SetDocument(nsnull);
    oldKid->SetParent(nsnull);
    NS_RELEASE(oldKid);
  }
  return NS_OK;
}

nsresult
nsHTMLGenericContainerContent::AppendChildTo(nsIContent* aKid, PRBool aNotify)
{
  NS_PRECONDITION((nsnull != aKid) && (aKid != mContent), "null ptr");
  PRBool rv = mChildren.AppendElement(aKid);
  if (rv) {
    NS_ADDREF(aKid);
    aKid->SetParent(mContent);
    nsIDocument* doc = mDocument;
    if (nsnull != doc) {
      aKid->SetDocument(doc);
      if (aNotify) {
        doc->ContentAppended(mContent, mChildren.Count() - 1);
      }
    }
  }
  return NS_OK;
}

nsresult
nsHTMLGenericContainerContent::RemoveChildAt(PRInt32 aIndex, PRBool aNotify)
{
  nsIContent* oldKid = (nsIContent*) mChildren.ElementAt(aIndex);
  if (nsnull != oldKid ) {
    nsIDocument* doc = mDocument;
    if (aNotify) {
      if (nsnull != doc) {
        doc->ContentWillBeRemoved(mContent, oldKid, aIndex);
      }
    }
    PRBool rv = mChildren.RemoveElementAt(aIndex);
    if (aNotify) {
      if (nsnull != doc) {
        doc->ContentHasBeenRemoved(mContent, oldKid, aIndex);
      }
    }
    oldKid->SetDocument(nsnull);
    oldKid->SetParent(nsnull);
    NS_RELEASE(oldKid);
  }

  return NS_OK;
}
