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
// nsSchemaParticleBase implementation
//
////////////////////////////////////////////////////////////
nsSchemaParticleBase::nsSchemaParticleBase(nsSchema* aSchema)
  : nsSchemaComponentBase(aSchema), mMinOccurs(1), mMaxOccurs(1)
{
}

nsSchemaParticleBase::~nsSchemaParticleBase()
{
}

NS_IMETHODIMP
nsSchemaParticleBase::GetMinOccurs(PRUint32 *aMinOccurs)
{
  NS_ENSURE_ARG_POINTER(aMinOccurs);

  *aMinOccurs = mMinOccurs;

  return NS_OK;
}
 
NS_IMETHODIMP
nsSchemaParticleBase::GetMaxOccurs(PRUint32 *aMaxOccurs)
{
  NS_ENSURE_ARG_POINTER(aMaxOccurs);

  *aMaxOccurs = mMaxOccurs;

  return NS_OK;
}

NS_IMETHODIMP
nsSchemaParticleBase::SetMinOccurs(PRUint32 aMinOccurs)
{
  mMinOccurs = aMinOccurs;

  if (mMaxOccurs < mMinOccurs) {
    mMaxOccurs = mMinOccurs;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsSchemaParticleBase::SetMaxOccurs(PRUint32 aMaxOccurs)
{
  mMaxOccurs = aMaxOccurs;

  if (mMinOccurs > mMaxOccurs) {
    mMinOccurs = mMaxOccurs;
  }

  return NS_OK;
}

////////////////////////////////////////////////////////////
//
// nsSchemaModelGroup implementation
//
////////////////////////////////////////////////////////////
nsSchemaModelGroup::nsSchemaModelGroup(nsSchema* aSchema, 
                                       const nsAString& aName)
  : nsSchemaParticleBase(aSchema), mName(aName), mCompositor(COMPOSITOR_SEQUENCE)
{
}

nsSchemaModelGroup::~nsSchemaModelGroup()
{
}

NS_IMPL_ISUPPORTS3_CI(nsSchemaModelGroup, 
                      nsISchemaComponent,
                      nsISchemaParticle,
                      nsISchemaModelGroup)


/* void resolve (); */
NS_IMETHODIMP
nsSchemaModelGroup::Resolve()
{
  if (mIsResolved) {
    return NS_OK;
  }

  mIsResolved = PR_TRUE;
  nsresult rv;
  PRUint32 i, count;

  count = mParticles.Count();
  for (i = 0; i < count; ++i) {
    rv = mParticles.ObjectAt(i)->Resolve();
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  return NS_OK;
}

/* void clear (); */
NS_IMETHODIMP
nsSchemaModelGroup::Clear()
{
  if (mIsCleared) {
    return NS_OK;
  }

  mIsCleared = PR_TRUE;

  PRUint32 i, count;
  count = mParticles.Count();
  for (i = 0; i < count; ++i) {
    mParticles.ObjectAt(i)->Clear();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsSchemaModelGroup::GetParticleType(PRUint16 *aParticleType)
{
  NS_ENSURE_ARG_POINTER(aParticleType);

  *aParticleType = nsISchemaParticle::PARTICLE_TYPE_MODEL_GROUP;

  return NS_OK;
}

NS_IMETHODIMP
nsSchemaModelGroup::GetName(nsAString& aName)
{
  aName.Assign(mName);

  return NS_OK;
}

/* readonly attribute unsigned short compositor; */
NS_IMETHODIMP
nsSchemaModelGroup::GetCompositor(PRUint16 *aCompositor)
{
  NS_ENSURE_ARG_POINTER(aCompositor);

  *aCompositor = mCompositor;

  return NS_OK;
}

/* readonly attribute PRUint32 particleCount; */
NS_IMETHODIMP
nsSchemaModelGroup::GetParticleCount(PRUint32 *aParticleCount)
{
  NS_ENSURE_ARG_POINTER(aParticleCount);

  *aParticleCount = mParticles.Count();

  return NS_OK;
}

/* nsISchemaParticle getParticle (in PRUint32 index); */
NS_IMETHODIMP
nsSchemaModelGroup::GetParticle(PRUint32 aIndex, nsISchemaParticle** aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

  if (aIndex >= (PRUint32)mParticles.Count()) {
    return NS_ERROR_FAILURE;
  }

  NS_ADDREF(*aResult = mParticles.ObjectAt(aIndex));

  return NS_OK;
}

/* nsISchemaElement getElementByName(in AString name); */
NS_IMETHODIMP
nsSchemaModelGroup::GetElementByName(const nsAString& aName,
                                     nsISchemaElement** aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

  PRUint32 i, count;
  count = mParticles.Count();

  for (i = 0; i < count; ++i) {
    nsISchemaParticle* particle = mParticles.ObjectAt(i);

    nsCOMPtr<nsISchemaElement> element = do_QueryInterface(particle);
    if (element) {
      nsAutoString name;
      element->GetName(name);

      if (name.Equals(aName)) {
        NS_ADDREF(*aResult = element);

        return NS_OK;
      }
    }
    else {
      nsCOMPtr<nsISchemaModelGroup> group = do_QueryInterface(particle);
      if (group) {
        nsresult rv = group->GetElementByName(aName, aResult);
        if (NS_SUCCEEDED(rv)) {
          return NS_OK;
        }
      }
    }
  }

  return NS_ERROR_FAILURE; // No element of that name found
}

NS_IMETHODIMP
nsSchemaModelGroup::SetCompositor(PRUint16 aCompositor)
{
  mCompositor = aCompositor;

  return NS_OK;
}

NS_IMETHODIMP
nsSchemaModelGroup::AddParticle(nsISchemaParticle* aParticle)
{
  NS_ENSURE_ARG_POINTER(aParticle);

  return mParticles.AppendObject(aParticle) ? NS_OK : NS_ERROR_FAILURE;
}

////////////////////////////////////////////////////////////
//
// nsSchemaModelGroupRef implementation
//
////////////////////////////////////////////////////////////
nsSchemaModelGroupRef::nsSchemaModelGroupRef(nsSchema* aSchema,
                                             const nsAString& aRef)
  : nsSchemaParticleBase(aSchema), mRef(aRef)
{
}

nsSchemaModelGroupRef::~nsSchemaModelGroupRef()
{
}

NS_IMPL_ISUPPORTS3_CI(nsSchemaModelGroupRef, 
                      nsISchemaComponent,
                      nsISchemaParticle,
                      nsISchemaModelGroup)

/* void resolve (); */
NS_IMETHODIMP
nsSchemaModelGroupRef::Resolve()
{
  nsresult rv = NS_OK;

  if (mIsResolved) {
    return NS_OK;
  }

  mIsResolved = PR_TRUE;
  if (!mModelGroup && mSchema) {
    mSchema->GetModelGroupByName(mRef, getter_AddRefs(mModelGroup));
  }

  if (mModelGroup) {
    rv = mModelGroup->Resolve();
  }

  return rv;
}

/* void clear (); */
NS_IMETHODIMP
nsSchemaModelGroupRef::Clear()
{
  if (mIsCleared) {
    return NS_OK;
  }

  mIsCleared = PR_TRUE;
  if (mModelGroup) {
    mModelGroup->Clear();
    mModelGroup = nsnull;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsSchemaModelGroupRef::GetParticleType(PRUint16 *aParticleType)
{
  NS_ENSURE_ARG_POINTER(aParticleType);

  *aParticleType = nsISchemaParticle::PARTICLE_TYPE_MODEL_GROUP;

  return NS_OK;
}

NS_IMETHODIMP
nsSchemaModelGroupRef::GetName(nsAString& aName)
{
  if (!mModelGroup) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  return mModelGroup->GetName(aName);
}


/* readonly attribute unsigned short compositor; */
NS_IMETHODIMP
nsSchemaModelGroupRef::GetCompositor(PRUint16 *aCompositor)
{
  NS_ENSURE_ARG_POINTER(aCompositor);

  if (!mModelGroup) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  return mModelGroup->GetCompositor(aCompositor);
}

/* readonly attribute PRUint32 particleCount; */
NS_IMETHODIMP
nsSchemaModelGroupRef::GetParticleCount(PRUint32 *aParticleCount)
{
  NS_ENSURE_ARG_POINTER(aParticleCount);
  
  if (!mModelGroup) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  return mModelGroup->GetParticleCount(aParticleCount);  
}

/* nsISchemaParticle getParticle (in PRUint32 index); */
NS_IMETHODIMP
nsSchemaModelGroupRef::GetParticle(PRUint32 index, nsISchemaParticle **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  
  if (!mModelGroup) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  return mModelGroup->GetParticle(index, _retval);
}

NS_IMETHODIMP
nsSchemaModelGroupRef::GetElementByName(const nsAString& aName,
                                        nsISchemaElement** _retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  
  if (!mModelGroup) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  return mModelGroup->GetElementByName(aName, _retval);
}

////////////////////////////////////////////////////////////
//
// nsSchemaAnyParticle implementation
//
////////////////////////////////////////////////////////////
nsSchemaAnyParticle::nsSchemaAnyParticle(nsSchema* aSchema)
  : nsSchemaParticleBase(aSchema), mProcess(PROCESS_STRICT)
{
}

nsSchemaAnyParticle::~nsSchemaAnyParticle()
{
}

NS_IMPL_ISUPPORTS3_CI(nsSchemaAnyParticle, 
                      nsISchemaComponent,
                      nsISchemaParticle,
                      nsISchemaAnyParticle)


/* void resolve (); */
NS_IMETHODIMP
nsSchemaAnyParticle::Resolve()
{
  return NS_OK;
}

/* void clear (); */
NS_IMETHODIMP
nsSchemaAnyParticle::Clear()
{
  return NS_OK;
}

NS_IMETHODIMP
nsSchemaAnyParticle::GetParticleType(PRUint16 *aParticleType)
{
  NS_ENSURE_ARG_POINTER(aParticleType);

  *aParticleType = nsISchemaParticle::PARTICLE_TYPE_ANY;

  return NS_OK;
}

NS_IMETHODIMP
nsSchemaAnyParticle::GetName(nsAString& aName)
{
  aName.AssignLiteral("any");

  return NS_OK;
}

/* readonly attribute unsigned short process; */
NS_IMETHODIMP
nsSchemaAnyParticle::GetProcess(PRUint16 *aProcess)
{
  NS_ENSURE_ARG_POINTER(aProcess);
  
  *aProcess = mProcess;

  return NS_OK;
}

/* readonly attribute AString namespace; */
NS_IMETHODIMP
nsSchemaAnyParticle::GetNamespace(nsAString & aNamespace)
{
  aNamespace.Assign(mNamespace);

  return NS_OK;
}

NS_IMETHODIMP
nsSchemaAnyParticle::SetProcess(PRUint16 aProcess)
{
  mProcess = aProcess;
  
  return NS_OK;
}

NS_IMETHODIMP
nsSchemaAnyParticle::SetNamespace(const nsAString& aNamespace)
{
  mNamespace.Assign(aNamespace);

  return NS_OK;
}

////////////////////////////////////////////////////////////
//
// nsSchemaElement implementation
//
////////////////////////////////////////////////////////////
nsSchemaElement::nsSchemaElement(nsSchema* aSchema, 
                                 const nsAString& aName)
  : nsSchemaParticleBase(aSchema), mName(aName), mFlags(0)
{
}

nsSchemaElement::~nsSchemaElement()
{
}

NS_IMPL_ISUPPORTS3_CI(nsSchemaElement, 
                      nsISchemaComponent,
                      nsISchemaParticle,
                      nsISchemaElement)

/* void resolve (); */
NS_IMETHODIMP
nsSchemaElement::Resolve()
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
      return rv;
    }
    
    mType = type;
    rv = mType->Resolve();
  }

  return rv;
}

/* void clear (); */
NS_IMETHODIMP
nsSchemaElement::Clear()
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

NS_IMETHODIMP
nsSchemaElement::GetParticleType(PRUint16 *aParticleType)
{
  NS_ENSURE_ARG_POINTER(aParticleType);

  *aParticleType = nsISchemaParticle::PARTICLE_TYPE_ELEMENT;

  return NS_OK;
}

NS_IMETHODIMP
nsSchemaElement::GetName(nsAString& aName)
{
  aName.Assign(mName);

  return NS_OK;
}

/* readonly attribute nsISchemaType type; */
NS_IMETHODIMP
nsSchemaElement::GetType(nsISchemaType * *aType)
{
  NS_ENSURE_ARG_POINTER(aType);
  
  NS_IF_ADDREF(*aType = mType);

  return NS_OK;
}

/* readonly attribute AString defaultValue; */
NS_IMETHODIMP
nsSchemaElement::GetDefaultValue(nsAString & aDefaultValue)
{
  aDefaultValue.Assign(mDefaultValue);
  
  return NS_OK;
}

/* readonly attribute AString fixedValue; */
NS_IMETHODIMP
nsSchemaElement::GetFixedValue(nsAString & aFixedValue)
{
  aFixedValue.Assign(mFixedValue);
  
  return NS_OK;
}

/* readonly attribute boolean nillable; */
NS_IMETHODIMP
nsSchemaElement::GetNillable(PRBool *aNillable)
{
  NS_ENSURE_ARG_POINTER(aNillable);

  *aNillable = mFlags & nsSchemaElement::NILLABLE;

  return NS_OK;
}

/* readonly attribute boolean abstract; */
NS_IMETHODIMP
nsSchemaElement::GetAbstract(PRBool *aAbstract)
{
  NS_ENSURE_ARG_POINTER(aAbstract);

  *aAbstract = mFlags & nsSchemaElement::ABSTRACT;

  return NS_OK;
}

NS_IMETHODIMP
nsSchemaElement::SetType(nsISchemaType* aType)
{
  NS_ENSURE_ARG_POINTER(aType);

  mType = aType;

  return NS_OK;
}

NS_IMETHODIMP
nsSchemaElement::SetConstraints(const nsAString& aDefaultValue,
                                const nsAString& aFixedValue)
{
  mDefaultValue.Assign(aDefaultValue);
  mFixedValue.Assign(aFixedValue);

  return NS_OK;
}

NS_IMETHODIMP
nsSchemaElement::SetFlags(PRInt32 aFlags)
{
  mFlags = aFlags;
  return NS_OK;
}

NS_IMETHODIMP
nsSchemaElement::GetTargetNamespace(nsAString& aTargetNamespace)
{
  if ((mFlags & nsSchemaElement::FORM_QUALIFIED) && mSchema) {
    return mSchema->GetTargetNamespace(aTargetNamespace);
  }
  aTargetNamespace.Truncate();
  return NS_OK;
}

////////////////////////////////////////////////////////////
//
// nsSchemaElementRef implementation
//
////////////////////////////////////////////////////////////
nsSchemaElementRef::nsSchemaElementRef(nsSchema* aSchema, 
                                       const nsAString& aRef)
  : nsSchemaParticleBase(aSchema), mRef(aRef)
{
}

nsSchemaElementRef::~nsSchemaElementRef()
{
}

NS_IMPL_ISUPPORTS3_CI(nsSchemaElementRef, 
                      nsISchemaComponent,
                      nsISchemaParticle,
                      nsISchemaElement)

/* void resolve (); */
NS_IMETHODIMP
nsSchemaElementRef::Resolve()
{
  nsresult rv = NS_OK;
  if (mIsResolved) {
    return NS_OK;
  }

  mIsResolved = PR_TRUE;
  if (!mElement && mSchema) {
    mSchema->GetElementByName(mRef, getter_AddRefs(mElement));
  }

  if (mElement) {
    rv = mElement->Resolve();
  }

  return rv;
}

/* void clear (); */
NS_IMETHODIMP
nsSchemaElementRef::Clear()
{
  if (mIsCleared) {
    return NS_OK;
  }

  mIsCleared = PR_TRUE;
  if (mElement) {
    mElement->Clear();
    mElement = nsnull;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsSchemaElementRef::GetParticleType(PRUint16 *aParticleType)
{
  NS_ENSURE_ARG_POINTER(aParticleType);

  *aParticleType = nsISchemaParticle::PARTICLE_TYPE_ELEMENT;

  return NS_OK;
}

NS_IMETHODIMP
nsSchemaElementRef::GetName(nsAString& aName)
{
  if (!mElement) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  return mElement->GetName(aName);
}

/* readonly attribute nsISchemaType type; */
NS_IMETHODIMP
nsSchemaElementRef::GetType(nsISchemaType * *aType)
{
  NS_ENSURE_ARG_POINTER(aType);
  
  if (!mElement) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  return mElement->GetType(aType);
}

/* readonly attribute AString defaultValue; */
NS_IMETHODIMP
nsSchemaElementRef::GetDefaultValue(nsAString & aDefaultValue)
{
  if (!mElement) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  return mElement->GetDefaultValue(aDefaultValue);
}

/* readonly attribute AString fixedValue; */
NS_IMETHODIMP
nsSchemaElementRef::GetFixedValue(nsAString & aFixedValue)
{
  if (!mElement) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  return mElement->GetFixedValue(aFixedValue);
}

/* readonly attribute boolean nillable; */
NS_IMETHODIMP
nsSchemaElementRef::GetNillable(PRBool *aNillable)
{
  NS_ENSURE_ARG_POINTER(aNillable);

  if (!mElement) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  return mElement->GetNillable(aNillable);
}

/* readonly attribute boolean abstract; */
NS_IMETHODIMP
nsSchemaElementRef::GetAbstract(PRBool *aAbstract)
{
  NS_ENSURE_ARG_POINTER(aAbstract);

  if (!mElement) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  return mElement->GetAbstract(aAbstract);
}

