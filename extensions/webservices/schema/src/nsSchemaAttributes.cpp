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
// nsSchemaAttribute implementation
//
////////////////////////////////////////////////////////////
nsSchemaAttribute::nsSchemaAttribute(nsSchema* aSchema, 
                                     const nsAString& aName)
  : nsSchemaComponentBase(aSchema), mName(aName)
{
}

nsSchemaAttribute::~nsSchemaAttribute()
{
}

NS_IMPL_ISUPPORTS3_CI(nsSchemaAttribute,
                      nsISchemaComponent,
                      nsISchemaAttributeComponent,
                      nsISchemaAttribute)


/* void resolve (); */
NS_IMETHODIMP
nsSchemaAttribute::Resolve()
{
  if (mIsResolved) {
    return NS_OK;
  }

  mIsResolved = PR_TRUE;
  nsresult rv = NS_OK;
  if (mType && mSchema) {
    nsCOMPtr<nsISchemaType> type;
    rv = mSchema->ResolveTypePlaceholder(mType, getter_AddRefs(type));
    if (NS_FAILED(rv)) {
      return NS_ERROR_FAILURE;
    }
    mType = do_QueryInterface(type);
    if (!mType) {
      return NS_ERROR_FAILURE;
    }
    rv = mType->Resolve();
  }

  return rv;
}

/* void clear (); */
NS_IMETHODIMP
nsSchemaAttribute::Clear()
{
  if (mIsCleared) {
    return NS_OK;
  }

  mIsCleared = PR_TRUE;
  if (mType) {
    mType->Clear();
    mType = nsnull;
  }

  return NS_OK;
}

/* readonly attribute AString name; */
NS_IMETHODIMP
nsSchemaAttribute::GetName(nsAString & aName)
{
  aName.Assign(mName);

  return NS_OK;
}

/* readonly attribute unsigned short componentType; */
NS_IMETHODIMP
nsSchemaAttribute::GetComponentType(PRUint16 *aComponentType)
{
  NS_ENSURE_ARG_POINTER(aComponentType);

  *aComponentType = nsISchemaAttributeComponent::COMPONENT_TYPE_ATTRIBUTE;

  return NS_OK;
}

/* readonly attribute nsISchemaSimpleType type; */
NS_IMETHODIMP
nsSchemaAttribute::GetType(nsISchemaSimpleType** aType)
{
  NS_ENSURE_ARG_POINTER(aType);

  NS_IF_ADDREF(*aType = mType);

  return NS_OK;
}

/* readonly attribute AString defaultValue; */
NS_IMETHODIMP
nsSchemaAttribute::GetDefaultValue(nsAString & aDefaultValue)
{
  aDefaultValue.Assign(mDefaultValue);
  
  return NS_OK;
}

/* readonly attribute AString fixedValue; */
NS_IMETHODIMP
nsSchemaAttribute::GetFixedValue(nsAString & aFixedValue)
{
  aFixedValue.Assign(mFixedValue);
  
  return NS_OK;
}

/* readonly attribute unsigned short use; */
NS_IMETHODIMP
nsSchemaAttribute::GetUse(PRUint16 *aUse)
{
  NS_ENSURE_ARG_POINTER(aUse);

  *aUse = mUse;

  return NS_OK;
}

NS_IMETHODIMP
nsSchemaAttribute::SetType(nsISchemaSimpleType* aType)
{
  NS_ENSURE_ARG(aType);
  
  mType = aType;

  return NS_OK;
}

NS_IMETHODIMP
nsSchemaAttribute::SetConstraints(const nsAString& aDefaultValue,
                                  const nsAString& aFixedValue)
{
  mDefaultValue.Assign(aDefaultValue);
  mFixedValue.Assign(aFixedValue);

  return NS_OK;
}
 
NS_IMETHODIMP
nsSchemaAttribute::SetUse(PRUint16 aUse)
{
  mUse = aUse;

  return NS_OK;
}

////////////////////////////////////////////////////////////
//
// nsSchemaAttributeRef implementation
//
////////////////////////////////////////////////////////////
nsSchemaAttributeRef::nsSchemaAttributeRef(nsSchema* aSchema, 
                                           const nsAString& aRef)
  : nsSchemaComponentBase(aSchema), mRef(aRef)
{
}

nsSchemaAttributeRef::~nsSchemaAttributeRef()
{
}

NS_IMPL_ISUPPORTS3_CI(nsSchemaAttributeRef,
                      nsISchemaComponent,
                      nsISchemaAttributeComponent,
                      nsISchemaAttribute)


/* void resolve (); */
NS_IMETHODIMP
nsSchemaAttributeRef::Resolve()
{
  nsresult rv = NS_OK;
  if (mIsResolved) {
    return NS_OK;
  }
  
  mIsResolved = PR_TRUE;
  if (!mAttribute && mSchema) {
    mSchema->GetAttributeByName(mRef, getter_AddRefs(mAttribute));
  }

  if (mAttribute) {
    rv = mAttribute->Resolve();
  }

  return rv;
}

/* void clear (); */
NS_IMETHODIMP
nsSchemaAttributeRef::Clear()
{
  if (mIsCleared) {
    return NS_OK;
  }

  mIsCleared = PR_TRUE;
  if (mAttribute) {
    mAttribute->Clear();
    mAttribute = nsnull;
  }

  return NS_OK;
}

/* readonly attribute AString name; */
NS_IMETHODIMP
nsSchemaAttributeRef::GetName(nsAString & aName)
{
  if (!mAttribute) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  return mAttribute->GetName(aName);
}

/* readonly attribute unsigned short componentType; */
NS_IMETHODIMP
nsSchemaAttributeRef::GetComponentType(PRUint16 *aComponentType)
{
  NS_ENSURE_ARG_POINTER(aComponentType);

  *aComponentType = nsISchemaAttributeComponent::COMPONENT_TYPE_ATTRIBUTE;

  return NS_OK;
}

/* readonly attribute nsISchemaSimpleType type; */
NS_IMETHODIMP
nsSchemaAttributeRef::GetType(nsISchemaSimpleType * *aType)
{
  NS_ENSURE_ARG_POINTER(aType);

  if (!mAttribute) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  return mAttribute->GetType(aType);
}

/* readonly attribute AString defaultValue; */
NS_IMETHODIMP
nsSchemaAttributeRef::GetDefaultValue(nsAString & aDefaultValue)
{
  aDefaultValue.Assign(mDefaultValue);
  
  return NS_OK;
}

/* readonly attribute AString fixedValue; */
NS_IMETHODIMP
nsSchemaAttributeRef::GetFixedValue(nsAString & aFixedValue)
{
  aFixedValue.Assign(mFixedValue);
  
  return NS_OK;
}

/* readonly attribute unsigned short use; */
NS_IMETHODIMP
nsSchemaAttributeRef::GetUse(PRUint16 *aUse)
{
  NS_ENSURE_ARG_POINTER(aUse);

  *aUse = mUse;

  return NS_OK;
}

NS_IMETHODIMP
nsSchemaAttributeRef::SetConstraints(const nsAString& aDefaultValue,
                                     const nsAString& aFixedValue)
{
  mDefaultValue.Assign(aDefaultValue);
  mFixedValue.Assign(aFixedValue);

  return NS_OK;
}
 
NS_IMETHODIMP
nsSchemaAttributeRef::SetUse(PRUint16 aUse)
{
  mUse = aUse;

  return NS_OK;
}

////////////////////////////////////////////////////////////
//
// nsSchemaAttributeGroup implementation
//
////////////////////////////////////////////////////////////
nsSchemaAttributeGroup::nsSchemaAttributeGroup(nsSchema* aSchema,
                                               const nsAString& aName)
  : nsSchemaComponentBase(aSchema), mName(aName)
{
}

nsSchemaAttributeGroup::~nsSchemaAttributeGroup()
{
}

NS_IMPL_ISUPPORTS3_CI(nsSchemaAttributeGroup,
                      nsISchemaComponent,
                      nsISchemaAttributeComponent,
                      nsISchemaAttributeGroup)

/* void resolve (); */
NS_IMETHODIMP
nsSchemaAttributeGroup::Resolve()
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
  
  return NS_OK;
}

/* void clear (); */
NS_IMETHODIMP
nsSchemaAttributeGroup::Clear()
{
  if (mIsCleared) {
    return NS_OK;
  }

  mIsCleared = PR_TRUE;

  PRUint32 i, count;
  count = mAttributes.Count();
  for (i = 0; i < count; ++i) {
    mAttributes.ObjectAt(i)->Clear();
  }
  mAttributes.Clear();
  mAttributesHash.Clear();

  return NS_OK;
}

/* readonly attribute AString name; */
NS_IMETHODIMP
nsSchemaAttributeGroup::GetName(nsAString & aName)
{
  aName.Assign(mName);

  return NS_OK;
}

/* readonly attribute unsigned short componentType; */
NS_IMETHODIMP
nsSchemaAttributeGroup::GetComponentType(PRUint16 *aComponentType)
{
  NS_ENSURE_ARG_POINTER(aComponentType);

  *aComponentType = nsISchemaAttributeComponent::COMPONENT_TYPE_GROUP;

  return NS_OK;
}

/* readonly attribute PRUint32 attributeCount; */
NS_IMETHODIMP
nsSchemaAttributeGroup::GetAttributeCount(PRUint32* aAttributeCount)
{
  NS_ENSURE_ARG_POINTER(aAttributeCount);

  *aAttributeCount = mAttributes.Count();

  return NS_OK;
}

/* nsISchemaAttributeComponent getAttributeByIndex (in PRUint32 index); */
NS_IMETHODIMP
nsSchemaAttributeGroup::GetAttributeByIndex(PRUint32 aIndex, 
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
nsSchemaAttributeGroup::GetAttributeByName(const nsAString& aName, 
                                           nsISchemaAttributeComponent** aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

  mAttributesHash.Get(aName, aResult);

  return NS_OK;
}

NS_IMETHODIMP
nsSchemaAttributeGroup::AddAttribute(nsISchemaAttributeComponent* aAttribute)
{
  NS_ENSURE_ARG_POINTER(aAttribute);

  nsAutoString name;
  aAttribute->GetName(name);

  mAttributes.AppendObject(aAttribute);
  mAttributesHash.Put(name, aAttribute);

  return NS_OK;    
}

////////////////////////////////////////////////////////////
//
// nsSchemaAttributeGroupRef implementation
//
////////////////////////////////////////////////////////////
nsSchemaAttributeGroupRef::nsSchemaAttributeGroupRef(nsSchema* aSchema,
                                                     const nsAString& aRef)
  : nsSchemaComponentBase(aSchema), mRef(aRef)
{
}

nsSchemaAttributeGroupRef::~nsSchemaAttributeGroupRef()
{
}

NS_IMPL_ISUPPORTS3_CI(nsSchemaAttributeGroupRef,
                      nsISchemaComponent,
                      nsISchemaAttributeComponent,
                      nsISchemaAttributeGroup)

/* void resolve (); */
NS_IMETHODIMP
nsSchemaAttributeGroupRef::Resolve()
{
  nsresult rv = NS_OK;
  if (mIsResolved) {
    return NS_OK;
  }

  mIsResolved = PR_TRUE;
  if (!mAttributeGroup && mSchema) {
    mSchema->GetAttributeGroupByName(mRef, getter_AddRefs(mAttributeGroup));
  }

  if (mAttributeGroup) {
    rv = mAttributeGroup->Resolve();
  }
  
  return rv;
}

/* void clear (); */
NS_IMETHODIMP
nsSchemaAttributeGroupRef::Clear()
{
  if (mIsCleared) {
    return NS_OK;
  }

  mIsCleared = PR_TRUE;
  if (mAttributeGroup) {
    mAttributeGroup->Clear();
    mAttributeGroup = nsnull;
  }

  return NS_OK;
}

/* readonly attribute AString name; */
NS_IMETHODIMP
nsSchemaAttributeGroupRef::GetName(nsAString & aName)
{
  if (!mAttributeGroup) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  return mAttributeGroup->GetName(aName);
}

/* readonly attribute unsigned short componentType; */
NS_IMETHODIMP
nsSchemaAttributeGroupRef::GetComponentType(PRUint16 *aComponentType)
{
  NS_ENSURE_ARG_POINTER(aComponentType);

  *aComponentType = nsISchemaAttributeComponent::COMPONENT_TYPE_GROUP;

  return NS_OK;
}

/* readonly attribute PRUint32 attributeCount; */
NS_IMETHODIMP
nsSchemaAttributeGroupRef::GetAttributeCount(PRUint32 *aAttributeCount)
{
  NS_ENSURE_ARG_POINTER(aAttributeCount);

  if (!mAttributeGroup) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  
  return mAttributeGroup->GetAttributeCount(aAttributeCount);
}

/* nsISchemaAttributeComponent getAttributeByIndex (in PRUint32 index); */
NS_IMETHODIMP
nsSchemaAttributeGroupRef::GetAttributeByIndex(PRUint32 index, 
                                               nsISchemaAttributeComponent **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  if (!mAttributeGroup) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  
  return mAttributeGroup->GetAttributeByIndex(index, _retval);
}

/* nsISchemaAttributeComponent getAttributeByName (in AString name); */
NS_IMETHODIMP
nsSchemaAttributeGroupRef::GetAttributeByName(const nsAString & name, 
                                              nsISchemaAttributeComponent **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  if (!mAttributeGroup) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  return mAttributeGroup->GetAttributeByName(name, _retval);
}

////////////////////////////////////////////////////////////
//
// nsSchemaAnyAttribute implementation
//
////////////////////////////////////////////////////////////
nsSchemaAnyAttribute::nsSchemaAnyAttribute(nsSchema* aSchema)
  : nsSchemaComponentBase(aSchema), mProcess(PROCESS_STRICT)
{
}

nsSchemaAnyAttribute::~nsSchemaAnyAttribute()
{
}

NS_IMPL_ISUPPORTS3_CI(nsSchemaAnyAttribute,
                      nsISchemaComponent,
                      nsISchemaAttributeComponent,
                      nsISchemaAnyAttribute)

/* void resolve (); */
NS_IMETHODIMP
nsSchemaAnyAttribute::Resolve()
{
  return NS_OK;
}

/* void clear (); */
NS_IMETHODIMP
nsSchemaAnyAttribute::Clear()
{
  return NS_OK;
}

/* readonly attribute unsigned short componentType; */
NS_IMETHODIMP
nsSchemaAnyAttribute::GetComponentType(PRUint16 *aComponentType)
{
  NS_ENSURE_ARG_POINTER(aComponentType);

  *aComponentType = nsISchemaAttributeComponent::COMPONENT_TYPE_ANY;

  return NS_OK;
}

/* readonly attribute AString name; */
NS_IMETHODIMP
nsSchemaAnyAttribute::GetName(nsAString & aName)
{
  aName.AssignLiteral("anyAttribute");

  return NS_OK;
}

/* readonly attribute unsigned short process; */
NS_IMETHODIMP
nsSchemaAnyAttribute::GetProcess(PRUint16 *aProcess)
{
  NS_ENSURE_ARG_POINTER(aProcess);

  *aProcess = mProcess;

  return NS_OK;
}

/* readonly attribute AString namespace; */
NS_IMETHODIMP
nsSchemaAnyAttribute::GetNamespace(nsAString & aNamespace)
{
  aNamespace.Assign(mNamespace);

  return NS_OK;
}

NS_IMETHODIMP
nsSchemaAnyAttribute::SetProcess(PRUint16 aProcess)
{
  mProcess = aProcess;
  
  return NS_OK;
}
 
NS_IMETHODIMP
nsSchemaAnyAttribute::SetNamespace(const nsAString& aNamespace)
{
  mNamespace.Assign(aNamespace);

  return NS_OK;
}
