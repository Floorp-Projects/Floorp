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

#ifndef __nsSchemaLoader_h__
#define __nsSchemaLoader_h__

#include "nsISchemaLoader.h"

// DOM includes
#include "nsIDOMElement.h"
#include "nsIDOMNodeList.h"

// XPCOM Includes
#include "nsCOMPtr.h"
#include "nsVoidArray.h"
#include "nsSupportsArray.h"
#include "nsHashtable.h"
#include "nsString.h"
#include "nsIAtom.h"

// Forward declarations
class nsSchemaLoadingContext;

class nsSchemaAtoms {
public:
  static void CreateSchemaAtoms();
  static void DestroySchemaAtoms();

  static nsIAtom* sString_atom;
  static nsIAtom* sNormalizedString_atom;
  static nsIAtom* sToken_atom;
  static nsIAtom* sByte_atom;
  static nsIAtom* sUnsignedByte_atom;
  static nsIAtom* sBase64Binary_atom;
  static nsIAtom* sHexBinary_atom;
  static nsIAtom* sInteger_atom;
  static nsIAtom* sPositiveInteger_atom;
  static nsIAtom* sNegativeInteger_atom;
  static nsIAtom* sNonnegativeInteger_atom;
  static nsIAtom* sNonpositiveInteger_atom;
  static nsIAtom* sInt_atom;
  static nsIAtom* sUnsignedInt_atom;
  static nsIAtom* sLong_atom;
  static nsIAtom* sUnsignedLong_atom;
  static nsIAtom* sShort_atom;
  static nsIAtom* sUnsignedShort_atom;
  static nsIAtom* sDecimal_atom;
  static nsIAtom* sFloat_atom;
  static nsIAtom* sDouble_atom;
  static nsIAtom* sBoolean_atom;
  static nsIAtom* sTime_atom;
  static nsIAtom* sDateTime_atom;
  static nsIAtom* sDuration_atom;
  static nsIAtom* sDate_atom;
  static nsIAtom* sGMonth_atom;
  static nsIAtom* sGYear_atom;
  static nsIAtom* sGYearMonth_atom;
  static nsIAtom* sGDay_atom;
  static nsIAtom* sGMonthDay_atom;
  static nsIAtom* sName_atom;
  static nsIAtom* sQName_atom;
  static nsIAtom* sNCName_atom;
  static nsIAtom* sAnyUri_atom;
  static nsIAtom* sLanguage_atom;
  static nsIAtom* sID_atom;
  static nsIAtom* sIDREF_atom;
  static nsIAtom* sIDREFS_atom;
  static nsIAtom* sENTITY_atom;
  static nsIAtom* sENTITIES_atom;
  static nsIAtom* sNOTATION_atom;
  static nsIAtom* sNMTOKEN_atom;
  static nsIAtom* sNMTOKENS_atom;

  static nsIAtom* sElement_atom;
  static nsIAtom* sModelGroup_atom;
  static nsIAtom* sAttribute_atom;
  static nsIAtom* sAttributeGroup_atom;
  static nsIAtom* sSimpleType_atom;
  static nsIAtom* sComplexType_atom;
  static nsIAtom* sSimpleContent_atom;
  static nsIAtom* sComplexContent_atom;
  static nsIAtom* sAll_atom;
  static nsIAtom* sChoice_atom;
  static nsIAtom* sSequence_atom;
  static nsIAtom* sAnyAttribute_atom;
  static nsIAtom* sRestriction_atom;
  static nsIAtom* sExtension_atom;
  static nsIAtom* sAnnotation_atom;
};

class nsSchemaLoader : public nsISchemaLoader
{
public:
  nsSchemaLoader();
  virtual ~nsSchemaLoader();

  NS_DECL_ISUPPORTS
  NS_DECL_NSISCHEMALOADER

protected:
  nsresult ProcessElement(nsSchema* aSchema, 
                          nsSchemaLoadingContext* aContext,
                          nsIDOMElement* aElement,
                          nsISchemaElement** aSchemaElement);
  nsresult ProcessComplexType(nsSchema* aSchema, 
                              nsSchemaLoadingContext* aContext,
                              nsIDOMElement* aElement,
                              nsISchemaComplexType** aComplexType);
  nsresult ProcessComplexTypeBody(nsSchema* aSchema, 
                                  nsSchemaLoadingContext* aContext,
                                  nsIDOMElement* aElement,
                                  nsSchemaComplexType* aComplexType,
                                  nsSchemaModelGroup* aSequence,
                                  PRUint16* aContentModel);
  nsresult ProcessSimpleType(nsSchema* aSchema, 
                             nsSchemaLoadingContext* aContext,
                             nsIDOMElement* aElement,
                             nsISchemaSimpleType** aSimpleType);
  nsresult ProcessAttribute(nsSchema* aSchema, 
                            nsSchemaLoadingContext* aContext,
                            nsIDOMElement* aElement,
                            nsISchemaAttribute** aAttribute);
  nsresult ProcessAttributeGroup(nsSchema* aSchema, 
                                 nsSchemaLoadingContext* aContext,
                                 nsIDOMElement* aElement,
                                 nsISchemaAttributeGroup** aAttributeGroup);
  nsresult ProcessAttributeComponent(nsSchema* aSchema, 
                                     nsSchemaLoadingContext* aContext,
                                     nsIDOMElement* aElement,
                                     nsISchemaAttributeComponent** aAttribute);
  nsresult ProcessModelGroup(nsSchema* aSchema, 
                             nsSchemaLoadingContext* aContext,
                             nsIDOMElement* aElement,
                             nsISchemaModelGroup** aModelGroup);
  nsresult ProcessSimpleContent(nsSchema* aSchema, 
                                nsSchemaLoadingContext* aContext,
                                nsIDOMElement* aElement,
                                nsSchemaComplexType* aComplexType,
                                PRUint16* aDerivation,
                                nsISchemaType** aBaseType);
  nsresult ProcessComplexContent(nsSchema* aSchema, 
                                 nsSchemaLoadingContext* aContext,
                                 nsIDOMElement* aElement,
                                 nsSchemaComplexType* aComplexType,
                                 PRUint16* aContentModel,
                                 PRUint16* aDerivation,
                                 nsISchemaType** aBaseType);
  nsresult ProcessSimpleContentRestriction(nsSchema* aSchema, 
                                           nsSchemaLoadingContext* aContext, 
                                           nsIDOMElement* aElement,
                                           nsSchemaComplexType* aComplexType, 
                                           nsISchemaType* aBaseType,
                                           nsISchemaSimpleType** aSimpleBaseType);
  nsresult ProcessSimpleContentExtension(nsSchema* aSchema, 
                                         nsSchemaLoadingContext* aContext, 
                                         nsIDOMElement* aElement,
                                         nsSchemaComplexType* aComplexType,
                                         nsISchemaType* aBaseType,
                                         nsISchemaSimpleType** aSimpleBaseType);

  nsresult GetBuiltinType(const nsAReadableString& aName,
                          nsISchemaType** aType);
  nsresult GetNewOrUsedType(nsSchema* aSchema,
                            nsSchemaLoadingContext* aContext,
                            const nsAReadableString& aTypeName,
                            nsISchemaType** aType);

  void GetMinAndMax(nsIDOMElement* aElement,
		    PRUint32* aMinOccurs,
		    PRUint32* aMaxOccurs);

protected:
  nsSupportsHashtable mBuiltinTypesHash;
};

class nsSchemaLoadingContext {
public:
  nsSchemaLoadingContext();
  ~nsSchemaLoadingContext();

  nsresult PushNamespaceDecls(nsIDOMElement* aElement);
  nsresult PopNamespaceDecls(nsIDOMElement* aElement);
  PRBool GetNamespaceURIForPrefix(const nsAReadableString& aPrefix,
                                  nsAWritableString& aURI);

protected:
  nsVoidArray mNamespaceStack;
};

#endif // __nsSchemaLoader_h__
