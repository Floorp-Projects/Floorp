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
      elementFormDefault.EqualsLiteral("qualified");
  }
}

nsSchema::~nsSchema()
{
  Clear();
}

NS_IMPL_ISUPPORTS2_CI(nsSchema, nsISchema, nsISchemaComponent)

nsresult
nsSchema::Init()
{
  PRBool ok = mTypesHash.Init();
  NS_ENSURE_TRUE(ok, NS_ERROR_FAILURE);

  ok = mAttributesHash.Init();
  NS_ENSURE_TRUE(ok, NS_ERROR_FAILURE);

  ok = mElementsHash.Init();
  NS_ENSURE_TRUE(ok, NS_ERROR_FAILURE);

  ok = mAttributeGroupsHash.Init();
  NS_ENSURE_TRUE(ok, NS_ERROR_FAILURE);

  return mModelGroupsHash.Init() ? NS_OK : NS_ERROR_FAILURE;
}

/* readonly attribute wstring targetNamespace; */
NS_IMETHODIMP
nsSchema::GetTargetNamespace(nsAString& aTargetNamespace)
{
  aTargetNamespace.Assign(mTargetNamespace);
  return NS_OK;
}

NS_IMETHODIMP
nsSchema::GetSchemaNamespace(nsAString& aSchemaNamespace)
{
  aSchemaNamespace.Assign(mSchemaNamespace);
  return NS_OK;
}

NS_IMETHODIMP
nsSchema::Resolve()
{
  nsresult rv;
  PRUint32 i, count;

  count = mTypes.Count();
  for (i = 0; i < count; ++i) {
    rv = mTypes.ObjectAt(i)->Resolve();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  count = mAttributes.Count();
  for (i = 0; i < count; ++i) {
    rv = mAttributes.ObjectAt(i)->Resolve();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  count = mElements.Count();
  for (i = 0; i < count; ++i) {
    rv = mElements.ObjectAt(i)->Resolve();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  count = mAttributeGroups.Count();
  for (i = 0; i < count; ++i) {
    rv = mAttributeGroups.ObjectAt(i)->Resolve();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  count = mModelGroups.Count();
  for (i = 0; i < count; ++i) {
    rv = mModelGroups.ObjectAt(i)->Resolve();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsSchema::Clear()
{
  PRUint32 i, count;

  count = mTypes.Count();
  for (i = 0; i < count; ++i) {
    mTypes.ObjectAt(i)->Clear();
  }
  mTypes.Clear();
  mTypesHash.Clear();

  count = mAttributes.Count();
  for (i = 0; i < count; ++i) {
    mAttributes.ObjectAt(i)->Clear();
  }
  mAttributes.Clear();
  mAttributesHash.Clear();

  count = mElements.Count();
  for (i = 0; i < count; ++i) {
    mElements.ObjectAt(i)->Clear();
  }
  mElements.Clear();
  mElementsHash.Clear();

  count = mAttributeGroups.Count();
  for (i = 0; i < count; ++i) {
    mAttributeGroups.ObjectAt(i)->Clear();
  }
  mAttributeGroups.Clear();
  mAttributeGroupsHash.Clear();

  count = mModelGroups.Count();
  for (i = 0; i < count; ++i) {
    mModelGroups.ObjectAt(i)->Clear();
  }
  mModelGroups.Clear();
  mModelGroupsHash.Clear();

  return NS_OK;
}

NS_IMETHODIMP
nsSchema::GetTypeCount(PRUint32 *aTypeCount)
{
  NS_ENSURE_ARG_POINTER(aTypeCount);

  *aTypeCount = mTypes.Count();

  return NS_OK;
}

NS_IMETHODIMP
nsSchema::GetTypeByIndex(PRUint32 aIndex, nsISchemaType** aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

  if (aIndex >= (PRUint32)mTypes.Count()) {
    return NS_ERROR_FAILURE;
  }

  NS_ADDREF(*aResult = mTypes.ObjectAt(aIndex));

  return NS_OK;
}

NS_IMETHODIMP
nsSchema::GetTypeByName(const nsAString& aName, nsISchemaType** aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

  mTypesHash.Get(aName, aResult);

  return NS_OK;
}

NS_IMETHODIMP
nsSchema::GetAttributeCount(PRUint32 *aAttributeCount)
{
  NS_ENSURE_ARG_POINTER(aAttributeCount);

  *aAttributeCount = mAttributes.Count();

  return NS_OK;
}

NS_IMETHODIMP
nsSchema::GetAttributeByIndex(PRUint32 aIndex, nsISchemaAttribute** aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

  if (aIndex >= (PRUint32)mAttributes.Count()) {
    return NS_ERROR_FAILURE;
  }

  NS_ADDREF(*aResult = mAttributes.ObjectAt(aIndex));

  return NS_OK;
}

NS_IMETHODIMP
nsSchema::GetAttributeByName(const nsAString& aName,
                             nsISchemaAttribute** aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

  mAttributesHash.Get(aName, aResult);

  return NS_OK;
}

NS_IMETHODIMP
nsSchema::GetElementCount(PRUint32 *aElementCount)
{
  NS_ENSURE_ARG_POINTER(aElementCount);

  *aElementCount = mElements.Count();

  return NS_OK;
}

NS_IMETHODIMP
nsSchema::GetElementByIndex(PRUint32 aIndex, nsISchemaElement** aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

  if (aIndex >= (PRUint32)mElements.Count()) {
    return NS_ERROR_FAILURE;
  }

  NS_ADDREF(*aResult = mElements.ObjectAt(aIndex));

  return NS_OK;
}

NS_IMETHODIMP
nsSchema::GetElementByName(const nsAString& aName, nsISchemaElement** aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

  mElementsHash.Get(aName, aResult);

  return NS_OK;
}

NS_IMETHODIMP
nsSchema::GetAttributeGroupCount(PRUint32 *aAttributeGroupCount)
{
  NS_ENSURE_ARG_POINTER(aAttributeGroupCount);

  *aAttributeGroupCount = mAttributeGroups.Count();

  return NS_OK;
}

NS_IMETHODIMP
nsSchema::GetAttributeGroupByIndex(PRUint32 aIndex,
                                   nsISchemaAttributeGroup** aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

  if (aIndex >= (PRUint32)mAttributeGroups.Count()) {
    return NS_ERROR_FAILURE;
  }

  NS_ADDREF(*aResult = mAttributeGroups.ObjectAt(aIndex));

  return NS_OK;
}

NS_IMETHODIMP
nsSchema::GetAttributeGroupByName(const nsAString& aName,
                                  nsISchemaAttributeGroup** aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

  mAttributeGroupsHash.Get(aName, aResult);

  return NS_OK;
}

NS_IMETHODIMP
nsSchema::GetModelGroupCount(PRUint32 *aModelGroupCount)
{
  NS_ENSURE_ARG_POINTER(aModelGroupCount);

  *aModelGroupCount = mModelGroups.Count();

  return NS_OK;
}

NS_IMETHODIMP
nsSchema::GetModelGroupByIndex(PRUint32 aIndex, nsISchemaModelGroup** aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

  if (aIndex >= (PRUint32)mModelGroups.Count()) {
    return NS_ERROR_FAILURE;
  }

  NS_ADDREF(*aResult = mModelGroups.ObjectAt(aIndex));

  return NS_OK;
}

NS_IMETHODIMP
nsSchema::GetModelGroupByName(const nsAString& aName,
                              nsISchemaModelGroup** aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

  mModelGroupsHash.Get(aName, aResult);

  return NS_OK;
}

NS_IMETHODIMP
nsSchema::GetCollection(nsISchemaCollection** aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

  NS_IF_ADDREF(*aResult = mCollection);

  return NS_OK;
}

NS_IMETHODIMP
nsSchema::AddType(nsISchemaType* aType)
{
  NS_ENSURE_ARG_POINTER(aType);

  nsAutoString name;
  aType->GetName(name);

  mTypes.AppendObject(aType);
  mTypesHash.Put(name, aType);

  return NS_OK;
}

NS_IMETHODIMP
nsSchema::AddAttribute(nsISchemaAttribute* aAttribute)
{
  NS_ENSURE_ARG_POINTER(aAttribute);

  nsAutoString name;
  aAttribute->GetName(name);

  mAttributes.AppendObject(aAttribute);
  mAttributesHash.Put(name, aAttribute);

  return NS_OK;
}

NS_IMETHODIMP
nsSchema::AddElement(nsISchemaElement* aElement)
{
  NS_ENSURE_ARG_POINTER(aElement);

  nsAutoString name;
  aElement->GetName(name);

  mElements.AppendObject(aElement);
  mElementsHash.Put(name, aElement);

  return NS_OK;
}

NS_IMETHODIMP
nsSchema::AddAttributeGroup(nsISchemaAttributeGroup* aAttributeGroup)
{
  NS_ENSURE_ARG_POINTER(aAttributeGroup);

  nsAutoString name;
  aAttributeGroup->GetName(name);

  mAttributeGroups.AppendObject(aAttributeGroup);
  mAttributeGroupsHash.Put(name, aAttributeGroup);

  return NS_OK;
}

NS_IMETHODIMP
nsSchema::AddModelGroup(nsISchemaModelGroup* aModelGroup)
{
  NS_ENSURE_ARG_POINTER(aModelGroup);

  nsAutoString name;
  aModelGroup->GetName(name);

  mModelGroups.AppendObject(aModelGroup);
  mModelGroupsHash.Put(name, aModelGroup);

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

  aPlaceholder->GetSchemaType(&schemaType);
  if (schemaType == nsISchemaType::SCHEMA_TYPE_PLACEHOLDER) {
    nsAutoString name;
    aPlaceholder->GetName(name);
    
    nsresult rv = GetTypeByName(name, aType);
    if (NS_FAILED(rv) || !*aType) {
      *aType = nsnull;

      return NS_ERROR_FAILURE;
    }
  }
  else {
    NS_ADDREF(*aType = aPlaceholder);
  }

  return NS_OK;
}
