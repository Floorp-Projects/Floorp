/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 *   Pierre Phaneuf <pp@ludusdesign.com>
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
#include "nsFixedSizeAllocator.h"
#include "nsIContent.h"
#include "nsINodeInfo.h"
#include "nsICSSParser.h"
#include "nsIDOMElement.h"
#include "nsINameSpaceManager.h"
#include "nsIServiceManager.h"
#include "nsIURL.h"
#include "nsXULAttributes.h"
#include "nsLayoutCID.h"
#include "nsReadableUtils.h"
#include "nsContentUtils.h"
#include "nsXULAtoms.h"
#include "nsCRT.h"

static NS_DEFINE_CID(kCSSParserCID, NS_CSSPARSER_CID);


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
nsClassList::ParseClasses(nsClassList** aList, const nsAString& aClassString)
{
    static const PRUnichar kNullCh = PRUnichar('\0');

    if (*aList != nsnull) {
        delete *aList;
        *aList = nsnull;
    }

    if (!aClassString.IsEmpty()) {
        nsAutoString classStr(aClassString);  // copy to work buffer
        classStr.Append(kNullCh);  // put an extra null at the end

        PRUnichar* start = (PRUnichar*)(const PRUnichar*)classStr.get();
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

nsXULAttribute::nsXULAttribute(nsIContent* aContent,
                               nsINodeInfo* aNodeInfo,
                               const nsAString& aValue)
    : mContent(aContent),
      mNodeInfo(aNodeInfo)
{
    NS_INIT_ISUPPORTS();
    NS_IF_ADDREF(aNodeInfo);
    SetValueInternal(aValue);
}

nsXULAttribute::~nsXULAttribute()
{
    NS_IF_RELEASE(mNodeInfo);
    mValue.ReleaseValue();
}

nsresult
nsXULAttribute::Create(nsIContent* aContent,
                       nsINodeInfo* aNodeInfo,
                       const nsAString& aValue,
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

// QueryInterface implementation for nsXULAttribute
NS_INTERFACE_MAP_BEGIN(nsXULAttribute)
    NS_INTERFACE_MAP_ENTRY(nsIDOMAttr)
    NS_INTERFACE_MAP_ENTRY(nsIDOMNode)
    NS_INTERFACE_MAP_ENTRY(nsIDOM3Node)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMAttr)
    NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(XULAttr)
NS_INTERFACE_MAP_END


NS_IMPL_ADDREF(nsXULAttribute);
NS_IMPL_RELEASE(nsXULAttribute);


// nsIDOMNode interface

NS_IMETHODIMP
nsXULAttribute::GetNodeName(nsAString& aNodeName)
{
    GetQualifiedName(aNodeName);
    return NS_OK;
}

NS_IMETHODIMP
nsXULAttribute::GetNodeValue(nsAString& aNodeValue)
{
    return mValue.GetValue(aNodeValue);
}

NS_IMETHODIMP
nsXULAttribute::SetNodeValue(const nsAString& aNodeValue)
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
nsXULAttribute::GetNamespaceURI(nsAString& aNamespaceURI)
{
  return mNodeInfo->GetNamespaceURI(aNamespaceURI);
}

NS_IMETHODIMP
nsXULAttribute::GetPrefix(nsAString& aPrefix)
{
  return mNodeInfo->GetPrefix(aPrefix);
}

NS_IMETHODIMP
nsXULAttribute::SetPrefix(const nsAString& aPrefix)
{
    // XXX: Validate the prefix string!

    nsINodeInfo *newNodeInfo = nsnull;
    nsCOMPtr<nsIAtom> prefix;

    if (!aPrefix.IsEmpty()) {
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
nsXULAttribute::GetLocalName(nsAString& aLocalName)
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
nsXULAttribute::HasAttributes(PRBool* aReturn)
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
nsXULAttribute::IsSupported(const nsAString& aFeature,
                            const nsAString& aVersion,
                            PRBool* aReturn)
{
  NS_NOTYETIMPLEMENTED("write me");
  return NS_ERROR_NOT_IMPLEMENTED;
}

// nsIDOM3Node interface

NS_IMETHODIMP
nsXULAttribute::GetBaseURI(nsAString &aURI)
{
  NS_NOTYETIMPLEMENTED("write me");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsXULAttribute::CompareDocumentPosition(nsIDOMNode* aOther,
                                        PRUint16* aReturn)
{
  NS_NOTYETIMPLEMENTED("write me");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsXULAttribute::IsSameNode(nsIDOMNode* aOther,
                           PRBool* aReturn)
{
  NS_NOTYETIMPLEMENTED("write me");
  return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsXULAttribute::LookupNamespacePrefix(const nsAString& aNamespaceURI,
                                      nsAString& aPrefix)
{
  NS_NOTYETIMPLEMENTED("write me");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsXULAttribute::LookupNamespaceURI(const nsAString& aNamespacePrefix,
                                   nsAString& aNamespaceURI) 
{
  NS_NOTYETIMPLEMENTED("write me");
  return NS_ERROR_NOT_IMPLEMENTED;
}


// nsIDOMAttr interface

NS_IMETHODIMP
nsXULAttribute::GetName(nsAString& aName)
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
nsXULAttribute::GetValue(nsAString& aValue)
{
    return mValue.GetValue(aValue);
}

NS_IMETHODIMP
nsXULAttribute::SetValue(const nsAString& aValue)
{
    // We call back to the content node's SetValue() method so we can
    // share all of the work that it does.
    return mContent->SetAttr(mNodeInfo, aValue, PR_TRUE);
}

NS_IMETHODIMP
nsXULAttribute::GetOwnerElement(nsIDOMElement** aOwnerElement)
{
  NS_ENSURE_ARG_POINTER(aOwnerElement);

  return mContent->QueryInterface(NS_GET_IID(nsIDOMElement),
                                  (void **)aOwnerElement);
}


// Implementation methods

void
nsXULAttribute::GetQualifiedName(nsAString& aQualifiedName)
{
    mNodeInfo->GetQualifiedName(aQualifiedName);
}



nsresult
nsXULAttribute::SetValueInternal(const nsAString& aValue)
{
    return mValue.SetValue( aValue, mNodeInfo->Equals(nsXULAtoms::id) );
}

nsresult
nsXULAttribute::GetValueAsAtom(nsIAtom** aResult)
{
    return mValue.GetValueAsAtom( aResult );
}


//----------------------------------------------------------------------
//
// nsXULAttributes
//

nsXULAttributes::nsXULAttributes(nsIContent* aContent)
    : mContent(aContent),
      mClassList(nsnull),
      mStyleRule(nsnull)
{
    NS_INIT_ISUPPORTS();
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

static nsFixedSizeAllocator *gAttrAllocator;
static PRUint32 gRefCnt;

static PRBool
CreateAttrAllocator()
{
    gAttrAllocator = new nsFixedSizeAllocator();
    if (!gAttrAllocator)
        return PR_FALSE;
    
    const size_t bucketSize = sizeof(nsXULAttributes);
    const PRInt32 initalElements = 128; // XXX tune me
    if (NS_FAILED(gAttrAllocator->Init("XUL Attributes", &bucketSize,
                                       1, bucketSize * initalElements,
                                       8))) {
        delete gAttrAllocator;
        gAttrAllocator = nsnull;
        return PR_FALSE;
    }

    return PR_TRUE;
}

static void
DestroyAttrAllocator()
{
    NS_ASSERTION(!gRefCnt, "Premature call to DestroyAttrAllocator!");
    delete gAttrAllocator;
    gAttrAllocator = nsnull;
}

nsresult
nsXULAttributes::Create(nsIContent* aContent, nsXULAttributes** aResult)
{
    NS_PRECONDITION(aResult != nsnull, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    if (!gRefCnt && !CreateAttrAllocator())
        return NS_ERROR_OUT_OF_MEMORY;

    // Increment here, because this controls the gAttrAllocator's destruction.
    // Failure cases below must decrement gRefCnt, and then call
    // DestroyAttrAllocator if gRefCnt == 0, to avoid leaking a ref that will
    // not be balanced by a matching Destroy call. (|goto error;| works.)
    gRefCnt++;
    void *place = gAttrAllocator->Alloc(sizeof (nsXULAttributes));
    if (!place)
        goto error;

    *aResult = ::new (place) nsXULAttributes(aContent);
    if (!*aResult)
        goto error;

    NS_ADDREF(*aResult);
    return NS_OK;

error:
    if (!--gRefCnt)
        DestroyAttrAllocator();
    return NS_ERROR_OUT_OF_MEMORY;
}

/* static */ void
nsXULAttributes::Destroy(nsXULAttributes *aAttrs)
{
    aAttrs->~nsXULAttributes();
    NS_ASSERTION(gAttrAllocator,
                 "Allocator destroyed with live nsXULAttributes remaining!");
    gAttrAllocator->Free(aAttrs, sizeof *aAttrs);
    if (!--gRefCnt)
        DestroyAttrAllocator();
}

// nsISupports interface

// QueryInterface implementation for nsXULAttributes
NS_INTERFACE_MAP_BEGIN(nsXULAttributes)
    NS_INTERFACE_MAP_ENTRY(nsIDOMNamedNodeMap)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMNamedNodeMap)
    NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(XULNamedNodeMap)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsXULAttributes);

// Custom release method to go through the fixed-size allocator
nsrefcnt
nsXULAttributes::Release()
{
    --mRefCnt;
    NS_LOG_RELEASE(this, mRefCnt, "nsXULAttributes");
    if (mRefCnt == 0) {
        mRefCnt = 1;            // stabilize (necessary for this class?)
        Destroy(this);
        return 0;
    }
    return mRefCnt;
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
nsXULAttributes::GetNamedItem(const nsAString& aName,
                              nsIDOMNode** aReturn)
{
    NS_PRECONDITION(aReturn != nsnull, "null ptr");
    if (! aReturn)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;
    *aReturn = nsnull;

    // XXX nameSpaceID only used in dead code (that was giving us a warning).
    // XXX I'd remove it completely, but maybe it is a useful reminder???
    // PRInt32 nameSpaceID;
    nsCOMPtr<nsINodeInfo> inpNodeInfo;

    if (NS_FAILED(rv = mContent->NormalizeAttrString(aName, *getter_AddRefs(inpNodeInfo))))
        return rv;

    // if (kNameSpaceID_Unknown == nameSpaceID) {
    //   nameSpaceID = kNameSpaceID_None;  // ignore unknown prefix XXX is this correct?
    // }

    // XXX doing this instead of calling mContent->GetAttr() will
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

    return NS_OK;
}

NS_IMETHODIMP
nsXULAttributes::SetNamedItem(nsIDOMNode* aArg, nsIDOMNode** aReturn)
{
    NS_NOTYETIMPLEMENTED("write me");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsXULAttributes::RemoveNamedItem(const nsAString& aName,
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
nsXULAttributes::GetNamedItemNS(const nsAString& aNamespaceURI, 
                                const nsAString& aLocalName,
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
nsXULAttributes::RemoveNamedItemNS(const nsAString& aNamespaceURI, 
                                   const nsAString& aLocalName,
                                   nsIDOMNode** aReturn)
{
  NS_NOTYETIMPLEMENTED("write me");
  return NS_ERROR_NOT_IMPLEMENTED;
}


// Implementation methods

nsresult 
nsXULAttributes::GetClasses(nsVoidArray& aArray) const
{
    return nsClassList::GetClasses(mClassList, aArray);
}

PRBool 
nsXULAttributes::HasClass(nsIAtom* aClass) const
{
    return nsClassList::HasClass(mClassList, aClass);
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

nsresult nsXULAttributes::UpdateClassList(const nsAString& aValue)
{
    return nsClassList::ParseClasses(&mClassList, aValue);
}

nsresult nsXULAttributes::UpdateStyleRule(nsIURI* aDocURL, const nsAString& aValue)
{
    if (aValue.IsEmpty())
    {
      // XXX: Removing the rule. Is this sufficient?
      mStyleRule = nsnull;
      return NS_OK;
    }

    nsresult result = NS_OK;

    nsCOMPtr<nsICSSParser> css(do_CreateInstance(kCSSParserCID, &result));
    if (NS_OK != result) {
      return result;
    }

    nsCOMPtr<nsIStyleRule> rule;
    result = css->ParseStyleAttribute(aValue, aDocURL, getter_AddRefs(rule));
    
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

