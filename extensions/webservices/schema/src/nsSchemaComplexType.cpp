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

////////////////////////////////////////////////////////////
//
// nsSchemaComplexType implementation
//
////////////////////////////////////////////////////////////
nsSchemaComplexType::nsSchemaComplexType(nsSchema* aSchema,
                                         const nsAReadableString& aName,
                                         PRBool aAbstract)
  : nsSchemaComponentBase(aSchema), mName(aName), mAbstract(aAbstract),
    mContentModel(CONTENT_MODEL_ELEMENT_ONLY), 
    mDerivation(DERIVATION_SELF_CONTAINED)
{
  NS_INIT_ISUPPORTS();
}

nsSchemaComplexType::~nsSchemaComplexType()
{
}

NS_IMPL_ISUPPORTS3(nsSchemaComplexType, 
                   nsISchemaComponent,
                   nsISchemaType,
                   nsISchemaComplexType)

/* void resolve (); */
NS_IMETHODIMP 
nsSchemaComplexType::Resolve()
{
  if (mIsResolving) {
    return NS_OK;
  }

  mIsResolving = PR_TRUE;
  nsresult rv;
  PRUint32 i, count;

  mAttributes.Count(&count);
  for (i = 0; i < count; i++) {
    nsCOMPtr<nsISchemaAttributeComponent> attribute;
    
    rv = mAttributes.QueryElementAt(i, NS_GET_IID(nsISchemaAttributeComponent),
                                    getter_AddRefs(attribute));
    if (NS_SUCCEEDED(rv)) {
      rv = attribute->Resolve();
      if (NS_FAILED(rv)) {
        mIsResolving = PR_FALSE;
        return rv;
      }
    }
  }

  if (!mSchema) {
    mIsResolving = PR_FALSE;
    return NS_ERROR_FAILURE;
  }

  if (mBaseType) {
    rv = mSchema->ResolveTypePlaceholder(mBaseType, getter_AddRefs(mBaseType));
    if (NS_FAILED(rv)) {
      mIsResolving = PR_FALSE;
      return NS_ERROR_FAILURE;
    }
    rv = mBaseType->Resolve();
    if (NS_FAILED(rv)) {
      mIsResolving = PR_FALSE;
      return NS_ERROR_FAILURE;
    }
  }
    
  if (mSimpleBaseType) {
    nsCOMPtr<nsISchemaType> type;
    rv = mSchema->ResolveTypePlaceholder(mSimpleBaseType, 
                                         getter_AddRefs(type));
    if (NS_FAILED(rv)) {
      mIsResolving = PR_FALSE;
      return NS_ERROR_FAILURE;
    }
    mSimpleBaseType = do_QueryInterface(type);
    if (!mSimpleBaseType) {
      mIsResolving = PR_FALSE;
      return NS_ERROR_FAILURE;
    }
    rv = mSimpleBaseType->Resolve();
  }
  mIsResolving = PR_FALSE;

  return NS_OK;
}

/* void clear (); */
NS_IMETHODIMP 
nsSchemaComplexType::Clear()
{
  if (mIsClearing) {
    return NS_OK;
  }

  mIsClearing = PR_TRUE;
  if (mBaseType) {
    mBaseType->Clear();
    mBaseType = nsnull;
  }
  if (mSimpleBaseType) {
    mSimpleBaseType->Clear();
    mSimpleBaseType = nsnull;
  }
  if (mModelGroup) {
    mModelGroup->Clear();
    mModelGroup = nsnull;
  }

  nsresult rv;
  PRUint32 i, count;
  mAttributes.Count(&count);
  for (i = 0; i < count; i++) {
    nsCOMPtr<nsISchemaAttributeComponent> attribute;
    
    rv = mAttributes.QueryElementAt(i, NS_GET_IID(nsISchemaAttributeComponent),
                                    getter_AddRefs(attribute));
    if (NS_SUCCEEDED(rv)) {
      attribute->Clear();
    }
  }
  mAttributes.Clear();
  mAttributesHash.Reset();
  mIsClearing = PR_FALSE;

  return NS_OK;
}

/* readonly attribute wstring name; */
NS_IMETHODIMP 
nsSchemaComplexType::GetName(nsAWritableString& aName)
{
  aName.Assign(mName);
  
  return NS_OK;
}

/* readonly attribute unsigned short schemaType; */
NS_IMETHODIMP 
nsSchemaComplexType::GetSchemaType(PRUint16 *aSchemaType)
{
  NS_ENSURE_ARG_POINTER(aSchemaType);

  *aSchemaType = nsISchemaType::SCHEMA_TYPE_COMPLEX;

  return NS_OK;
}

/* readonly attribute unsigned short contentModel; */
NS_IMETHODIMP 
nsSchemaComplexType::GetContentModel(PRUint16 *aContentModel)
{
  NS_ENSURE_ARG_POINTER(aContentModel);
  
  *aContentModel = mContentModel;
  
  return NS_OK;
}

/* readonly attribute unsigned short derivation; */
NS_IMETHODIMP 
nsSchemaComplexType::GetDerivation(PRUint16 *aDerivation)
{
  NS_ENSURE_ARG_POINTER(aDerivation);

  *aDerivation = mDerivation;

  return NS_OK;
}

/* readonly attribute nsISchemaType baseType; */
NS_IMETHODIMP 
nsSchemaComplexType::GetBaseType(nsISchemaType * *aBaseType)
{
  NS_ENSURE_ARG_POINTER(aBaseType);

  *aBaseType = mBaseType;
  NS_IF_ADDREF(*aBaseType);

  return NS_OK;
}

/* readonly attribute nsISchemaSimpleType simplBaseType; */
NS_IMETHODIMP 
nsSchemaComplexType::GetSimpleBaseType(nsISchemaSimpleType * *aSimpleBaseType)
{
  NS_ENSURE_ARG_POINTER(aSimpleBaseType);

  *aSimpleBaseType = mSimpleBaseType;
  NS_IF_ADDREF(*aSimpleBaseType);

  return NS_OK;
}

/* readonly attribute nsISchemaModelGroup modelGroup; */
NS_IMETHODIMP 
nsSchemaComplexType::GetModelGroup(nsISchemaModelGroup * *aModelGroup)
{
  NS_ENSURE_ARG_POINTER(aModelGroup);

  *aModelGroup = mModelGroup;
  NS_IF_ADDREF(*aModelGroup);

  return NS_OK;
}

/* readonly attribute PRUint32 attributeCount; */
NS_IMETHODIMP 
nsSchemaComplexType::GetAttributeCount(PRUint32 *aAttributeCount)
{
  NS_ENSURE_ARG_POINTER(aAttributeCount);

  return mAttributes.Count(aAttributeCount);
}

/* nsISchemaAttributeComponent getAttributeByIndex (in PRUint32 index); */
NS_IMETHODIMP 
nsSchemaComplexType::GetAttributeByIndex(PRUint32 index, 
                                         nsISchemaAttributeComponent **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  return mAttributes.QueryElementAt(index, 
                                    NS_GET_IID(nsISchemaAttributeComponent),
                                    (void**)_retval);
}

/* nsISchemaAttributeComponent getAttributeByName (in AString name); */
NS_IMETHODIMP 
nsSchemaComplexType::GetAttributeByName(const nsAReadableString& name, 
                                        nsISchemaAttributeComponent **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  nsStringKey key(name);
  nsCOMPtr<nsISupports> sup = dont_AddRef(mAttributesHash.Get(&key));

  if (sup) {
    return CallQueryInterface(sup, _retval);
  }

  return NS_OK;
}

/* readonly attribute boolean abstract; */
NS_IMETHODIMP 
nsSchemaComplexType::GetAbstract(PRBool *aAbstract)
{
  NS_ENSURE_ARG_POINTER(aAbstract);

  *aAbstract = mAbstract;
  
  return NS_OK;
}

NS_IMETHODIMP
nsSchemaComplexType::SetContentModel(PRUint16 aContentModel)
{
  mContentModel = aContentModel;

  return NS_OK;
}

NS_IMETHODIMP 
nsSchemaComplexType::SetDerivation(PRUint16 aDerivation, 
                                   nsISchemaType* aBaseType)
{
  mDerivation = aDerivation;
  mBaseType = aBaseType;

  return NS_OK;
}

NS_IMETHODIMP 
nsSchemaComplexType::SetSimpleBaseType(nsISchemaSimpleType* aSimpleBaseType)
{
  mSimpleBaseType = aSimpleBaseType;

  return NS_OK;
}

NS_IMETHODIMP 
nsSchemaComplexType::SetModelGroup(nsISchemaModelGroup* aModelGroup)
{
  mModelGroup = aModelGroup;

  return NS_OK;
}

NS_IMETHODIMP 
nsSchemaComplexType::AddAttribute(nsISchemaAttributeComponent* aAttribute)
{
  NS_ENSURE_ARG_POINTER(aAttribute);

  nsAutoString name;
  aAttribute->GetName(name);

  mAttributes.AppendElement(aAttribute);
  nsStringKey key(name);
  mAttributesHash.Put(&key, aAttribute);

  return NS_OK;  
}

