/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
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
 *    Alex Fritze <alex.fritze@crocodile-clips.com> (original author)
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
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

#include "nsSVGAttributes.h"

#include "nsIContent.h"
#include "nsINodeInfo.h"
#include "nsIDOMElement.h"
#include "nsINameSpaceManager.h"
#include "nsIDocument.h"
#include "nsIBindingManager.h"
#include "nsIXBLBinding.h"
#include "nsMutationEvent.h"
#include "nsGenericElement.h"
#include "nsIDOMMutationEvent.h"
#include "nsStyleConsts.h"

////////////////////////////////////////////////////////////////////////
// nsSVGAttribute implementation

nsresult
nsSVGAttribute::Create(nsINodeInfo* aNodeInfo,
                       nsISVGValue* value,
                       nsSVGAttributeFlags flags,
                       nsSVGAttribute** aResult)
{
  NS_ENSURE_ARG_POINTER(aNodeInfo);
  NS_PRECONDITION(aResult != nsnull, "null ptr");
  if (! aResult) return NS_ERROR_NULL_POINTER;
  
  *aResult = (nsSVGAttribute*) new nsSVGAttribute(aNodeInfo, value, flags);
  if(!*aResult) return NS_ERROR_OUT_OF_MEMORY;
  
  NS_ADDREF(*aResult);

  if ((*aResult)->mValue)
    (*aResult)->mValue->AddObserver(*aResult);
  
  return NS_OK;
}

nsresult
nsSVGAttribute::Create(nsINodeInfo* aNodeInfo,
                       const nsAString& value,
                       nsSVGAttribute** aResult)
{
  nsCOMPtr<nsISVGValue> svg_value;
  NS_CreateSVGGenericStringValue(value, getter_AddRefs(svg_value));
  if(!svg_value) return NS_ERROR_OUT_OF_MEMORY;
 
  return nsSVGAttribute::Create(aNodeInfo, svg_value, 0, aResult);
}


nsSVGAttribute::nsSVGAttribute(nsINodeInfo* aNodeInfo,
                               nsISVGValue* value,
                               nsSVGAttributeFlags flags)
    : mFlags(flags),
      mOwner(0),
      mNodeInfo(aNodeInfo),
      mValue(value)
{
}

nsSVGAttribute::~nsSVGAttribute()
{
  if (mValue)
    mValue->RemoveObserver(this);
}

//----------------------------------------------------------------------
// nsISupports interface

NS_IMPL_ADDREF(nsSVGAttribute)
NS_IMPL_RELEASE(nsSVGAttribute)

NS_INTERFACE_MAP_BEGIN(nsSVGAttribute)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNode)
  NS_INTERFACE_MAP_ENTRY(nsIDOMAttr)
  NS_INTERFACE_MAP_ENTRY(nsISVGAttribute)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsISVGValueObserver)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMNode)
NS_INTERFACE_MAP_END

//----------------------------------------------------------------------
// nsIDOMNode interface

NS_IMETHODIMP
nsSVGAttribute::GetNodeName(nsAString& aNodeName)
{
  GetQualifiedName(aNodeName);
  return NS_OK;
}

NS_IMETHODIMP
nsSVGAttribute::GetNodeValue(nsAString& aNodeValue)
{
  return GetValue()->GetValueString(aNodeValue);
}

NS_IMETHODIMP
nsSVGAttribute::SetNodeValue(const nsAString& aNodeValue)
{
  return SetValue(aNodeValue);
}

NS_IMETHODIMP
nsSVGAttribute::GetNodeType(PRUint16* aNodeType)
{
  *aNodeType = (PRUint16)nsIDOMNode::ATTRIBUTE_NODE;
  return NS_OK;
}

NS_IMETHODIMP
nsSVGAttribute::GetParentNode(nsIDOMNode** aParentNode)
{
  *aParentNode = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsSVGAttribute::GetChildNodes(nsIDOMNodeList** aChildNodes)
{
  NS_NOTYETIMPLEMENTED("write me");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsSVGAttribute::GetFirstChild(nsIDOMNode** aFirstChild)
{
  NS_NOTYETIMPLEMENTED("write me");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsSVGAttribute::GetLastChild(nsIDOMNode** aLastChild)
{
  NS_NOTYETIMPLEMENTED("write me");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsSVGAttribute::GetPreviousSibling(nsIDOMNode** aPreviousSibling)
{
  *aPreviousSibling = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsSVGAttribute::GetNextSibling(nsIDOMNode** aNextSibling)
{
  *aNextSibling = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsSVGAttribute::GetAttributes(nsIDOMNamedNodeMap** aAttributes)
{
  *aAttributes = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsSVGAttribute::GetOwnerDocument(nsIDOMDocument** aOwnerDocument)
{
  NS_NOTYETIMPLEMENTED("write me");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsSVGAttribute::GetNamespaceURI(nsAString& aNamespaceURI)
{
  return mNodeInfo->GetNamespaceURI(aNamespaceURI);
}

NS_IMETHODIMP
nsSVGAttribute::GetPrefix(nsAString& aPrefix)
{
  return mNodeInfo->GetPrefix(aPrefix);
}

NS_IMETHODIMP
nsSVGAttribute::SetPrefix(const nsAString& aPrefix)
{
  // XXX: Validate the prefix string!
  
  nsINodeInfo *newNodeInfo = nsnull;
  nsCOMPtr<nsIAtom> prefix;
  
  if (!aPrefix.IsEmpty()) {
    prefix = do_GetAtom(aPrefix);
    NS_ENSURE_TRUE(prefix, NS_ERROR_OUT_OF_MEMORY);
  }
  
  nsresult rv = mNodeInfo->PrefixChanged(prefix, newNodeInfo);
  NS_ENSURE_SUCCESS(rv, rv);
  
  mNodeInfo = newNodeInfo;
  
  return NS_OK;
}

NS_IMETHODIMP
nsSVGAttribute::GetLocalName(nsAString& aLocalName)
{
  return mNodeInfo->GetLocalName(aLocalName);
}

NS_IMETHODIMP
nsSVGAttribute::InsertBefore(nsIDOMNode* aNewChild, nsIDOMNode* aRefChild, nsIDOMNode** aReturn)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSVGAttribute::ReplaceChild(nsIDOMNode* aNewChild, nsIDOMNode* aOldChild, nsIDOMNode** aReturn)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSVGAttribute::RemoveChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSVGAttribute::AppendChild(nsIDOMNode* aNewChild, nsIDOMNode** aReturn)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSVGAttribute::HasChildNodes(PRBool* aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsSVGAttribute::HasAttributes(PRBool* aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsSVGAttribute::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  NS_NOTYETIMPLEMENTED("write me");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsSVGAttribute::Normalize()
{
  return NS_OK;
}

NS_IMETHODIMP
nsSVGAttribute::IsSupported(const nsAString& aFeature,
                            const nsAString& aVersion,
                            PRBool* aReturn)
{
  NS_NOTYETIMPLEMENTED("write me");
  return NS_ERROR_NOT_IMPLEMENTED;
}

//----------------------------------------------------------------------
// nsIDOMAttr interface

NS_IMETHODIMP
nsSVGAttribute::GetName(nsAString& aName)
{
  GetQualifiedName(aName);
  return NS_OK;
}

NS_IMETHODIMP
nsSVGAttribute::GetSpecified(PRBool* aSpecified)
{
  // XXX this'll break when we make Clone() work
  *aSpecified = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
nsSVGAttribute::GetValue(nsAString& aValue)
{
  return GetValue()->GetValueString(aValue);
}

NS_IMETHODIMP
nsSVGAttribute::SetValue(const nsAString& aValue)
{
  if (mOwner) {
    return mOwner->SetAttr(mNodeInfo, aValue, PR_TRUE);
  }

  return GetValue()->SetValueString(aValue);
}

NS_IMETHODIMP
nsSVGAttribute::GetOwnerElement(nsIDOMElement** aOwnerElement)
{
  NS_ENSURE_ARG_POINTER(aOwnerElement);

  nsIContent *content;
  if (mOwner && (content = mOwner->GetContent())) {
    return CallQueryInterface(content, aOwnerElement);
  }

  return NS_ERROR_FAILURE;
}

//----------------------------------------------------------------------
// nsISVGAttribute methods

NS_IMETHODIMP
nsSVGAttribute::GetSVGValue(nsISVGValue** value)
{
  *value = mValue;
  NS_IF_ADDREF(*value);
  return NS_OK;
}


//----------------------------------------------------------------------
// nsISVGValueObserver methods

NS_IMETHODIMP
nsSVGAttribute::WillModifySVGObservable(nsISVGValue* observable)
{
  return NS_OK;
}

NS_IMETHODIMP
nsSVGAttribute::DidModifySVGObservable (nsISVGValue* observable)
{
  if (!mOwner) return NS_OK;
  
  mOwner->AttributeWasModified(this);
    
  return NS_OK;
}



//----------------------------------------------------------------------
// Implementation functions

void
nsSVGAttribute::GetQualifiedName(nsAString& aQualifiedName)const
{
  mNodeInfo->GetQualifiedName(aQualifiedName);
}


////////////////////////////////////////////////////////////////////////
// nsSVGAttributes
//

nsSVGAttributes::nsSVGAttributes(nsIContent* aContent)
    : mContent(aContent)
{
}


nsSVGAttributes::~nsSVGAttributes()
{
  ReleaseAttributes();
  ReleaseMappedAttributes();
}


nsresult
nsSVGAttributes::Create(nsIContent* aContent, nsSVGAttributes** aResult)
{
  NS_PRECONDITION(aResult != nsnull, "null ptr");
  if (! aResult) return NS_ERROR_NULL_POINTER;
  
  *aResult = new nsSVGAttributes(aContent);
  if (! *aResult) return NS_ERROR_OUT_OF_MEMORY;
  
  NS_ADDREF(*aResult);
  return NS_OK;
}

//----------------------------------------------------------------------
// nsISupports interface
NS_IMPL_ISUPPORTS1(nsSVGAttributes,nsIDOMNamedNodeMap);

//----------------------------------------------------------------------
// Implementation Helpers

void 
nsSVGAttributes::ReleaseAttributes()
{
  PRInt32 count = mAttributes.Count();
  for (PRInt32 index = 0; index < count; ++index) {
    nsSVGAttribute* attr = NS_REINTERPRET_CAST(nsSVGAttribute*, mAttributes.ElementAt(index));
    attr->mOwner = nsnull;
    NS_RELEASE(attr);
  }
  mAttributes.Clear();
}  

void 
nsSVGAttributes::ReleaseMappedAttributes()
{
  PRInt32 count = mMappedAttributes.Count();
  for (PRInt32 index = 0; index < count; ++index) {
    nsSVGAttribute* attr = NS_REINTERPRET_CAST(nsSVGAttribute*, mMappedAttributes.ElementAt(index));
    attr->mOwner = nsnull;
    NS_RELEASE(attr);
  }
  mMappedAttributes.Clear();
}  

PRBool
nsSVGAttributes::GetMappedAttribute(nsINodeInfo* aNodeInfo, nsSVGAttribute** attrib)
{
  PRInt32 count = mMappedAttributes.Count();
  PRInt32 index;
  for (index = 0; index < count; index++) {
    *attrib = (nsSVGAttribute*)mMappedAttributes.ElementAt(index);
    if ((*attrib)->GetNodeInfo()->Equals(aNodeInfo)) { // XXX is this the right test? (don't want to compare prefixes!)
      NS_ADDREF(*attrib);
      return PR_TRUE;
    }
  }
  *attrib = nsnull;
  return PR_FALSE;
}

PRBool
nsSVGAttributes::IsExplicitAttribute(nsSVGAttribute* attrib)
{
  PRInt32 count = mAttributes.Count();
  for (PRInt32 i=0; i<count; ++i) {
    if (mAttributes.ElementAt(i) == attrib)
      return PR_TRUE;
  }
  return PR_FALSE;
}

PRBool
nsSVGAttributes::IsMappedAttribute(nsSVGAttribute* attrib)
{
  PRInt32 count = mMappedAttributes.Count();
  for (PRInt32 i=0; i<count; ++i) {
    if (mMappedAttributes.ElementAt(i) == attrib)
      return PR_TRUE;
  }
  return PR_FALSE;
}


nsSVGAttribute*
nsSVGAttributes::ElementAt(PRInt32 i) const
{
  return (nsSVGAttribute*)mAttributes.ElementAt(i);
}

void
nsSVGAttributes::AppendElement(nsSVGAttribute* aElement)
{
  aElement->mOwner = this;
  NS_ADDREF(aElement);
  mAttributes.AppendElement((void*)aElement);
}

void
nsSVGAttributes::RemoveElementAt(PRInt32 aIndex)
{
  nsSVGAttribute* attrib = ElementAt(aIndex);
  NS_ASSERTION(attrib,"null attrib");
  if (!IsMappedAttribute(attrib))
    attrib->mOwner = 0;
  mAttributes.RemoveElementAt(aIndex);
  NS_RELEASE(attrib);
}


//----------------------------------------------------------------------
// interface used by the content element:

PRInt32
nsSVGAttributes::Count() const
{
  return mAttributes.Count();
}

NS_IMETHODIMP
nsSVGAttributes::GetAttr(PRInt32 aNameSpaceID, nsIAtom* aName, 
                         nsIAtom*& aPrefix,
                         nsAString& aResult)
{
  NS_ASSERTION(nsnull != aName, "must have attribute name");
  if (nsnull == aName) {
    return NS_ERROR_NULL_POINTER;
  }
  
  nsresult rv = NS_CONTENT_ATTR_NOT_THERE;
  
  PRInt32 count = Count();
  PRInt32 index;
  for (index = 0; index < count; index++) {
    nsSVGAttribute *attr = ElementAt(index);
    if ((aNameSpaceID == kNameSpaceID_Unknown ||
         attr->GetNodeInfo()->NamespaceEquals(aNameSpaceID)) &&
        (attr->GetNodeInfo()->Equals(aName))) {
      attr->GetNodeInfo()->GetPrefixAtom(aPrefix);
      attr->GetValue()->GetValueString(aResult);
      if (!aResult.IsEmpty()) {
        rv = NS_CONTENT_ATTR_HAS_VALUE;
      }
      else {
        rv = NS_CONTENT_ATTR_NO_VALUE;
      }
      break;
    }
  }
  
  if (rv == NS_CONTENT_ATTR_NOT_THERE) {
    // In other cases we already set the out param.
    // Since we are returning a success code we'd better do
    // something about the out parameters (someone may have
    // given us a non-empty string).
    aResult.Truncate();
  }
  
  return rv;
}

NS_IMETHODIMP
nsSVGAttributes::SetAttr(nsINodeInfo* aNodeInfo,
                         const nsAString& aValue,
                         PRBool aNotify)
{
  NS_ENSURE_ARG_POINTER(aNodeInfo);
  PRBool modification = PR_FALSE;
  nsAutoString oldValue;
  nsresult rv = NS_ERROR_OUT_OF_MEMORY;

  nsCOMPtr<nsIDocument> document;
  if (mContent)
    mContent->GetDocument(*getter_AddRefs(document));
  
  if (aNotify && document) {
    document->BeginUpdate();
  }

  nsSVGAttribute* attr;
  PRInt32 index;
  PRInt32 count;

  count = Count();
  for (index = 0; index < count; index++) {
    attr = ElementAt(index);
    if (attr->GetNodeInfo() == aNodeInfo) {
      attr->GetValue()->GetValueString(oldValue);
      modification = PR_TRUE;
      attr->GetValue()->SetValueString(aValue);
      rv = NS_OK;
      break;
    }
  }
  
  if (index >= count) { // didn't find it

    if (GetMappedAttribute(aNodeInfo, &attr)) {
      AppendElement(attr);
      attr->GetValue()->SetValueString(aValue);
    }
    else {
      rv = nsSVGAttribute::Create(aNodeInfo, aValue, &attr);
      NS_ENSURE_TRUE(attr, rv);
      AppendElement(attr);
    }
    attr->Release();
    rv = NS_OK;
  }

  if (document && NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIAtom> name;
    PRInt32 nameSpaceID;
    
    aNodeInfo->GetNameAtom(*getter_AddRefs(name));
    aNodeInfo->GetNamespaceID(nameSpaceID);

    nsCOMPtr<nsIBindingManager> bindingManager;
    document->GetBindingManager(getter_AddRefs(bindingManager));
    nsCOMPtr<nsIXBLBinding> binding;
    bindingManager->GetBinding(mContent, getter_AddRefs(binding));
    if (binding)
      binding->AttributeChanged(name, nameSpaceID, PR_FALSE, aNotify);

    if (nsGenericElement::HasMutationListeners(mContent, NS_EVENT_BITS_MUTATION_ATTRMODIFIED)) {
      nsCOMPtr<nsIDOMEventTarget> node(do_QueryInterface(mContent));
      nsMutationEvent mutation;
      mutation.eventStructType = NS_MUTATION_EVENT;
      mutation.message = NS_MUTATION_ATTRMODIFIED;
      mutation.mTarget = node;

      CallQueryInterface(attr,
                         NS_STATIC_CAST(nsIDOMNode**,
                                        getter_AddRefs(mutation.mRelatedNode)));
      mutation.mAttrName = name;
      if (!oldValue.IsEmpty())
        mutation.mPrevAttrValue = do_GetAtom(oldValue);
      if (!aValue.IsEmpty())
        mutation.mNewAttrValue = do_GetAtom(aValue);
      mutation.mAttrChange = modification ? nsIDOMMutationEvent::MODIFICATION :
                                             nsIDOMMutationEvent::ADDITION;
      nsEventStatus status = nsEventStatus_eIgnore;
      nsCOMPtr<nsIDOMEvent> domEvent;
      mContent->HandleDOMEvent(nsnull, &mutation, getter_AddRefs(domEvent),
                     NS_EVENT_FLAG_INIT, &status);
    }

    if (aNotify) {
      PRInt32 modHint = modification ? PRInt32(nsIDOMMutationEvent::MODIFICATION)
                                     : PRInt32(nsIDOMMutationEvent::ADDITION);
      document->AttributeChanged(mContent, nameSpaceID, name,
                                 modHint, NS_STYLE_HINT_UNKNOWN);
      document->EndUpdate();
    }
  }

  return rv;
}

NS_IMETHODIMP
nsSVGAttributes::UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aName, 
                           PRBool aNotify)
{
  NS_ASSERTION(nsnull != aName, "must have attribute name");
  if (nsnull == aName) {
    return NS_ERROR_NULL_POINTER;
  }
  
  nsresult rv = NS_OK;

  nsCOMPtr<nsIDocument> document;
  if (mContent)
    mContent->GetDocument(*getter_AddRefs(document));
  
  PRInt32 count = Count();
  PRInt32 index;
  PRBool  found = PR_FALSE;
  for (index = 0; index < count; index++) {
    nsSVGAttribute* attr = ElementAt(index);
    if ((aNameSpaceID == kNameSpaceID_Unknown ||
         attr->GetNodeInfo()->NamespaceEquals(aNameSpaceID)) &&
        attr->GetNodeInfo()->Equals(aName) &&
        !attr->IsRequired() &&
        !attr->IsFixed()) {
      if (aNotify && document) {
        document->BeginUpdate();
      }
      
      if (mContent && nsGenericElement::HasMutationListeners(mContent, NS_EVENT_BITS_MUTATION_ATTRMODIFIED)) {
        nsCOMPtr<nsIDOMEventTarget> node(do_QueryInterface(mContent));
        nsMutationEvent mutation;
        mutation.eventStructType = NS_MUTATION_EVENT;
        mutation.message = NS_MUTATION_ATTRMODIFIED;
        mutation.mTarget = node;
        
        CallQueryInterface(attr,
                           NS_STATIC_CAST(nsIDOMNode**,
                                          getter_AddRefs(mutation.mRelatedNode)));
        mutation.mAttrName = aName;
        nsAutoString str;
        attr->GetValue()->GetValueString(str);
        if (!str.IsEmpty())
          mutation.mPrevAttrValue = do_GetAtom(str);
        mutation.mAttrChange = nsIDOMMutationEvent::REMOVAL;
        
        nsEventStatus status = nsEventStatus_eIgnore;
        nsCOMPtr<nsIDOMEvent> domEvent;
        mContent->HandleDOMEvent(nsnull, &mutation, getter_AddRefs(domEvent),
                             NS_EVENT_FLAG_INIT, &status);
      }
      
      RemoveElementAt(index);
      found = PR_TRUE;
      break;
    }
  }
  
  if (NS_SUCCEEDED(rv) && found && document) {
    nsCOMPtr<nsIBindingManager> bindingManager;
    document->GetBindingManager(getter_AddRefs(bindingManager));
    nsCOMPtr<nsIXBLBinding> binding;
    bindingManager->GetBinding(mContent, getter_AddRefs(binding));
    if (binding)
      binding->AttributeChanged(aName, aNameSpaceID, PR_TRUE, aNotify);
    
    if (aNotify) {
      document->AttributeChanged(mContent, aNameSpaceID, aName,
                                 nsIDOMMutationEvent::REMOVAL,
                                 NS_STYLE_HINT_UNKNOWN);
      document->EndUpdate();
    }
  }
  
  return rv;
}

NS_IMETHODIMP_(PRBool)
nsSVGAttributes::HasAttr(PRInt32 aNameSpaceID, nsIAtom* aName) const
{
  // XXX - this should be hashed, or something
  PRInt32 count = Count();
  PRInt32 index;
  for (index = 0; index < count; index++) {
    nsSVGAttribute *attr = ElementAt(index);
    if ((aNameSpaceID == kNameSpaceID_Unknown ||
         attr->GetNodeInfo()->NamespaceEquals(aNameSpaceID)) &&
        (attr->GetNodeInfo()->Equals(aName))) {
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

NS_IMETHODIMP
nsSVGAttributes::NormalizeAttrString(const nsAString& aStr,
                                     nsINodeInfo*& aNodeInfo)
{
  PRInt32 indx, count = Count();
  for (indx = 0; indx < count; indx++) {
    nsSVGAttribute* attr = ElementAt(indx);
    if (attr->GetNodeInfo()->QualifiedNameEquals(aStr)) {
      aNodeInfo = attr->GetNodeInfo();
      NS_ADDREF(aNodeInfo);
      
      return NS_OK;
    }
  }

  NS_ASSERTION(mContent,"no owner content");
  if (!mContent) return NS_ERROR_FAILURE;

  nsCOMPtr<nsINodeInfo> contentNodeInfo;
  mContent->GetNodeInfo(*getter_AddRefs(contentNodeInfo));
  
  nsCOMPtr<nsINodeInfoManager> nimgr;
  contentNodeInfo->GetNodeInfoManager(*getter_AddRefs(nimgr));
  NS_ENSURE_TRUE(nimgr, NS_ERROR_FAILURE);
  
  return nimgr->GetNodeInfo(aStr, nsnull, kNameSpaceID_None, aNodeInfo);
}

NS_IMETHODIMP
nsSVGAttributes::GetAttrNameAt(PRInt32 aIndex,
                               PRInt32& aNameSpaceID, 
                               nsIAtom*& aName,
                               nsIAtom*& aPrefix)
{
  nsSVGAttribute* attr = ElementAt(aIndex);
  if (attr) {
    attr->GetNodeInfo()->GetNamespaceID(aNameSpaceID);
    attr->GetNodeInfo()->GetNameAtom(aName);
    attr->GetNodeInfo()->GetPrefixAtom(aPrefix);
    
    return NS_OK;
  }
  
  aNameSpaceID = kNameSpaceID_None;
  aName = nsnull;
  return NS_ERROR_ILLEGAL_VALUE;  
}

NS_IMETHODIMP
nsSVGAttributes::AddMappedSVGValue(nsIAtom* name, nsISupports* value)
{
  nsCOMPtr<nsISVGValue> svg_value = do_QueryInterface(value);
  NS_ENSURE_TRUE(svg_value, NS_ERROR_FAILURE);
  
  NS_ASSERTION(mContent,"no owner content");
  if (!mContent) return NS_ERROR_FAILURE;
  
  nsCOMPtr<nsINodeInfo> contentNodeInfo;
  mContent->GetNodeInfo(*getter_AddRefs(contentNodeInfo));
  
  nsCOMPtr<nsINodeInfoManager> nimgr;
  contentNodeInfo->GetNodeInfoManager(*getter_AddRefs(nimgr));
  NS_ENSURE_TRUE(nimgr, NS_ERROR_FAILURE);

  nsCOMPtr<nsINodeInfo> ni;
  nimgr->GetNodeInfo(name, nsnull, kNameSpaceID_None, *getter_AddRefs(ni));
  NS_ENSURE_TRUE(ni, NS_ERROR_FAILURE);

  nsSVGAttribute* attrib = nsnull;
  nsSVGAttribute::Create(ni, svg_value, NS_SVGATTRIBUTE_FLAGS_MAPPED, &attrib);
  NS_ENSURE_TRUE(attrib, NS_ERROR_FAILURE);
  attrib->mOwner = this;
  mMappedAttributes.AppendElement((void*)attrib);
  return NS_OK;
}

NS_IMETHODIMP
nsSVGAttributes::CopyAttributes(nsSVGAttributes* dest)
{
  nsresult rv;
  NS_ENSURE_TRUE(dest, NS_ERROR_FAILURE);
  PRInt32 count = Count();
  for (PRInt32 i = 0; i < count; ++i) {
    nsSVGAttribute* attr = ElementAt(i);
    nsAutoString value;
    rv = attr->GetValue()->GetValueString(value);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = dest->SetAttr(attr->GetNodeInfo(), value, PR_FALSE);
    NS_ENSURE_SUCCESS(rv,rv);
  }
  return NS_OK;
}


//----------------------------------------------------------------------
// interface used by our attributes:

void
nsSVGAttributes::AttributeWasModified(nsSVGAttribute* caller)
{
  if (!IsExplicitAttribute(caller)) {
    NS_ASSERTION(IsMappedAttribute(caller), "unknown attribute");
    AppendElement(caller);
  }
  // XXX Mutation events
}

//----------------------------------------------------------------------
// nsIDOMNamedNodeMap interface:
  
  NS_IMETHODIMP
nsSVGAttributes::GetLength(PRUint32* aLength)
{
  NS_PRECONDITION(aLength != nsnull, "null ptr");
  if (! aLength)
    return NS_ERROR_NULL_POINTER;
  
  *aLength = mAttributes.Count();
  return NS_OK;
}

NS_IMETHODIMP
nsSVGAttributes::GetNamedItem(const nsAString& aName,
                              nsIDOMNode** aReturn)
{
  NS_PRECONDITION(aReturn != nsnull, "null ptr");
  if (! aReturn)
    return NS_ERROR_NULL_POINTER;
  
  nsresult rv;
  *aReturn = nsnull;
  
  nsCOMPtr<nsINodeInfo> inpNodeInfo;
  
  if (NS_FAILED(rv = mContent->NormalizeAttrString(aName, *getter_AddRefs(inpNodeInfo))))
    return rv;
  
  for (PRInt32 i = mAttributes.Count() - 1; i >= 0; --i) {
    nsSVGAttribute* attr = (nsSVGAttribute*) mAttributes[i];
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
nsSVGAttributes::SetNamedItem(nsIDOMNode* aArg, nsIDOMNode** aReturn)
{
  NS_NOTYETIMPLEMENTED("write me");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsSVGAttributes::RemoveNamedItem(const nsAString& aName,
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
nsSVGAttributes::Item(PRUint32 aIndex, nsIDOMNode** aReturn)
{
  *aReturn = (nsSVGAttribute*) mAttributes[aIndex];
  NS_IF_ADDREF(*aReturn);
  return NS_OK;
}

nsresult
nsSVGAttributes::GetNamedItemNS(const nsAString& aNamespaceURI, 
                                const nsAString& aLocalName,
                                nsIDOMNode** aReturn)
{
  NS_NOTYETIMPLEMENTED("write me");
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
nsSVGAttributes::SetNamedItemNS(nsIDOMNode* aArg, nsIDOMNode** aReturn)
{
  NS_NOTYETIMPLEMENTED("write me");
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
nsSVGAttributes::RemoveNamedItemNS(const nsAString& aNamespaceURI, 
                                   const nsAString& aLocalName,
                                   nsIDOMNode** aReturn)
{
  NS_NOTYETIMPLEMENTED("write me");
  return NS_ERROR_NOT_IMPLEMENTED;
}



