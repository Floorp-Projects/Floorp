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
#include "nsIWebServiceErrorHandler.h"

nsSOAPArray::nsSOAPArray(nsISchemaType* aAnyType)
  : mAnyType(aAnyType)
{
}

nsSOAPArray::~nsSOAPArray()
{
}

NS_IMPL_ISUPPORTS3_CI(nsSOAPArray,
                      nsISchemaComponent,
                      nsISchemaType,
                      nsISchemaComplexType)

/* readonly attribute wstring targetNamespace; */
NS_IMETHODIMP 
nsSOAPArray::GetTargetNamespace(nsAString& aTargetNamespace)
{
  aTargetNamespace.AssignLiteral(NS_SOAP_1_2_ENCODING_NAMESPACE);
  return NS_OK;
}

/* void resolve(in nsIWebServiceErrorHandler); */
NS_IMETHODIMP
nsSOAPArray::Resolve(nsIWebServiceErrorHandler* aErrorHandler) 
{
  return NS_OK;
}

/* void clear(); */
NS_IMETHODIMP
nsSOAPArray::Clear()
{
  return NS_OK;
}

/* readonly attribute wstring name; */
NS_IMETHODIMP 
nsSOAPArray::GetName(nsAString& aName)
{
  aName.AssignLiteral("Array"); 
  return NS_OK;
}

/* readonly attribute unsigned short schemaType; */
NS_IMETHODIMP 
nsSOAPArray::GetSchemaType(PRUint16 *aSchemaType)
{
  NS_ENSURE_ARG_POINTER(aSchemaType);
  *aSchemaType = nsISchemaType::SCHEMA_TYPE_COMPLEX;
  return NS_OK;
}

/* readonly attribute unsigned short contentModel; */
NS_IMETHODIMP 
nsSOAPArray::GetContentModel(PRUint16 *aContentModel)
{
  NS_ENSURE_ARG_POINTER(aContentModel);
  *aContentModel = nsISchemaComplexType::CONTENT_MODEL_ELEMENT_ONLY;
  return NS_OK;
}

/* readonly attribute unsigned short derivation; */
NS_IMETHODIMP 
nsSOAPArray::GetDerivation(PRUint16 *aDerivation)
{
  NS_ENSURE_ARG_POINTER(aDerivation);
  *aDerivation = nsISchemaComplexType::DERIVATION_SELF_CONTAINED;
  return NS_OK;
}

/* readonly attribute nsISchemaType baseType; */
NS_IMETHODIMP 
nsSOAPArray::GetBaseType(nsISchemaType * *aBaseType)
{
  NS_ENSURE_ARG_POINTER(aBaseType);
  NS_ADDREF(*aBaseType = mAnyType);
  return NS_OK;
}

/* readonly attribute nsISchemaSimpleType simplBaseType; */
NS_IMETHODIMP 
nsSOAPArray::GetSimpleBaseType(nsISchemaSimpleType * *aSimpleBaseType)
{
  NS_ENSURE_ARG_POINTER(aSimpleBaseType);
  *aSimpleBaseType = nsnull;
  return NS_OK;
}

/* readonly attribute nsISchemaModelGroup modelGroup; */
NS_IMETHODIMP 
nsSOAPArray::GetModelGroup(nsISchemaModelGroup * *aModelGroup)
{
  NS_ENSURE_ARG_POINTER(aModelGroup);
  *aModelGroup = nsnull;
  return NS_OK;
}

/* readonly attribute PRUint32 attributeCount; */
NS_IMETHODIMP 
nsSOAPArray::GetAttributeCount(PRUint32 *aAttributeCount)
{
  NS_ENSURE_ARG_POINTER(aAttributeCount);
  *aAttributeCount = 0;
  return NS_OK;
}

/* nsISchemaAttributeComponent getAttributeByIndex (in PRUint32 index); */
NS_IMETHODIMP 
nsSOAPArray::GetAttributeByIndex(PRUint32 index, 
                                 nsISchemaAttributeComponent **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = nsnull;
  return NS_OK;
}

/* nsISchemaAttributeComponent getAttributeByName (in AString name); */
NS_IMETHODIMP 
nsSOAPArray::GetAttributeByName(const nsAString& name, 
                                nsISchemaAttributeComponent **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = nsnull;
  return NS_OK;
}

/* readonly attribute boolean abstract; */
NS_IMETHODIMP 
nsSOAPArray::GetAbstract(PRBool *aAbstract)
{
  NS_ENSURE_ARG_POINTER(aAbstract);
  *aAbstract = PR_FALSE;
  return NS_OK;
}

/* readonly attribute boolean isArray; */
NS_IMETHODIMP
nsSOAPArray::GetIsArray(PRBool* aIsArray) 
{
  NS_ENSURE_ARG_POINTER(aIsArray);
  *aIsArray = PR_TRUE;
  return NS_OK;
}

/* readonly attribute nsISchemaType arrayType; */
NS_IMETHODIMP
nsSOAPArray::GetArrayType(nsISchemaType** aArrayType)
{
  NS_ENSURE_ARG_POINTER(aArrayType);
  *aArrayType = mAnyType;
  NS_ADDREF(*aArrayType);
  return NS_OK;
}

/* readonly attribute PRUint32 arrayDimension; */
NS_IMETHODIMP
nsSOAPArray::GetArrayDimension(PRUint32* aDimension)
{
  NS_ENSURE_ARG_POINTER(aDimension);
  *aDimension = 0;
  return NS_OK;
}

nsSOAPArrayType::nsSOAPArrayType()
{
}

nsSOAPArrayType::~nsSOAPArrayType()
{
}

NS_IMPL_ISUPPORTS4_CI(nsSOAPArrayType,
                      nsISchemaComponent,
                      nsISchemaType,
                      nsISchemaSimpleType,
                      nsISchemaRestrictionType)

/* readonly attribute wstring targetNamespace; */
NS_IMETHODIMP 
nsSOAPArrayType::GetTargetNamespace(nsAString& aTargetNamespace)
{
  aTargetNamespace.AssignLiteral(NS_SOAP_1_2_ENCODING_NAMESPACE);
  return NS_OK;
}

/* void resolve(in nsIWebServiceErrorHandler); */
NS_IMETHODIMP
nsSOAPArrayType::Resolve(nsIWebServiceErrorHandler* aErrorHandler) 
{
  return NS_OK;
}

/* void clear(); */
NS_IMETHODIMP
nsSOAPArrayType::Clear()
{
  return NS_OK;
}

/* readonly attribute wstring name; */
NS_IMETHODIMP 
nsSOAPArrayType::GetName(nsAString& aName)
{
  aName.AssignLiteral("arrayType");
  return NS_OK;
}

/* readonly attribute unsigned short schemaType; */
NS_IMETHODIMP 
nsSOAPArrayType::GetSchemaType(PRUint16 *aSchemaType)
{
  NS_ENSURE_ARG_POINTER(aSchemaType);
  *aSchemaType = nsISchemaType::SCHEMA_TYPE_SIMPLE;
  return NS_OK;
}

/* readonly attribute unsigned short simpleType; */
NS_IMETHODIMP 
nsSOAPArrayType::GetSimpleType(PRUint16 *aSimpleType)
{
  NS_ENSURE_ARG_POINTER(aSimpleType);
  *aSimpleType = nsISchemaSimpleType::SIMPLE_TYPE_RESTRICTION;
  return NS_OK;
}

/* readonly attribute nsISchemaSimpleType baseType; */
NS_IMETHODIMP 
nsSOAPArrayType::GetBaseType(nsISchemaSimpleType * *aBaseType)
{
  NS_ENSURE_ARG_POINTER(aBaseType);
  *aBaseType = nsnull;
  return NS_OK;
}

/* readonly attribute PRUint32 facetCount; */
NS_IMETHODIMP 
nsSOAPArrayType::GetFacetCount(PRUint32 *aFacetCount)
{
  NS_ENSURE_ARG_POINTER(aFacetCount);
  *aFacetCount = 0;
  return NS_OK;
}

/* nsISchemaFacet getFacet(in PRUint32 index); */
NS_IMETHODIMP 
nsSOAPArrayType::GetFacet(PRUint32 index, nsISchemaFacet **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = nsnull;
  return NS_OK;
}
