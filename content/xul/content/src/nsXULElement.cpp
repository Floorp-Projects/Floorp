/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

/*

  Implementation for a "pseudo content element" that acts as a proxy
  to the RDF graph.

  Unfortunately, there is no one right way to transform RDF into a
  document model. Ideally, one would like to use something like XSL to
  define how to do it in a declarative, user-definable way. But since
  we don't have XSL yet, that's not an option. So here we have a
  hard-coded implementation that does the job.

  TO DO

  1) In the absence of XSL, at least factor out the strategy used to
     build the content model so that multiple models can be build
     (e.g., table-like HTML vs. XUI tree control, etc.)

     This involves both hacking the code that generates children for
     presentation, and the code that manipulates children via the DOM,
     which leads us to the next item...

  2) Implement DOM interfaces.

 */


// #define the following if you want properties to show up as
// attributes on an element. I know, this sucks, but I'm just not
// really sure if this is necessary...
//#define CREATE_PROPERTIES_AS_ATTRIBUTES

#include "nsDOMEvent.h"
#include "nsHashtable.h"
#include "nsIAtom.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMScriptObjectFactory.h"
#include "nsIDocument.h"
#include "nsIEventListenerManager.h"
#include "nsIEventStateManager.h"
#include "nsIRDFCursor.h"
#include "nsIRDFDataBase.h"
#include "nsIRDFDocument.h"
#include "nsIRDFNode.h"
#include "nsIRDFService.h"
#include "nsIServiceManager.h"
#include "nsISupportsArray.h"
#include "nsRDFCID.h"
#include "nsRDFElement.h"
#include "nsINameSpaceManager.h"

////////////////////////////////////////////////////////////////////////

static NS_DEFINE_IID(kIContentIID,            NS_ICONTENT_IID);
static NS_DEFINE_IID(kIDOMElementIID,         NS_IDOMELEMENT_IID);
static NS_DEFINE_IID(kIDOMEventReceiverIID,   NS_IDOMEVENTRECEIVER_IID);
static NS_DEFINE_IID(kIDOMNodeIID,            NS_IDOMNODE_IID);
static NS_DEFINE_IID(kIDOMNodeListIID,        NS_IDOMNODELIST_IID);
static NS_DEFINE_IID(kIDocumentIID,           NS_IDOCUMENT_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID,     NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIRDFContentIID,         NS_IRDFCONTENT_IID);
static NS_DEFINE_IID(kIRDFDataBaseIID,        NS_IRDFDATABASE_IID);
static NS_DEFINE_IID(kIRDFDocumentIID,        NS_IRDFDOCUMENT_IID);
static NS_DEFINE_IID(kIRDFServiceIID,         NS_IRDFSERVICE_IID);
static NS_DEFINE_IID(kIScriptObjectOwnerIID,  NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kISupportsIID,           NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIXMLContentIID,         NS_IXMLCONTENT_IID);

static NS_DEFINE_CID(kRDFServiceCID,          NS_RDFSERVICE_CID);



////////////////////////////////////////////////////////////////////////
// RDFDOMNodeListImpl
//
//   Helper class to implement the nsIDOMNodeList interface. It's
//   probably wrong in some sense, as it uses the "naked" content
//   interface to look for kids. (I assume in general this is bad
//   because there may be pseudo-elements created for presentation
//   that aren't visible to the DOM.)
//

class RDFDOMNodeListImpl : public nsIDOMNodeList {
private:
    nsRDFElement* mElement;

public:
    RDFDOMNodeListImpl(nsRDFElement* element) : mElement(element) {
        NS_IF_ADDREF(mElement);
    }

    virtual ~RDFDOMNodeListImpl(void) {
        NS_IF_RELEASE(mElement);
    }

    // nsISupports interface
    NS_DECL_ISUPPORTS

    NS_DECL_IDOMNODELIST
};

NS_IMPL_ISUPPORTS(RDFDOMNodeListImpl, kIDOMNodeListIID);

NS_IMETHODIMP
RDFDOMNodeListImpl::GetLength(PRUint32* aLength)
{
    PRInt32 count;
    nsresult rv;
    if (NS_FAILED(rv = mElement->ChildCount(count)))
        return rv;
    *aLength = count;
    return NS_OK;
}


NS_IMETHODIMP
RDFDOMNodeListImpl::Item(PRUint32 aIndex, nsIDOMNode** aReturn)
{
    // XXX naive. probably breaks when there are pseudo elements or something.
    nsresult rv;
    nsIContent* contentChild;
    if (NS_FAILED(rv = mElement->ChildAt(aIndex, contentChild)))
        return rv;

    rv = contentChild->QueryInterface(kIDOMNodeIID, (void**) aReturn);
    NS_RELEASE(contentChild);

    return rv;
}


////////////////////////////////////////////////////////////////////////
// nsGenericAttribute

struct nsGenericAttribute
{
  nsGenericAttribute(PRInt32 aNameSpaceID, nsIAtom* aName, const nsString& aValue)
    : mNameSpaceID(aNameSpaceID),
      mName(aName),
      mValue(aValue)
  {
    NS_IF_ADDREF(mName);
  }

  ~nsGenericAttribute(void)
  {
    NS_IF_RELEASE(mName);
  }

  PRInt32   mNameSpaceID;
  nsIAtom*  mName;
  nsString  mValue;
};

////////////////////////////////////////////////////////////////////////
// nsRDFElement

nsresult
NS_NewRDFElement(nsIRDFContent** result)
{
    NS_PRECONDITION(result, "null ptr");
    if (! result)
        return NS_ERROR_NULL_POINTER;

    *result = NS_STATIC_CAST(nsIRDFContent*, new nsRDFElement());
    if (! *result)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*result);
    return NS_OK;
}

nsRDFElement::nsRDFElement(void)
    : mDocument(nsnull),
      mNameSpacePrefix(nsnull),
      mNameSpaceID(kNameSpaceID_Unknown),
      mScriptObject(nsnull),
      mResource(nsnull),
      mChildren(nsnull),
      mChildrenMustBeGenerated(PR_TRUE),
      mParent(nsnull),
      mAttributes(nsnull)
{
    NS_INIT_REFCNT();
}

nsRDFElement::~nsRDFElement()
{
  if (nsnull != mAttributes) {
    PRInt32 count = mAttributes->Count();
    PRInt32 index;
    for (index = 0; index < count; index++) {
      nsGenericAttribute* attr = (nsGenericAttribute*)mAttributes->ElementAt(index);
      delete attr;
    }
    delete mAttributes;
  }
    // mDocument is not refcounted
    //NS_IF_RELEASE(mScriptObject); XXX don't forget!
    NS_IF_RELEASE(mChildren);
    NS_IF_RELEASE(mNameSpacePrefix);
    NS_IF_RELEASE(mResource);
}

NS_IMPL_ADDREF(nsRDFElement);
NS_IMPL_RELEASE(nsRDFElement);

NS_IMETHODIMP 
nsRDFElement::QueryInterface(REFNSIID iid, void** result)
{
    if (! result)
        return NS_ERROR_NULL_POINTER;

    if (iid.Equals(kIRDFContentIID) ||
        iid.Equals(kIXMLContentIID) ||
        iid.Equals(kIContentIID) ||
        iid.Equals(kISupportsIID)) {
        *result = NS_STATIC_CAST(nsIRDFContent*, this);
    }
    else if (iid.Equals(kIDOMElementIID) ||
             iid.Equals(kIDOMNodeIID)) {
        *result = NS_STATIC_CAST(nsIDOMElement*, this);
    }
    else if (iid.Equals(kIScriptObjectOwnerIID)) {
        *result = NS_STATIC_CAST(nsIScriptObjectOwner*, this);
    }
    else if (iid.Equals(kIDOMEventReceiverIID)) {
        *result = NS_STATIC_CAST(nsIDOMEventReceiver*, this);
    }
    else if (iid.Equals(kIJSScriptObjectIID)) {
        *result = NS_STATIC_CAST(nsIJSScriptObject*, this);
    }
    else {
        *result = nsnull;
        return NS_NOINTERFACE;
    }

    // if we get here, we know one of the above IIDs was ok.
    AddRef();
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////
// nsIDOMNode interface

NS_IMETHODIMP
nsRDFElement::GetNodeName(nsString& aNodeName)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsRDFElement::GetNodeValue(nsString& aNodeValue)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsRDFElement::SetNodeValue(const nsString& aNodeValue)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsRDFElement::GetNodeType(PRUint16* aNodeType)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsRDFElement::GetParentNode(nsIDOMNode** aParentNode)
{
    if (!mParent)
        return NS_ERROR_NOT_INITIALIZED;

    return mParent->QueryInterface(kIDOMNodeIID, (void**) aParentNode);
}


NS_IMETHODIMP
nsRDFElement::GetChildNodes(nsIDOMNodeList** aChildNodes)
{
    nsresult rv;
    if (mChildrenMustBeGenerated) {
        if (NS_FAILED(rv = GenerateChildren()))
            return rv;
    }

    RDFDOMNodeListImpl* list = new RDFDOMNodeListImpl(this);
    if (! list)
        return NS_ERROR_OUT_OF_MEMORY;

    return list->QueryInterface(kIDOMNodeListIID, (void**) aChildNodes);
}


NS_IMETHODIMP
nsRDFElement::GetFirstChild(nsIDOMNode** aFirstChild)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsRDFElement::GetLastChild(nsIDOMNode** aLastChild)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsRDFElement::GetPreviousSibling(nsIDOMNode** aPreviousSibling)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsRDFElement::GetNextSibling(nsIDOMNode** aNextSibling)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsRDFElement::GetAttributes(nsIDOMNamedNodeMap** aAttributes)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsRDFElement::GetOwnerDocument(nsIDOMDocument** aOwnerDocument)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsRDFElement::InsertBefore(nsIDOMNode* aNewChild, nsIDOMNode* aRefChild, nsIDOMNode** aReturn)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsRDFElement::ReplaceChild(nsIDOMNode* aNewChild, nsIDOMNode* aOldChild, nsIDOMNode** aReturn)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsRDFElement::RemoveChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsRDFElement::AppendChild(nsIDOMNode* aNewChild, nsIDOMNode** aReturn)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsRDFElement::HasChildNodes(PRBool* aReturn)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsRDFElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;

#if 0
    nsRDFElement* it = new nsRDFElement();
    if (! it)
        return NS_ERROR_OUT_OF_MEMORY;

    return it->QueryInterface(kIDOMNodeIID, (void**) aReturn);
#endif
}


////////////////////////////////////////////////////////////////////////
// nsIDOMElement interface

NS_IMETHODIMP
nsRDFElement::GetTagName(nsString& aTagName)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsRDFElement::GetDOMAttribute(const nsString& aName, nsString& aReturn)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsRDFElement::SetDOMAttribute(const nsString& aName, const nsString& aValue)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsRDFElement::RemoveAttribute(const nsString& aName)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsRDFElement::GetAttributeNode(const nsString& aName, nsIDOMAttr** aReturn)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsRDFElement::SetAttributeNode(nsIDOMAttr* aNewAttr, nsIDOMAttr** aReturn)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsRDFElement::RemoveAttributeNode(nsIDOMAttr* aOldAttr, nsIDOMAttr** aReturn)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsRDFElement::GetElementsByTagName(const nsString& aName, nsIDOMNodeList** aReturn)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsRDFElement::Normalize()
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}



////////////////////////////////////////////////////////////////////////
// nsIDOMEventReceiver interface

NS_IMETHODIMP
nsRDFElement::AddEventListener(nsIDOMEventListener *aListener, const nsIID& aIID)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsRDFElement::RemoveEventListener(nsIDOMEventListener *aListener, const nsIID& aIID)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsRDFElement::GetListenerManager(nsIEventListenerManager** aInstancePtrResult)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsRDFElement::GetNewListenerManager(nsIEventListenerManager **aInstancePtrResult)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}



////////////////////////////////////////////////////////////////////////
// nsIScriptObjectOwner interface

NS_IMETHODIMP 
nsRDFElement::GetScriptObject(nsIScriptContext* aContext, void** aScriptObject)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
nsRDFElement::SetScriptObject(void *aScriptObject)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


////////////////////////////////////////////////////////////////////////
// nsIJSScriptObject interface

PRBool
nsRDFElement::AddProperty(JSContext *aContext, jsval aID, jsval *aVp)
{
    return PR_FALSE;
}

PRBool
nsRDFElement::DeleteProperty(JSContext *aContext, jsval aID, jsval *aVp)
{
    return PR_FALSE;
}

PRBool
nsRDFElement::GetProperty(JSContext *aContext, jsval aID, jsval *aVp)
{
    return PR_FALSE;
}

PRBool
nsRDFElement::SetProperty(JSContext *aContext, jsval aID, jsval *aVp)
{
    return PR_FALSE;
}

PRBool
nsRDFElement::EnumerateProperty(JSContext *aContext)
{
    return PR_FALSE;
}


PRBool
nsRDFElement::Resolve(JSContext *aContext, jsval aID)
{
    return PR_FALSE;
}


PRBool
nsRDFElement::Convert(JSContext *aContext, jsval aID)
{
    return PR_FALSE;
}


void
nsRDFElement::Finalize(JSContext *aContext)
{
}



////////////////////////////////////////////////////////////////////////
// nsIContent interface
//
//   Just to say this again (I said it in the header file), none of
//   the manipulators for nsIContent will do anything to the RDF
//   graph. These are assumed to be used only by the content model
//   constructor, who is presumed to be _using_ the RDF graph to
//   construct this content model.
//
//   You have been warned.
//

NS_IMETHODIMP
nsRDFElement::GetDocument(nsIDocument*& aResult) const
{
    nsIDocument* doc;
    nsresult rv = mDocument->QueryInterface(kIDocumentIID, (void**) &doc);
    aResult = doc; // implicit AddRef() from QI
    return rv;
}

NS_IMETHODIMP
nsRDFElement::SetDocument(nsIDocument* aDocument, PRBool aDeep)
{
    nsresult rv;

    if (aDocument) {
        nsIRDFDocument* doc;
        if (NS_FAILED(rv = aDocument->QueryInterface(kIRDFDocumentIID,
                                                     (void**) &doc)))
            return rv;

        mDocument = doc;
        NS_RELEASE(doc); // not refcounted
    }

    if (aDeep && mChildren) {
        for (PRInt32 i = mChildren->Count() - 1; i >= 0; --i) {
            // XXX this entire block could be more rigorous about
            // dealing with failure.
            nsISupports* obj = mChildren->ElementAt(i);

            PR_ASSERT(obj);
            if (! obj)
                continue;

            nsIContent* child;
            if (NS_SUCCEEDED(obj->QueryInterface(kIContentIID, (void**) &child))) {
                child->SetDocument(aDocument, aDeep);
                NS_RELEASE(child);
            }

            NS_RELEASE(obj);
        }
    }

    return NS_OK;
}

NS_IMETHODIMP
nsRDFElement::GetParent(nsIContent*& aResult) const
{
    if (!mParent)
        return NS_ERROR_NOT_INITIALIZED;

    aResult = mParent;
    NS_ADDREF(aResult);

    return NS_OK;
}

NS_IMETHODIMP
nsRDFElement::SetParent(nsIContent* aParent)
{
    mParent = aParent; // no refcount
    return NS_OK;
}

NS_IMETHODIMP
nsRDFElement::CanContainChildren(PRBool& aResult) const
{
    // XXX Hmm -- not sure if this is unilaterally true...
    aResult = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
nsRDFElement::ChildCount(PRInt32& aResult) const
{
    nsresult rv;
    if (mChildrenMustBeGenerated) {
        if (NS_FAILED(rv = GenerateChildren()))
            return rv;
    }

    aResult = mChildren->Count();
    return NS_OK;
}

NS_IMETHODIMP
nsRDFElement::ChildAt(PRInt32 aIndex, nsIContent*& aResult) const
{
    nsresult rv;
    if (mChildrenMustBeGenerated) {
        if (NS_FAILED(rv = GenerateChildren()))
            return rv;
    }

    nsISupports* obj = mChildren->ElementAt(aIndex);
    nsIContent* content;
    rv = obj->QueryInterface(kIContentIID, (void**) &content);
    obj->Release();

    aResult = content;
    return rv;
}

NS_IMETHODIMP
nsRDFElement::IndexOf(nsIContent* aPossibleChild, PRInt32& aResult) const
{
    nsresult rv;
    if (mChildrenMustBeGenerated) {
        if (NS_FAILED(rv = GenerateChildren()))
            return rv;
    }
    aResult = mChildren->IndexOf(aPossibleChild);
    return NS_OK;
}

NS_IMETHODIMP
nsRDFElement::InsertChildAt(nsIContent* aKid, PRInt32 aIndex, PRBool aNotify)
{
    NS_PRECONDITION(aKid, "null ptr");
    if (! aKid)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(mChildren, "uninitialized");
    if (! mChildren)
        return NS_ERROR_NOT_INITIALIZED;

    if (! mChildren->InsertElementAt(aKid, aIndex)) {
        PR_ASSERT(0);
        return NS_ERROR_ILLEGAL_VALUE;
    }
    aKid->SetParent(this);
    if (aNotify) {
        // XXX deal with aNotify
    }
    return NS_OK;
}

NS_IMETHODIMP
nsRDFElement::ReplaceChildAt(nsIContent* aKid, PRInt32 aIndex, PRBool aNotify)
{
    NS_PRECONDITION(aKid, "null ptr");
    if (! aKid)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(mChildren, "uninitialized");
    if (! mChildren)
        return NS_ERROR_NOT_INITIALIZED;

    if (! mChildren->ReplaceElementAt(aKid, aIndex)) {
        PR_ASSERT(0);
        return NS_ERROR_ILLEGAL_VALUE;
    }
    aKid->SetParent(this);
    if (aNotify) {
        // XXX deal with aNotify
    }
    return NS_OK;
}

NS_IMETHODIMP
nsRDFElement::AppendChildTo(nsIContent* aKid, PRBool aNotify)
{
    NS_PRECONDITION(aKid, "null ptr");
    if (! aKid)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(mChildren, "uninitialized");
    if (! mChildren)
        return NS_ERROR_NOT_INITIALIZED;

    mChildren->AppendElement(aKid);
    aKid->SetParent(this);
    if (aNotify) {
        // XXX deal with aNotify
    }
    return NS_OK;
}

NS_IMETHODIMP
nsRDFElement::RemoveChildAt(PRInt32 aIndex, PRBool aNotify)
{
    NS_PRECONDITION(mChildren, "uninitialized");
    if (! mChildren)
        return NS_ERROR_NOT_INITIALIZED;

    // XXX probably need to SetParent(nsnull) on the guy.

    if (! mChildren->RemoveElementAt(aIndex)) {
        PR_ASSERT(0);
        return NS_ERROR_ILLEGAL_VALUE;
    }
    if (aNotify) {
        // XXX deal with aNotify
    }
    return NS_OK;
}

NS_IMETHODIMP
nsRDFElement::IsSynthetic(PRBool& aResult)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsRDFElement::GetTag(nsIAtom*& aResult) const
{
    // XXX the problem with this is that we only have a
    // fully-qualified URI, not "just" the tag. And I think the style
    // system ain't gonna work right with that. So this is a complete
    // hack to parse what probably _was_ the tag out.
    nsAutoString s = mTag;
    PRInt32 index;

    if ((index = s.RFind('#')) >= 0)
        s.Cut(0, index + 1);
    else if ((index = s.RFind('/')) >= 0)
        s.Cut(0, index + 1);

    s.ToUpperCase(); // because that's how CSS works

    aResult = NS_NewAtom(s);
    return NS_OK;
}


// XXX attribute code swiped from nsGenericContainerElement
// this class could probably just use nsGenericContainerElement
// needed to maintain attribute namespace ID as well as ordering
NS_IMETHODIMP 
nsRDFElement::SetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName, 
                           const nsString& aValue,
                           PRBool aNotify)
{
  NS_ASSERTION(kNameSpaceID_Unknown != aNameSpaceID, "must have name space ID");
  if (kNameSpaceID_Unknown == aNameSpaceID) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  NS_ASSERTION(nsnull != aName, "must have attribute name");
  if (nsnull == aName) {
    return NS_ERROR_NULL_POINTER;
  }

  nsresult rv = NS_ERROR_OUT_OF_MEMORY;

  if (nsnull == mAttributes) {
    mAttributes = new nsVoidArray();
  }
  if (nsnull != mAttributes) {
    nsGenericAttribute* attr;
    PRInt32 index;
    PRInt32 count = mAttributes->Count();
    for (index = 0; index < count; index++) {
      attr = (nsGenericAttribute*)mAttributes->ElementAt(index);
      if ((aNameSpaceID == attr->mNameSpaceID) && (aName == attr->mName)) {
        attr->mValue = aValue;
        rv = NS_OK;
        break;
      }
    }
    
    if (index >= count) { // didn't find it
      attr = new nsGenericAttribute(aNameSpaceID, aName, aValue);
      if (nsnull != attr) {
        mAttributes->AppendElement(attr);
        rv = NS_OK;
      }
    }
    // XXX notify doc?
  }

  return rv;
}


NS_IMETHODIMP
nsRDFElement::GetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName, nsString& aResult) const
{
  NS_ASSERTION(nsnull != aName, "must have attribute name");
  if (nsnull == aName) {
    return NS_ERROR_NULL_POINTER;
  }

  nsresult rv = NS_CONTENT_ATTR_NOT_THERE;

  if (nsnull != mAttributes) {
    PRInt32 count = mAttributes->Count();
    PRInt32 index;
    for (index = 0; index < count; index++) {
      const nsGenericAttribute* attr = (const nsGenericAttribute*)mAttributes->ElementAt(index);
      if ((attr->mNameSpaceID == aNameSpaceID) && (attr->mName == aName)) {
        aResult = attr->mValue;
        if (0 < aResult.Length()) {
          rv = NS_CONTENT_ATTR_HAS_VALUE;
        }
        else {
          rv = NS_CONTENT_ATTR_NO_VALUE;
        }
        break;
      }
    }
  }

#if defined(CREATE_PROPERTIES_AS_ATTRIBUTES)
    // XXX I'm not sure if we should support properties as attributes
    // or not...
    nsIRDFService* mgr = nsnull;
    if (NS_FAILED(rv = nsServiceManager::GetService(kRDFServiceCID,
                                                    kIRDFServiceIID,
                                                    (nsISupports**) &mgr)))
        return rv;
    
    nsIRDFDataBase* db   = nsnull;
    nsIRDFNode* property = nsnull;
    nsIRDFNode* value    = nsnull;

    if (NS_FAILED(rv = mDocument->GetDataBase(db)))
        goto done;

    if (NS_FAILED(rv = mgr->GetNode(aName, property)))
        goto done;

    // XXX Only returns the first value. yer screwed for
    // multi-attributes, I guess.

    if (NS_FAILED(rv = db->GetTarget(mResource, property, PR_TRUE, value)))
        goto done;

    rv = value->GetStringValue(aResult);

done:
    NS_IF_RELEASE(property);
    NS_IF_RELEASE(value);
    NS_IF_RELEASE(db);
    nsServiceManager::ReleaseService(kRDFServiceCID, mgr);

#endif // defined(CREATE_PROPERTIES_AS_ATTRIBUTES)
    return rv;
}

NS_IMETHODIMP
nsRDFElement::UnsetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName, PRBool aNotify)
{
  NS_ASSERTION(nsnull != aName, "must have attribute name");
  if (nsnull == aName) {
    return NS_ERROR_NULL_POINTER;
  }

  nsresult rv = NS_OK;

  if (nsnull != mAttributes) {
    PRInt32 count = mAttributes->Count();
    PRInt32 index;
    for (index = 0; index < count; index++) {
      nsGenericAttribute* attr = (nsGenericAttribute*)mAttributes->ElementAt(index);
      if ((attr->mNameSpaceID == aNameSpaceID) && (attr->mName == aName)) {
        mAttributes->RemoveElementAt(index);
        delete attr;
        break;
      }
    }

// XXX notify document??
  }

  return rv;
}

NS_IMETHODIMP
nsRDFElement::GetAttributeNameAt(PRInt32 aIndex,
                                 PRInt32& aNameSpaceID, 
                                 nsIAtom*& aName) const
{
  if (nsnull != mAttributes) {
    nsGenericAttribute* attr = (nsGenericAttribute*)mAttributes->ElementAt(aIndex);
    if (nsnull != attr) {
      aNameSpaceID = attr->mNameSpaceID;
      aName = attr->mName;
      NS_IF_ADDREF(aName);
      return NS_OK;
    }
  }
  aNameSpaceID = kNameSpaceID_None;
  aName = nsnull;
  return NS_ERROR_ILLEGAL_VALUE;

#if defined(CREATE_PROPERTIES_AS_ATTRIBUTES)
    // XXX I'm not sure if we should support attributes or not...

    nsIRDFService* mgr = nsnull;
    if (NS_FAILED(rv = nsServiceManager::GetService(kRDFServiceCID,
                                                    kIRDFServiceIID,
                                                    (nsISupports**) &mgr)))
        return rv;
    
    nsIRDFDataBase* db       = nsnull;
    nsIRDFCursor* properties = nsnull;
    PRBool moreProperties;

    if (NS_FAILED(rv = mDocument->GetDataBase(db)))
        goto done;

    if (NS_FAILED(rv = db->ArcLabelsOut(mResource, properties)))
        goto done;

    while (NS_SUCCEEDED(rv = properties->HasMoreElements(moreProperties)) && moreProperties) {
        nsIRDFNode* property = nsnull;
        PRBool tv;

        if (NS_FAILED(rv = properties->GetNext(property, tv /* ignored */)))
            break;

        nsAutoString uri;
        if (NS_SUCCEEDED(rv = property->GetStringValue(uri))) {
            nsIAtom* atom = NS_NewAtom(uri);
            if (atom) {
                aArray->AppendElement(atom);
                ++aResult;
            } else {
                rv = NS_ERROR_OUT_OF_MEMORY;
            }
        }

        NS_RELEASE(property);

        if (NS_FAILED(rv))
            break;
    }

done:
    NS_IF_RELEASE(properties);
    NS_IF_RELEASE(db);
    nsServiceManager::ReleaseService(kRDFServiceCID, mgr);

    return rv;
#endif // defined(CREATE_PROPERTIES_AS_ATTRIBUTES)
}

NS_IMETHODIMP
nsRDFElement::GetAttributeCount(PRInt32& aResult) const
{
    nsresult rv = NS_OK;
    if (nsnull != mAttributes) {
      aResult = mAttributes->Count();
    }
    else {
      aResult = 0;
    }

#if defined(CREATE_PROPERTIES_AS_ATTRIBUTES)
    PR_ASSERT(0);     // XXX need to write this...
#endif // defined(CREATE_PROPERTIES_AS_ATTRIBUTES)

    return rv;
}


static void
rdf_Indent(FILE* out, PRInt32 aIndent)
{
    for (PRInt32 i = aIndent; --i >= 0; ) fputs("  ", out);
}

NS_IMETHODIMP
nsRDFElement::List(FILE* out, PRInt32 aIndent) const
{
    nsresult rv;

    {
        nsIAtom* tag;
        if (NS_FAILED(rv = GetTag(tag)))
            return rv;

        rdf_Indent(out, aIndent);
        fputs("[RDF ", out);
        fputs(tag->GetUnicode(), out);

        NS_RELEASE(tag);
    }

    {
        PRInt32 nattrs;

        if (NS_SUCCEEDED(rv = GetAttributeCount(nattrs))) {
            for (PRInt32 i = 0; i < nattrs; ++i) {
                nsIAtom* attr = nsnull;
                PRInt32 nameSpaceID;
                GetAttributeNameAt(i, nameSpaceID, attr);


                nsAutoString v;
                GetAttribute(nameSpaceID, attr, v);

                nsAutoString s;
                attr->ToString(s);
                NS_RELEASE(attr);

                fputs(" ", out);
                fputs(s, out);
                fputs("=", out);
                fputs(v, out);
            }
        }

        if (NS_FAILED(rv))
            return rv;
    }

    fputs("]\n", out);

    {
        PRInt32 nchildren;
        if (NS_FAILED(rv = ChildCount(nchildren)))
            return rv;

        for (PRInt32 i = 0; i < nchildren; ++i) {
            nsIContent* child;
            if (NS_FAILED(rv = ChildAt(i, child)))
                return rv;

            rv = child->List(out, aIndent + 1);
            NS_RELEASE(child);

            if (NS_FAILED(rv))
                return rv;
        }
    }

    return NS_OK;
}

NS_IMETHODIMP
nsRDFElement::BeginConvertToXIF(nsXIFConverter& aConverter) const
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsRDFElement::ConvertContentToXIF(nsXIFConverter& aConverter) const
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsRDFElement::FinishConvertToXIF(nsXIFConverter& aConverter) const
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsRDFElement::SizeOf(nsISizeOfHandler* aHandler) const
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP 
nsRDFElement::HandleDOMEvent(nsIPresContext& aPresContext,
                             nsEvent* aEvent,
                             nsIDOMEvent** aDOMEvent,
                             PRUint32 aFlags,
                             nsEventStatus& aEventStatus)
{
    return NS_OK;
}



NS_IMETHODIMP 
nsRDFElement::RangeAdd(nsIDOMRange& aRange) 
{  
  // rdf content does not yet support DOM ranges
  return NS_OK;
}


 
NS_IMETHODIMP 
nsRDFElement::RangeRemove(nsIDOMRange& aRange) 
{
  // rdf content does not yet support DOM ranges
  return NS_OK;
}                                                                        


NS_IMETHODIMP 
nsRDFElement::GetRangeList(nsVoidArray*& aResult) const
{
  // rdf content does not yet support DOM ranges
  return NS_OK;
}


////////////////////////////////////////////////////////////////////////
// nsIXMLContent

NS_IMETHODIMP 
nsRDFElement::SetNameSpacePrefix(nsIAtom* aNameSpacePrefix)
{
    NS_IF_RELEASE(mNameSpacePrefix);
    mNameSpacePrefix = aNameSpacePrefix;
    NS_IF_ADDREF(mNameSpacePrefix);
    return NS_OK;
}

NS_IMETHODIMP 
nsRDFElement::GetNameSpacePrefix(nsIAtom*& aNameSpacePrefix) const
{
    aNameSpacePrefix = mNameSpacePrefix;
    NS_IF_ADDREF(mNameSpacePrefix);
    return NS_OK;
}

NS_IMETHODIMP
nsRDFElement::SetNameSpaceID(PRInt32 aNameSpaceID)
{
    mNameSpaceID = aNameSpaceID;
    return NS_OK;
}

NS_IMETHODIMP 
nsRDFElement::GetNameSpaceID(PRInt32& aNameSpaceID) const
{
    aNameSpaceID = mNameSpaceID;
    return NS_OK;
}


////////////////////////////////////////////////////////////////////////
// nsIRDFContent

NS_IMETHODIMP
nsRDFElement::Init(nsIRDFDocument* doc,
                   const nsString& aTag,
                   nsIRDFResource* resource,
                   PRBool childrenMustBeGenerated)
{
    NS_PRECONDITION(doc, "null ptr");

    // XXX Not using this because of a hack in nsRDFTreeDocument: need
    // to expose generic XML elements!
    //NS_PRECONDITION(resource, "null ptr");

    if (!doc /* || !resource // XXX ibid */)
        return NS_ERROR_NULL_POINTER;

    mTag = aTag;

    mDocument = doc; // not refcounted

    mResource = resource;
    NS_IF_ADDREF(mResource);

    mChildrenMustBeGenerated = childrenMustBeGenerated;

    nsresult rv;
    if (NS_FAILED(rv = NS_NewISupportsArray(&mChildren)))
        return rv;

    return NS_OK;
}

NS_IMETHODIMP
nsRDFElement::SetResource(const nsString& aURI)
{
    if (mResource)
        return NS_ERROR_ALREADY_INITIALIZED;

    nsresult rv;

    nsIRDFService* service = nsnull;
    if (NS_FAILED(rv = nsServiceManager::GetService(kRDFServiceCID,
                                                    kIRDFServiceIID,
                                                    (nsISupports**) &service)))
        return rv;
    
    rv = service->GetUnicodeResource(aURI, &mResource); // implicit AddRef()
    nsServiceManager::ReleaseService(kRDFServiceCID, service);

    return rv;
}

NS_IMETHODIMP
nsRDFElement::GetResource(nsString& rURI) const
{
    if (! mResource)
        return NS_ERROR_NOT_INITIALIZED;

    const char* s;
    nsresult rv;
    if (NS_FAILED(rv = mResource->GetValue(&s)))
        return rv;

    rURI = s;
    return NS_OK;
}


NS_IMETHODIMP
nsRDFElement::SetResource(nsIRDFResource* aResource)
{
    NS_PRECONDITION(aResource, "null ptr");
    NS_PRECONDITION(!mResource, "already initialized");

    if (!aResource)
        return NS_ERROR_NULL_POINTER;

    if (mResource)
        return NS_ERROR_ALREADY_INITIALIZED;

    mResource = aResource;
    NS_ADDREF(mResource);
    return NS_OK;
}


NS_IMETHODIMP
nsRDFElement::GetResource(nsIRDFResource*& aResource)
{
    if (! mResource)
        return NS_ERROR_NOT_INITIALIZED;

    aResource = mResource;
    NS_ADDREF(aResource);
    return NS_OK;
}

NS_IMETHODIMP
nsRDFElement::SetProperty(const nsString& aPropertyURI, const nsString& aValue)
{
    if (!mResource || !mDocument)
        return NS_ERROR_NOT_INITIALIZED;

    const char* s;
    mResource->GetValue(&s);
    nsAutoString resource = s;

#ifdef DEBUG_waterson
    char buf[256];
    printf("assert [\n");
    printf("  %s\n", resource.ToCString(buf, sizeof(buf)));
    printf("  %s\n", aPropertyURI.ToCString(buf, sizeof(buf)));
    printf("  %s\n", aValue.ToCString(buf, sizeof(buf)));
    printf("]\n");
#endif

    nsresult rv;
    nsIRDFService* service = nsnull;

    if (NS_FAILED(rv = nsServiceManager::GetService(kRDFServiceCID,
                                                    kIRDFServiceIID,
                                                    (nsISupports**) &service)))
        return rv;
    
    nsIRDFResource* property = nsnull;
    nsIRDFLiteral* value     = nsnull;
    nsIRDFDataBase* db       = nsnull;

    if (NS_FAILED(rv = service->GetUnicodeResource(aPropertyURI, &property)))
        goto done;

    // XXX assume it's a literal?
    if (NS_FAILED(rv = service->GetLiteral(aValue, &value)))
        goto done;

    if (NS_FAILED(rv = mDocument->GetDataBase(db)))
        goto done;

    rv = db->Assert(mResource, property, value, PR_TRUE);

done:
    NS_IF_RELEASE(db);
    NS_IF_RELEASE(value);
    NS_IF_RELEASE(property);
    nsServiceManager::ReleaseService(kRDFServiceCID, service);

    return rv;
}

NS_IMETHODIMP
nsRDFElement::GetProperty(const nsString& aPropertyURI, nsString& rValue) const
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


////////////////////////////////////////////////////////////////////////
// Implementation methods


nsresult
nsRDFElement::GenerateChildren(void) const
{
    nsresult rv;

    if (!mResource || !mDocument)
        return NS_ERROR_NOT_INITIALIZED;

    // XXX hack because we can't use "mutable"
    nsRDFElement* unconstThis = NS_CONST_CAST(nsRDFElement*, this);

    if (! unconstThis->mChildren) {
        if (NS_FAILED(rv = NS_NewISupportsArray(&unconstThis->mChildren)))
            return rv;
    }
    else {
        unconstThis->mChildren->Clear();
    }

    // Clear this value *first*, so we can re-enter the nsIContent
    // getters if needed.
    unconstThis->mChildrenMustBeGenerated = PR_FALSE;

    if (NS_FAILED(rv = mDocument->CreateChildren(unconstThis))) {
        // Well, maybe it was a transient error. This'll let use try
        // again some time in the future.
        unconstThis->mChildrenMustBeGenerated = PR_TRUE;
        return rv;
    }

    return NS_OK;
}

