/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */


/*

  A helper class used to implement attributes.

*/


/*
 * Notes
 *
 * A lot of these methods delegate back to the original content node
 * that created them. This is so that we can lazily produce attribute
 * values from the RDF graph as they're asked for.
 *
 */

#include "nsCOMPtr.h"
#include "nsDOMCID.h"
#include "nsIContent.h"
#include "nsINodeInfo.h"
#include "nsICSSParser.h"
#include "nsIDOMElement.h"
#include "nsIDOMScriptObjectFactory.h"
#include "nsINameSpaceManager.h"
#include "nsIServiceManager.h"
#include "nsIURL.h"
#include "nsXULAttributes.h"
#include "nsLayoutCID.h"
#include "nsReadableUtils.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_CID(kDOMScriptObjectFactoryCID, NS_DOM_SCRIPT_OBJECT_FACTORY_CID);
static NS_DEFINE_CID(kCSSParserCID, NS_CSSPARSER_CID);
static NS_DEFINE_CID(kICSSParserIID, NS_ICSS_PARSER_IID);


const PRInt32 nsXULAttribute::kMaxAtomValueLength = 12;

//----------------------------------------------------------------------
//
// nsClassList
//

PRBool
nsClassList::HasClass(nsClassList* aList, nsIAtom* aClass)
{
    const nsClassList* classList = aList;
    while (nsnull != classList) {
        if (classList->mAtom.get() == aClass) {
            return PR_TRUE;
        }
        classList = classList->mNext;
    }
    return PR_FALSE;
}


nsresult
nsClassList::GetClasses(nsClassList* aList, nsVoidArray& aArray)
{
    aArray.Clear();
    const nsClassList* classList = aList;
    while (nsnull != classList) {
        aArray.AppendElement(classList->mAtom); // NOTE atom is not addrefed
        classList = classList->mNext;
    }
    return NS_OK;
}



nsresult
nsClassList::ParseClasses(nsClassList** aList, const nsAReadableString& aClassString)
{
    static const PRUnichar kNullCh = PRUnichar('\0');

    if (*aList != nsnull) {
        delete *aList;
        *aList = nsnull;
    }

    if (aClassString.Length() > 0) {
        nsAutoString classStr(aClassString);  // copy to work buffer
        classStr.Append(kNullCh);  // put an extra null at the end

        PRUnichar* start = (PRUnichar*)(const PRUnichar*)classStr.GetUnicode();
        PRUnichar* end   = start;

        while (kNullCh != *start) {
            while ((kNullCh != *start) && nsCRT::IsAsciiSpace(*start)) {  // skip leading space
                start++;
            }
            end = start;

            while ((kNullCh != *end) && (PR_FALSE == nsCRT::IsAsciiSpace(*end))) { // look for space or end
                end++;
            }
            *end = kNullCh; // end string here

            if (start < end) {
                *aList = new nsClassList(NS_NewAtom(start));
                aList = &((*aList)->mNext);
            }

            start = ++end;
        }
    }
    return NS_OK;
}



//----------------------------------------------------------------------
//
// nsXULAttribute
//

PRInt32 nsXULAttribute::gRefCnt;
nsIAtom* nsXULAttribute::kIdAtom;

const PRInt32 nsXULAttribute::kBlockSize = 512;

nsXULAttribute* nsXULAttribute::gFreeList;

void*
nsXULAttribute::operator new(size_t aSize)
{
    // Simple fixed size allocator for nsXULAttribute objects. Look
    // for an object on the |gFreeList|, if there isn't one, create a
    // new block of 'em.
    if (aSize != sizeof(nsXULAttribute))
        return ::operator new(aSize);

    nsXULAttribute* result = gFreeList;

    if (result) {
        gFreeList = gFreeList->mNext;
    }
    else {
        // Create a chunk o' memory for the freelist.
        nsXULAttribute* block =
            NS_STATIC_CAST(nsXULAttribute*, ::operator new(kBlockSize * sizeof(nsXULAttribute)));

        if (! block)
            return nsnull;

        // Thread it.
        for (PRInt32 i = 1; i < kBlockSize - 1; ++i)
            block[i].mNext = &block[i + 1];

        block[kBlockSize - 1].mNext = nsnull;

        result = block;
        gFreeList = &block[1];
    }

    return result;
}


void
nsXULAttribute::operator delete(void* aObject, size_t aSize)
{
    // Return |aObject| to |gFreeList|, if we can.
    if (! aObject)
        return;

    if (aSize != sizeof(nsXULAttribute)) {
        ::operator delete(aObject);
        return;
    }

#ifdef DEBUG
   nsCRT::memset(aObject, 0xdd, aSize);
#endif

    nsXULAttribute* doomed = NS_STATIC_CAST(nsXULAttribute*, aObject);

    doomed->mNext = gFreeList;
    gFreeList = doomed;
}

nsXULAttribute::nsXULAttribute(nsIContent* aContent,
                               nsINodeInfo* aNodeInfo,
                               const nsAReadableString& aValue)
    : mContent(aContent),
      mScriptObject(nsnull),
      mNodeInfo(aNodeInfo),
      mValue(nsnull)
{
    NS_INIT_REFCNT();

    if (gRefCnt++ == 0) {
        kIdAtom = NS_NewAtom("id");
    }

    NS_IF_ADDREF(aNodeInfo);
    SetValueInternal(aValue);
}

nsXULAttribute::~nsXULAttribute()
{
    NS_IF_RELEASE(mNodeInfo);
    ReleaseValue();

    if (--gRefCnt == 0) {
        NS_IF_RELEASE(kIdAtom);
    }
}

nsresult
nsXULAttribute::Create(nsIContent* aContent,
                       nsINodeInfo* aNodeInfo,
                       const nsAReadableString& aValue,
                       nsXULAttribute** aResult)
{
    NS_ENSURE_ARG_POINTER(aNodeInfo);
    NS_PRECONDITION(aResult != nsnull, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    if (! (*aResult = new nsXULAttribute(aContent, aNodeInfo, aValue)))
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*aResult);
    return NS_OK;
}

// nsISupports interface
NS_IMPL_ADDREF(nsXULAttribute);
NS_IMPL_RELEASE(nsXULAttribute);

NS_IMETHODIMP
nsXULAttribute::QueryInterface(REFNSIID aIID, void** aResult)
{
    NS_PRECONDITION(aResult != nsnull, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    if (aIID.Equals(NS_GET_IID(nsIDOMAttr)) ||
        aIID.Equals(NS_GET_IID(nsIDOMNode)) ||
        aIID.Equals(kISupportsIID)) {
        *aResult = NS_STATIC_CAST(nsIDOMAttr*, this);
        NS_ADDREF(this);
        return NS_OK;
    }
    else if (aIID.Equals(NS_GET_IID(nsIScriptObjectOwner))) {
        *aResult = NS_STATIC_CAST(nsIScriptObjectOwner*, this);
        NS_ADDREF(this);
        return NS_OK;
    }
    else {
        *aResult = nsnull;
        return NS_NOINTERFACE;
    }
}

// nsIDOMNode interface

NS_IMETHODIMP
nsXULAttribute::GetNodeName(nsAWritableString& aNodeName)
{
    GetQualifiedName(aNodeName);
    return NS_OK;
}

NS_IMETHODIMP
nsXULAttribute::GetNodeValue(nsAWritableString& aNodeValue)
{
    return GetValueInternal(aNodeValue);
}

NS_IMETHODIMP
nsXULAttribute::SetNodeValue(const nsAReadableString& aNodeValue)
{
    return SetValue(aNodeValue);
}

NS_IMETHODIMP
nsXULAttribute::GetNodeType(PRUint16* aNodeType)
{
    *aNodeType = (PRUint16)nsIDOMNode::ATTRIBUTE_NODE;
    return NS_OK;
}

NS_IMETHODIMP
nsXULAttribute::GetParentNode(nsIDOMNode** aParentNode)
{
    *aParentNode = nsnull;
    return NS_OK;
}

NS_IMETHODIMP
nsXULAttribute::GetChildNodes(nsIDOMNodeList** aChildNodes)
{
    NS_NOTYETIMPLEMENTED("write me");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsXULAttribute::GetFirstChild(nsIDOMNode** aFirstChild)
{
    NS_NOTYETIMPLEMENTED("write me");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsXULAttribute::GetLastChild(nsIDOMNode** aLastChild)
{
    NS_NOTYETIMPLEMENTED("write me");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsXULAttribute::GetPreviousSibling(nsIDOMNode** aPreviousSibling)
{
    *aPreviousSibling = nsnull;
    return NS_OK;
}

NS_IMETHODIMP
nsXULAttribute::GetNextSibling(nsIDOMNode** aNextSibling)
{
    *aNextSibling = nsnull;
    return NS_OK;
}

NS_IMETHODIMP
nsXULAttribute::GetAttributes(nsIDOMNamedNodeMap** aAttributes)
{
    *aAttributes = nsnull;
    return NS_OK;
}

NS_IMETHODIMP
nsXULAttribute::GetOwnerDocument(nsIDOMDocument** aOwnerDocument)
{
    NS_NOTYETIMPLEMENTED("write me");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsXULAttribute::GetNamespaceURI(nsAWritableString& aNamespaceURI)
{
  return mNodeInfo->GetNamespaceURI(aNamespaceURI);
}

NS_IMETHODIMP
nsXULAttribute::GetPrefix(nsAWritableString& aPrefix)
{
  return mNodeInfo->GetPrefix(aPrefix);
}

NS_IMETHODIMP
nsXULAttribute::SetPrefix(const nsAReadableString& aPrefix)
{
    // XXX: Validate the prefix string!

    nsINodeInfo *newNodeInfo = nsnull;
    nsCOMPtr<nsIAtom> prefix;

    if (aPrefix.Length()) {
        prefix = dont_AddRef(NS_NewAtom(aPrefix));
        NS_ENSURE_TRUE(prefix, NS_ERROR_OUT_OF_MEMORY);
    }

    nsresult rv = mNodeInfo->PrefixChanged(prefix, newNodeInfo);
    NS_ENSURE_SUCCESS(rv, rv);

    NS_RELEASE(mNodeInfo);
    mNodeInfo = newNodeInfo;

    return NS_OK;
}

NS_IMETHODIMP
nsXULAttribute::GetLocalName(nsAWritableString& aLocalName)
{
  return mNodeInfo->GetLocalName(aLocalName);
}

NS_IMETHODIMP
nsXULAttribute::InsertBefore(nsIDOMNode* aNewChild, nsIDOMNode* aRefChild, nsIDOMNode** aReturn)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsXULAttribute::ReplaceChild(nsIDOMNode* aNewChild, nsIDOMNode* aOldChild, nsIDOMNode** aReturn)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsXULAttribute::RemoveChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsXULAttribute::AppendChild(nsIDOMNode* aNewChild, nsIDOMNode** aReturn)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsXULAttribute::HasChildNodes(PRBool* aReturn)
{
    NS_ENSURE_ARG_POINTER(aReturn);
    *aReturn = PR_FALSE;
    return NS_OK;
}

NS_IMETHODIMP
nsXULAttribute::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
    NS_NOTYETIMPLEMENTED("write me");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsXULAttribute::Normalize()
{
  return NS_OK;
}

NS_IMETHODIMP
nsXULAttribute::Supports(const nsAReadableString& aFeature,
                         const nsAReadableString& aVersion,
                         PRBool* aReturn)
{
  NS_NOTYETIMPLEMENTED("write me");
  return NS_ERROR_NOT_IMPLEMENTED;
}


// nsIDOMAttr interface

NS_IMETHODIMP
nsXULAttribute::GetName(nsAWritableString& aName)
{
    GetQualifiedName(aName);
    return NS_OK;
}

NS_IMETHODIMP
nsXULAttribute::GetSpecified(PRBool* aSpecified)
{
    // XXX this'll break when we make Clone() work
    *aSpecified = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
nsXULAttribute::GetValue(nsAWritableString& aValue)
{
    return GetValueInternal(aValue);
}

NS_IMETHODIMP
nsXULAttribute::SetValue(const nsAReadableString& aValue)
{
    // We call back to the content node's SetValue() method so we can
    // share all of the work that it does.
    return mContent->SetAttribute(mNodeInfo, aValue, PR_TRUE);
}

NS_IMETHODIMP
nsXULAttribute::GetOwnerElement(nsIDOMElement** aOwnerElement)
{
  NS_ENSURE_ARG_POINTER(aOwnerElement);

  return mContent->QueryInterface(NS_GET_IID(nsIDOMElement),
                                  (void **)aOwnerElement);
}


// nsIScriptObjectOwner interface

NS_IMETHODIMP
nsXULAttribute::GetScriptObject(nsIScriptContext* aContext, void** aScriptObject)
{
    nsresult rv = NS_OK;
    if (! mScriptObject) {
        nsIDOMScriptObjectFactory *factory;
    
        rv = nsServiceManager::GetService(kDOMScriptObjectFactoryCID,
                                          NS_GET_IID(nsIDOMScriptObjectFactory),
                                          (nsISupports **)&factory);

        if (NS_FAILED(rv))
            return rv;

        rv = factory->NewScriptAttr(aContext, 
                                    (nsISupports*)(nsIDOMAttr*) this,
                                    (nsISupports*) mContent,
                                    (void**) &mScriptObject);

        nsServiceManager::ReleaseService(kDOMScriptObjectFactoryCID, factory);
    }

    *aScriptObject = mScriptObject;
    return rv;
}


NS_IMETHODIMP
nsXULAttribute::SetScriptObject(void *aScriptObject)
{
    mScriptObject = aScriptObject;
    return NS_OK;
}


// Implementation methods

void
nsXULAttribute::GetQualifiedName(nsAWritableString& aQualifiedName)
{
    mNodeInfo->GetQualifiedName(aQualifiedName);
}



nsresult
nsXULAttribute::SetValueInternal(const nsAReadableString& aValue)
{
    nsCOMPtr<nsIAtom> newAtom;

    // Atomize the value if it is short, or if it is the 'id'
    // attribute. We atomize the 'id' attribute to "prime" the global
    // atom table: the style system frequently asks for it, and if the
    // table is "unprimed" we see quite a bit of thrashing as the 'id'
    // value is repeatedly added and then removed from the atom table.
    if ((aValue.Length() <= kMaxAtomValueLength) || mNodeInfo->Equals(kIdAtom)) {
        newAtom = getter_AddRefs( NS_NewAtom(aValue) );
    }

    if (mValue) {
        // Release the old value
        ReleaseValue();
    }

    // ...and set the new value
    if (newAtom) {
        NS_ADDREF((nsIAtom*)newAtom.get());
        mValue = (void*)(PRWord(newAtom.get()) | kAtomType);
    }
    else {
        PRUnichar* str = ToNewUnicode(aValue);
        if (! str)
            return NS_ERROR_OUT_OF_MEMORY;

        mValue = str;
    }

    return NS_OK;
}

nsresult
nsXULAttribute::GetValueAsAtom(nsIAtom** aResult)
{
    if (! mValue) {
        *aResult = nsnull;
    }
    else if (IsStringValue()) {
        *aResult = NS_NewAtom((const PRUnichar*) mValue);
    }
    else {
        *aResult = (nsIAtom*)(PRWord(mValue) & ~PRWord(kTypeMask));
        NS_ADDREF(*aResult);
    }
    return NS_OK;
}


//----------------------------------------------------------------------
//
// nsXULAttributes
//

nsXULAttributes::nsXULAttributes(nsIContent* aContent)
    : mContent(aContent),
      mClassList(nsnull),
      mStyleRule(nsnull),
      mScriptObject(nsnull)
{
    NS_INIT_REFCNT();
}


nsXULAttributes::~nsXULAttributes()
{
    PRInt32 count = mAttributes.Count();
    for (PRInt32 indx = 0; indx < count; indx++) {
        nsXULAttribute* attr = NS_REINTERPRET_CAST(nsXULAttribute*, mAttributes.ElementAt(indx));
        NS_RELEASE(attr);
    }
    delete mClassList;
}


nsresult
nsXULAttributes::Create(nsIContent* aContent, nsXULAttributes** aResult)
{
    NS_PRECONDITION(aResult != nsnull, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    if (! (*aResult = new nsXULAttributes(aContent)))
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*aResult);
    return NS_OK;
}


// nsISupports interface

NS_IMPL_ADDREF(nsXULAttributes);
NS_IMPL_RELEASE(nsXULAttributes);

NS_IMETHODIMP
nsXULAttributes::QueryInterface(REFNSIID aIID, void** aResult)
{
    NS_PRECONDITION(aResult != nsnull, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    if (aIID.Equals(NS_GET_IID(nsIDOMNamedNodeMap)) ||
        aIID.Equals(kISupportsIID)) {
        *aResult = NS_STATIC_CAST(nsIDOMNamedNodeMap*, this);
        NS_ADDREF(this);
        return NS_OK;
    }
    else if (aIID.Equals(NS_GET_IID(nsIScriptObjectOwner))) {
        *aResult = NS_STATIC_CAST(nsIScriptObjectOwner*, this);
        NS_ADDREF(this);
        return NS_OK;
    }
    else {
        *aResult = nsnull;
        return NS_NOINTERFACE;
    }
}


// nsIDOMNamedNodeMap interface

NS_IMETHODIMP
nsXULAttributes::GetLength(PRUint32* aLength)
{
    NS_PRECONDITION(aLength != nsnull, "null ptr");
    if (! aLength)
        return NS_ERROR_NULL_POINTER;

    *aLength = mAttributes.Count();
    return NS_OK;
}

NS_IMETHODIMP
nsXULAttributes::GetNamedItem(const nsAReadableString& aName,
                              nsIDOMNode** aReturn)
{
    NS_PRECONDITION(aReturn != nsnull, "null ptr");
    if (! aReturn)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;
    *aReturn = nsnull;

    PRInt32 nameSpaceID;
    nsIAtom* name;
    nsCOMPtr<nsINodeInfo> inpNodeInfo;

    if (NS_FAILED(rv = mContent->NormalizeAttributeString(aName, *getter_AddRefs(inpNodeInfo))))
        return rv;

    if (kNameSpaceID_Unknown == nameSpaceID) {
      nameSpaceID = kNameSpaceID_None;  // ignore unknown prefix XXX is this correct?
    }
    // XXX doing this instead of calling mContent->GetAttribute() will
    // make it a lot harder to lazily instantiate properties from the
    // graph. The problem is, how else do we get the named item?
    for (PRInt32 i = mAttributes.Count() - 1; i >= 0; --i) {
        nsXULAttribute* attr = (nsXULAttribute*) mAttributes[i];
        nsINodeInfo *ni = attr->GetNodeInfo();

        if (inpNodeInfo->Equals(ni)) {
            NS_ADDREF(attr);
            *aReturn = attr;
            break;
        }
    }

    NS_RELEASE(name);
    return NS_OK;
}

NS_IMETHODIMP
nsXULAttributes::SetNamedItem(nsIDOMNode* aArg, nsIDOMNode** aReturn)
{
    NS_NOTYETIMPLEMENTED("write me");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsXULAttributes::RemoveNamedItem(const nsAReadableString& aName,
                                 nsIDOMNode** aReturn)
{
    nsCOMPtr<nsIDOMElement> element( do_QueryInterface(mContent) );
    if (element) {
        return element->RemoveAttribute(aName);
        *aReturn = nsnull; // XXX should be the element we just removed
        return NS_OK;
    }
    else {
        return NS_ERROR_FAILURE;
    }
}

NS_IMETHODIMP
nsXULAttributes::Item(PRUint32 aIndex, nsIDOMNode** aReturn)
{
    *aReturn = (nsXULAttribute*) mAttributes[aIndex];
    NS_IF_ADDREF(*aReturn);
    return NS_OK;
}

nsresult
nsXULAttributes::GetNamedItemNS(const nsAReadableString& aNamespaceURI, 
                                const nsAReadableString& aLocalName,
                                nsIDOMNode** aReturn)
{
  NS_NOTYETIMPLEMENTED("write me");
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
nsXULAttributes::SetNamedItemNS(nsIDOMNode* aArg, nsIDOMNode** aReturn)
{
  NS_NOTYETIMPLEMENTED("write me");
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
nsXULAttributes::RemoveNamedItemNS(const nsAReadableString& aNamespaceURI, 
                                   const nsAReadableString& aLocalName,
                                   nsIDOMNode** aReturn)
{
  NS_NOTYETIMPLEMENTED("write me");
  return NS_ERROR_NOT_IMPLEMENTED;
}



// nsIScriptObjectOwner interface

NS_IMETHODIMP
nsXULAttributes::GetScriptObject(nsIScriptContext* aContext, void** aScriptObject)
{
    nsresult rv = NS_OK;
    if (! mScriptObject) {
        nsIDOMScriptObjectFactory *factory;
    
        rv = nsServiceManager::GetService(kDOMScriptObjectFactoryCID,
                                          NS_GET_IID(nsIDOMScriptObjectFactory),
                                          (nsISupports **)&factory);

        if (NS_FAILED(rv))
            return rv;

        rv = factory->NewScriptNamedNodeMap(aContext, 
                                            (nsISupports*)(nsIDOMNamedNodeMap*) this, 
                                            (nsISupports*) mContent,
                                            (void**) &mScriptObject);

        nsServiceManager::ReleaseService(kDOMScriptObjectFactoryCID, factory);
    }

    *aScriptObject = mScriptObject;
    return rv;
}

NS_IMETHODIMP
nsXULAttributes::SetScriptObject(void *aScriptObject)
{
    mScriptObject = aScriptObject;
    return NS_OK;
}

// Implementation methods

nsresult 
nsXULAttributes::GetClasses(nsVoidArray& aArray) const
{
    return nsClassList::GetClasses(mClassList, aArray);
}

nsresult 
nsXULAttributes::HasClass(nsIAtom* aClass) const
{
    return nsClassList::HasClass(mClassList, aClass) ? NS_OK : NS_COMFALSE;
}

nsresult nsXULAttributes::SetClassList(nsClassList* aClassList)
{
    delete mClassList;
    if (aClassList) {
        mClassList = new nsClassList(*aClassList);
    }
    else {
        mClassList = nsnull;
    }
    return NS_OK;
}

nsresult nsXULAttributes::UpdateClassList(const nsAReadableString& aValue)
{
    return nsClassList::ParseClasses(&mClassList, aValue);
}

nsresult nsXULAttributes::UpdateStyleRule(nsIURI* aDocURL, const nsAReadableString& aValue)
{
    if (aValue.IsEmpty())
    {
      // XXX: Removing the rule. Is this sufficient?
      mStyleRule = nsnull;
      return NS_OK;
    }

    nsCOMPtr<nsICSSParser> css;
    nsresult result = nsComponentManager::CreateInstance(kCSSParserCID,
                                                         nsnull,
                                                         kICSSParserIID,
                                                         getter_AddRefs(css));
    if (NS_OK != result) {
      return result;
    }

    nsCOMPtr<nsIStyleRule> rule;
    result = css->ParseDeclarations(aValue, aDocURL, *getter_AddRefs(rule));
    
    if ((NS_OK == result) && rule) {
      mStyleRule = rule;
    }

    return NS_OK;
}


nsresult nsXULAttributes::SetInlineStyleRule(nsIStyleRule* aRule)
{
    mStyleRule = aRule;
    return NS_OK;
}

nsresult nsXULAttributes::GetInlineStyleRule(nsIStyleRule*& aRule)
{
  nsresult result = NS_ERROR_NULL_POINTER;
  if (mStyleRule != nsnull)
  {
    aRule = mStyleRule;
    NS_ADDREF(aRule);
    result = NS_OK;
  }

  return result;
}

