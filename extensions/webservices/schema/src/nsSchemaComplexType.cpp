/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vidur Apparao <vidur@netscape.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsSchemaPrivate.h"

////////////////////////////////////////////////////////////
//
// nsSchemaComplexType implementation
//
////////////////////////////////////////////////////////////
nsSchemaComplexType::nsSchemaComplexType(nsSchema* aSchema,
                                         const nsAString& aName,
                                         PRBool aAbstract)
  : nsSchemaComponentBase(aSchema), mName(aName), mAbstract(aAbstract),
    mContentModel(CONTENT_MODEL_ELEMENT_ONLY), 
    mDerivation(DERIVATION_SELF_CONTAINED)
{
}

nsSchemaComplexType::~nsSchemaComplexType()
{
}

NS_IMPL_ISUPPORTS3_CI(nsSchemaComplexType, 
                      nsISchemaComponent,
                      nsISchemaType,
                      nsISchemaComplexType)

/* void resolve (); */
NS_IMETHODIMP
nsSchemaComplexType::Resolve()
{
  if (mIsResolved) {
    return NS_OK;
  }

  mIsResolved = PR_TRUE;
  nsresult rv;
  PRUint32 i, count;

  count = mAttributes.Count();
  for (i = 0; i < count; ++i) {
    rv = mAttributes.ObjectAt(i)->Resolve();
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  if (!mSchema) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsISchemaType> type;
  if (mBaseType) {
    rv = mSchema->ResolveTypePlaceholder(mBaseType, getter_AddRefs(type));
    if (NS_FAILED(rv)) {
      return NS_ERROR_FAILURE;
    }
    mBaseType = type;
    rv = mBaseType->Resolve();
    if (NS_FAILED(rv)) {
      return NS_ERROR_FAILURE;
    }
  }
    
  if (mSimpleBaseType) {
    rv = mSchema->ResolveTypePlaceholder(mSimpleBaseType, 
                                         getter_AddRefs(type));
    if (NS_FAILED(rv)) {
      return NS_ERROR_FAILURE;
    }
    mSimpleBaseType = do_QueryInterface(type);
    if (!mSimpleBaseType) {
      return NS_ERROR_FAILURE;
    }
    rv = mSimpleBaseType->Resolve();
    if (NS_FAILED(rv)) {
      return NS_ERROR_FAILURE;
    }
  }

  if (mModelGroup) {
    rv = mModelGroup->Resolve();
    if (NS_FAILED(rv)) {
      return NS_ERROR_FAILURE;
    }
  }

  if (mArrayInfo) {
    nsCOMPtr<nsISchemaType> placeHolder;
    mArrayInfo->GetType(getter_AddRefs(placeHolder));
    if (placeHolder) {
      PRUint16 schemaType;
      placeHolder->GetSchemaType(&schemaType);
      if (schemaType == nsISchemaType::SCHEMA_TYPE_PLACEHOLDER) {
        rv = mSchema->ResolveTypePlaceholder(placeHolder, getter_AddRefs(type));
        if (NS_FAILED(rv)) {
          return NS_ERROR_FAILURE;
        }
        rv = type->Resolve();
        if (NS_FAILED(rv)) {
          return NS_ERROR_FAILURE;
        }
        SetArrayInfo(type, mArrayInfo->GetDimension());
      }
      else {
         rv = placeHolder->Resolve();
        if (NS_FAILED(rv)) {
          return NS_ERROR_FAILURE;
        }
      }
    } 
  }

  return NS_OK;
}

/* void clear (); */
NS_IMETHODIMP
nsSchemaComplexType::Clear()
{
  if (mIsCleared) {
    return NS_OK;
  }

  mIsCleared = PR_TRUE;
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

  PRUint32 i, count;
  count = mAttributes.Count();
  for (i = 0; i < count; ++i) {
    mAttributes.ObjectAt(i)->Clear();
  }
  mAttributes.Clear();
  mAttributesHash.Clear();

  return NS_OK;
}

/* readonly attribute wstring name; */
NS_IMETHODIMP
nsSchemaComplexType::GetName(nsAString& aName)
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

  NS_IF_ADDREF(*aBaseType = mBaseType);

  return NS_OK;
}

/* readonly attribute nsISchemaSimpleType simplBaseType; */
NS_IMETHODIMP
nsSchemaComplexType::GetSimpleBaseType(nsISchemaSimpleType * *aSimpleBaseType)
{
  NS_ENSURE_ARG_POINTER(aSimpleBaseType);

  NS_IF_ADDREF(*aSimpleBaseType = mSimpleBaseType);

  return NS_OK;
}

/* readonly attribute nsISchemaModelGroup modelGroup; */
NS_IMETHODIMP
nsSchemaComplexType::GetModelGroup(nsISchemaModelGroup * *aModelGroup)
{
  NS_ENSURE_ARG_POINTER(aModelGroup);

  NS_IF_ADDREF(*aModelGroup = mModelGroup);

  return NS_OK;
}

/* readonly attribute PRUint32 attributeCount; */
NS_IMETHODIMP
nsSchemaComplexType::GetAttributeCount(PRUint32 *aAttributeCount)
{
  NS_ENSURE_ARG_POINTER(aAttributeCount);

  *aAttributeCount = mAttributes.Count();

  return NS_OK;
}

/* nsISchemaAttributeComponent getAttributeByIndex (in PRUint32 index); */
NS_IMETHODIMP
nsSchemaComplexType::GetAttributeByIndex(PRUint32 aIndex, 
                                         nsISchemaAttributeComponent** aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

  if (aIndex >= (PRUint32)mAttributes.Count()) {
    return NS_ERROR_FAILURE;
  }

  NS_ADDREF(*aResult = mAttributes.ObjectAt(aIndex));

  return NS_OK;
}

/* nsISchemaAttributeComponent getAttributeByName (in AString name); */
NS_IMETHODIMP
nsSchemaComplexType::GetAttributeByName(const nsAString& aName, 
                                        nsISchemaAttributeComponent** aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

  mAttributesHash.Get(aName, aResult);

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

/* readonly attribute boolean isArray; */
NS_IMETHODIMP
nsSchemaComplexType::GetIsArray(PRBool* aIsArray) 
{
  NS_ENSURE_ARG_POINTER(aIsArray);

  nsCOMPtr<nsISchemaComplexType> complexBase = do_QueryInterface(mBaseType);
  if (complexBase) {
    return complexBase->GetIsArray(aIsArray);
  }

  *aIsArray = PR_FALSE;

  return NS_OK;
}

/* readonly attribute nsISchemaType arrayType; */
NS_IMETHODIMP
nsSchemaComplexType::GetArrayType(nsISchemaType** aArrayType)
{
  NS_ENSURE_ARG_POINTER(aArrayType);

  *aArrayType = nsnull;
  if (mArrayInfo) {
    mArrayInfo->GetType(aArrayType);
  }
  else {
    nsCOMPtr<nsISchemaComplexType> complexBase = do_QueryInterface(mBaseType);
    if (complexBase) {
      return complexBase->GetArrayType(aArrayType);
    }
  }

  return NS_OK;
}

/* readonly attribute PRUint32 arrayDimension; */
NS_IMETHODIMP
nsSchemaComplexType::GetArrayDimension(PRUint32* aDimension)
{
  NS_ENSURE_ARG_POINTER(aDimension);

  *aDimension = 0;
  if (mArrayInfo) {
    *aDimension = mArrayInfo->GetDimension();
  }
  else {
    nsCOMPtr<nsISchemaComplexType> complexBase = do_QueryInterface(mBaseType);
    if (complexBase) {
      return complexBase->GetArrayDimension(aDimension);
    }
  }

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

  mAttributes.AppendObject(aAttribute);
  mAttributesHash.Put(name, aAttribute);

  return NS_OK;  
}

NS_IMETHODIMP
nsSchemaComplexType::SetArrayInfo(nsISchemaType* aType, PRUint32 aDimension)
{
  mArrayInfo = new nsComplexTypeArrayInfo(aType, aDimension);

  return mArrayInfo ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}
