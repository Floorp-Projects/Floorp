/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is Mozilla.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications.  Portions created by Netscape Communications are
 * Copyright (C) 2001 by Netscape Communications.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Vidur Apparao <vidur@netscape.com> (original author)
 */

#include "nsSchemaPrivate.h"
#include "nsReadableUtils.h"

////////////////////////////////////////////////////////////
//
// nsSchema implementation
//
////////////////////////////////////////////////////////////
nsSchema::nsSchema(nsISchemaCollection* aCollection,
                   nsIDOMElement* aSchemaElement) 
{
  mCollection = aCollection;  // Weak reference
  
  if (aSchemaElement) {
    const nsAFlatString& empty = EmptyString();

    aSchemaElement->GetAttributeNS(empty, 
                                   NS_LITERAL_STRING("targetNamespace"), 
                                   mTargetNamespace);
    mTargetNamespace.Trim(" \r\n\t");
    aSchemaElement->GetNamespaceURI(mSchemaNamespace);

    nsAutoString elementFormDefault;
    aSchemaElement->GetAttributeNS(empty, 
                                   NS_LITERAL_STRING("elementFormDefault"), 
                                   elementFormDefault);
    elementFormDefault.Trim(" \r\n\t");
    mElementFormQualified = 
      elementFormDefault.Equals(NS_LITERAL_STRING("qualified"));
  }
}

nsSchema::~nsSchema()
{
  Clear();
}

NS_IMPL_ISUPPORTS2_CI(nsSchema, nsISchema, nsISchemaComponent)

/* readonly attribute wstring targetNamespace; */
NS_IMETHODIMP 
nsSchema::GetTargetNamespace(nsAString& aTargetNamespace)
{
  aTargetNamespace.Assign(mTargetNamespace);
  return NS_OK;
}

/* readonly attribute wstring schemaNamespace; */
NS_IMETHODIMP 
nsSchema::GetSchemaNamespace(nsAString& aSchemaNamespace)
{
  aSchemaNamespace.Assign(mSchemaNamespace);
  return NS_OK;
}

/* void resolve (); */
NS_IMETHODIMP 
nsSchema::Resolve()
{
  nsresult rv;
  PRUint32 i, count;

  mTypes.Count(&count);
  for (i = 0; i < count; i++) {
    nsCOMPtr<nsISchemaType> type;
    
    rv = mTypes.QueryElementAt(i, NS_GET_IID(nsISchemaType),
                               getter_AddRefs(type));
    if (NS_SUCCEEDED(rv)) {
      rv = type->Resolve();
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  mAttributes.Count(&count);
  for (i = 0; i < count; i++) {
    nsCOMPtr<nsISchemaAttribute> attribute;
    
    rv = mAttributes.QueryElementAt(i, NS_GET_IID(nsISchemaAttribute),
                                    getter_AddRefs(attribute));
    if (NS_SUCCEEDED(rv)) {
      rv = attribute->Resolve();
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  mElements.Count(&count);
  for (i = 0; i < count; i++) {
    nsCOMPtr<nsISchemaElement> element;
    
    rv = mElements.QueryElementAt(i, NS_GET_IID(nsISchemaElement),
                                  getter_AddRefs(element));
    if (NS_SUCCEEDED(rv)) {
      rv = element->Resolve();
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }
  
  mAttributeGroups.Count(&count);
  for (i = 0; i < count; i++) {
    nsCOMPtr<nsISchemaAttributeGroup> attributeGroup;
    
    rv = mAttributeGroups.QueryElementAt(i, NS_GET_IID(nsISchemaAttributeGroup),
                                         getter_AddRefs(attributeGroup));
    if (NS_SUCCEEDED(rv)) {
      rv = attributeGroup->Resolve();
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  mModelGroups.Count(&count);
  for (i = 0; i < count; i++) {
    nsCOMPtr<nsISchemaModelGroup> modelGroup;
    
    rv = mModelGroups.QueryElementAt(i, NS_GET_IID(nsISchemaModelGroup),
                                     getter_AddRefs(modelGroup));
    if (NS_SUCCEEDED(rv)) {
      rv = modelGroup->Resolve();
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  return NS_OK;
}

/* void clear (); */
NS_IMETHODIMP 
nsSchema::Clear()
{
  nsresult rv;
  PRUint32 i, count;

  mTypes.Count(&count);
  for (i = 0; i < count; i++) {
    nsCOMPtr<nsISchemaType> type;
    
    rv = mTypes.QueryElementAt(i, NS_GET_IID(nsISchemaType),
                               getter_AddRefs(type));
    if (NS_SUCCEEDED(rv)) {
      type->Clear();
    }
  }
  mTypes.Clear();
  mTypesHash.Reset();

  mAttributes.Count(&count);
  for (i = 0; i < count; i++) {
    nsCOMPtr<nsISchemaAttribute> attribute;
    
    rv = mAttributes.QueryElementAt(i, NS_GET_IID(nsISchemaAttribute),
                                    getter_AddRefs(attribute));
    if (NS_SUCCEEDED(rv)) {
      attribute->Clear();
    }
  }
  mAttributes.Clear();
  mAttributesHash.Reset();

  mElements.Count(&count);
  for (i = 0; i < count; i++) {
    nsCOMPtr<nsISchemaElement> element;
    
    rv = mElements.QueryElementAt(i, NS_GET_IID(nsISchemaElement),
                                  getter_AddRefs(element));
    if (NS_SUCCEEDED(rv)) {
      element->Clear();
    }
  }
  mElements.Clear();
  mElementsHash.Reset();
  
  mAttributeGroups.Count(&count);
  for (i = 0; i < count; i++) {
    nsCOMPtr<nsISchemaAttributeGroup> attributeGroup;
    
    rv = mAttributeGroups.QueryElementAt(i, NS_GET_IID(nsISchemaAttributeGroup),
                                         getter_AddRefs(attributeGroup));
    if (NS_SUCCEEDED(rv)) {
      attributeGroup->Clear();
    }
  }
  mAttributeGroups.Clear();
  mAttributeGroupsHash.Reset();

  mModelGroups.Count(&count);
  for (i = 0; i < count; i++) {
    nsCOMPtr<nsISchemaModelGroup> modelGroup;
    
    rv = mModelGroups.QueryElementAt(i, NS_GET_IID(nsISchemaModelGroup),
                                     getter_AddRefs(modelGroup));
    if (NS_SUCCEEDED(rv)) {
      modelGroup->Clear();
    }
  }
  mModelGroups.Clear();
  mModelGroupsHash.Reset();

  return NS_OK;
}

/* readonly attribute PRUint32 typeCount; */
NS_IMETHODIMP 
nsSchema::GetTypeCount(PRUint32 *aTypeCount)
{
  NS_ENSURE_ARG_POINTER(aTypeCount);
  
  return mTypes.Count(aTypeCount);
}

/* nsISchemaType getTypeByIndex (in PRUint32 index); */
NS_IMETHODIMP 
nsSchema::GetTypeByIndex(PRUint32 index, nsISchemaType **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  return mTypes.QueryElementAt(index, NS_GET_IID(nsISchemaType),
                               (void**)_retval);
}

/* nsISchemaType getTypeByName (in wstring name); */
NS_IMETHODIMP 
nsSchema::GetTypeByName(const nsAString& name, nsISchemaType **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  nsStringKey key(name);
  nsCOMPtr<nsISupports> sup = dont_AddRef(mTypesHash.Get(&key));

  if (sup) {
    return CallQueryInterface(sup, _retval);
  }

  return NS_OK;
}

/* readonly attribute PRUint32 attributeCount; */
NS_IMETHODIMP 
nsSchema::GetAttributeCount(PRUint32 *aAttributeCount)
{
  NS_ENSURE_ARG_POINTER(aAttributeCount);
  
  return mAttributes.Count(aAttributeCount);
}

/* nsISchemaAttribute getAttributeByIndex (in PRUint32 index); */
NS_IMETHODIMP 
nsSchema::GetAttributeByIndex(PRUint32 index, nsISchemaAttribute **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  return mAttributes.QueryElementAt(index, NS_GET_IID(nsISchemaAttribute),
                                    (void**)_retval);
}

/* nsISchemaAttribute getAttributeByName (in wstring name); */
NS_IMETHODIMP 
nsSchema::GetAttributeByName(const nsAString& name, nsISchemaAttribute **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  nsStringKey key(name);
  nsCOMPtr<nsISupports> sup = dont_AddRef(mAttributesHash.Get(&key));

  if (sup) {
    return CallQueryInterface(sup, _retval);
  }

  return NS_OK;
}

/* readonly attribute PRUint32 elementCount; */
NS_IMETHODIMP 
nsSchema::GetElementCount(PRUint32 *aElementCount)
{
  NS_ENSURE_ARG_POINTER(aElementCount);
  
  return mElements.Count(aElementCount);
}

/* nsISchemaElement getElementByIndex (in PRUint32 index); */
NS_IMETHODIMP 
nsSchema::GetElementByIndex(PRUint32 index, nsISchemaElement **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  return mElements.QueryElementAt(index, NS_GET_IID(nsISchemaElement),
                                  (void**)_retval);
}

/* nsISchemaElement getElementByName (in wstring name); */
NS_IMETHODIMP 
nsSchema::GetElementByName(const nsAString& name, 
                           nsISchemaElement **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  nsStringKey key(name);
  nsCOMPtr<nsISupports> sup = dont_AddRef(mElementsHash.Get(&key));

  if (sup) {
    return CallQueryInterface(sup, _retval);
  }

  return NS_OK;
}

/* readonly attribute PRUint32 attributeGroupCount; */
NS_IMETHODIMP 
nsSchema::GetAttributeGroupCount(PRUint32 *aAttributeGroupCount)
{
  NS_ENSURE_ARG_POINTER(aAttributeGroupCount);
  
  return mAttributeGroups.Count(aAttributeGroupCount);
}

/* nsISchemaAttributeGroup getAttributeGroupByIndex (in PRUint32 index); */
NS_IMETHODIMP 
nsSchema::GetAttributeGroupByIndex(PRUint32 index, nsISchemaAttributeGroup **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  return mAttributeGroups.QueryElementAt(index, 
                                         NS_GET_IID(nsISchemaAttributeGroup),
                                         (void**)_retval);
}

/* nsISchemaAttributeGroup getAttributeGroupByName (in wstring name); */
NS_IMETHODIMP 
nsSchema::GetAttributeGroupByName(const nsAString& name, nsISchemaAttributeGroup **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  nsStringKey key(name);
  nsCOMPtr<nsISupports> sup = dont_AddRef(mAttributeGroupsHash.Get(&key));

  if (sup) {
    return CallQueryInterface(sup, _retval);
  }

  return NS_OK;
}

/* readonly attribute PRUint32 modelGroupCount; */
NS_IMETHODIMP 
nsSchema::GetModelGroupCount(PRUint32 *aModelGroupCount)
{
  NS_ENSURE_ARG_POINTER(aModelGroupCount);
  
  return mModelGroups.Count(aModelGroupCount);
}

/* nsISchemaModelGroup getModelGroupByIndex (in PRUint32 index); */
NS_IMETHODIMP 
nsSchema::GetModelGroupByIndex(PRUint32 index, nsISchemaModelGroup **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  return mModelGroups.QueryElementAt(index, 
                                     NS_GET_IID(nsISchemaModelGroup),
                                     (void**)_retval);
}

/* nsISchemaModelGroup getModelGroupByName (in wstring name); */
NS_IMETHODIMP 
nsSchema::GetModelGroupByName(const nsAString& name, nsISchemaModelGroup **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  nsStringKey key(name);
  nsCOMPtr<nsISupports> sup = dont_AddRef(mModelGroupsHash.Get(&key));

  if (sup) {
    return CallQueryInterface(sup, _retval);
  }

  return NS_OK;
}

/* readonly attribute nsISchemaCollection collection; */
NS_IMETHODIMP
nsSchema::GetCollection(nsISchemaCollection** _retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  *_retval = mCollection;
  NS_IF_ADDREF(*_retval);

  return NS_OK;
}

NS_IMETHODIMP
nsSchema::AddType(nsISchemaType* aType)
{
  NS_ENSURE_ARG_POINTER(aType);

  nsAutoString name;
  aType->GetName(name);

  mTypes.AppendElement(aType);
  nsStringKey key(name);
  mTypesHash.Put(&key, aType);

  return NS_OK;
}

NS_IMETHODIMP 
nsSchema::AddAttribute(nsISchemaAttribute* aAttribute)
{
  NS_ENSURE_ARG_POINTER(aAttribute);

  nsAutoString name;
  aAttribute->GetName(name);

  mAttributes.AppendElement(aAttribute);
  nsStringKey key(name);
  mAttributesHash.Put(&key, aAttribute);

  return NS_OK;
}

NS_IMETHODIMP 
nsSchema::AddElement(nsISchemaElement* aElement)
{
  NS_ENSURE_ARG_POINTER(aElement);

  nsAutoString name;
  aElement->GetName(name);

  mElements.AppendElement(aElement);
  nsStringKey key(name);
  mElementsHash.Put(&key, aElement);

  return NS_OK;
}

NS_IMETHODIMP 
nsSchema::AddAttributeGroup(nsISchemaAttributeGroup* aAttributeGroup)
{
  NS_ENSURE_ARG_POINTER(aAttributeGroup);

  nsAutoString name;
  aAttributeGroup->GetName(name);

  mAttributeGroups.AppendElement(aAttributeGroup);
  nsStringKey key(name);
  mAttributeGroupsHash.Put(&key, aAttributeGroup);

  return NS_OK;
}

NS_IMETHODIMP 
nsSchema::AddModelGroup(nsISchemaModelGroup* aModelGroup)
{
  NS_ENSURE_ARG_POINTER(aModelGroup);

  nsAutoString name;
  aModelGroup->GetName(name);

  mModelGroups.AppendElement(aModelGroup);
  nsStringKey key(name);
  mModelGroupsHash.Put(&key, aModelGroup);

  return NS_OK;
}

void 
nsSchema::DropCollectionReference()
{
  mCollection = nsnull;
}

nsresult
nsSchema::ResolveTypePlaceholder(nsISchemaType* aPlaceholder,
                                 nsISchemaType** aType)
{
  PRUint16 schemaType;

  *aType = nsnull;
  aPlaceholder->GetSchemaType(&schemaType);
  if (schemaType == nsISchemaType::SCHEMA_TYPE_PLACEHOLDER) {
    nsAutoString name;
    aPlaceholder->GetName(name);
    
    nsresult rv = GetTypeByName(name, aType);
    if (NS_FAILED(rv) || !*aType) {
      return NS_ERROR_FAILURE;
    }
  }
  else {
    *aType = aPlaceholder;
    NS_ADDREF(*aType);
  }

  return NS_OK;
}

