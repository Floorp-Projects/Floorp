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

#ifndef __nsSchemaLoader_h__
#define __nsSchemaLoader_h__

#include "nsIWebServiceErrorHandler.h"
#include "nsISchemaLoader.h"
#include "nsSchemaPrivate.h"
#include "nsDOMUtils.h"

// DOM includes
#include "nsIDOMElement.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMNode.h"

// XPCOM Includes
#include "nsCOMPtr.h"
#include "nsVoidArray.h"
#include "nsHashtable.h"
#include "nsString.h"
#include "nsIAtom.h"

// Loading includes
#include "nsIURI.h"

class nsSchemaAtoms 
{
public:
  static nsresult AddRefAtoms();

#define SCHEMA_ATOM(_name, _value) static nsIAtom* _name;
#include "nsSchemaAtomList.h"
#undef SCHEMA_ATOM
};

class nsBuiltinSchemaCollection : public nsISchemaCollection
{
public:
  nsBuiltinSchemaCollection();
  virtual ~nsBuiltinSchemaCollection();

  NS_DECL_ISUPPORTS
  NS_DECL_NSISCHEMACOLLECTION

protected:
  nsresult GetBuiltinType(const nsAString& aName,
                          const nsAString& aNamespace,
                          nsISchemaType** aType);
  nsresult GetSOAPType(const nsAString& aName,
                       const nsAString& aNamespace,
                       nsISchemaType** aType);

protected:
  nsSupportsHashtable mBuiltinTypesHash;
  nsSupportsHashtable mSOAPTypeHash;
};

class nsSchemaLoader : public nsISchemaLoader,
                       public nsISchemaCollection
{
public:
  nsSchemaLoader();
  virtual ~nsSchemaLoader();

  NS_DECL_ISUPPORTS
  NS_DECL_NSISCHEMALOADER
  NS_DECL_NSISCHEMACOLLECTION

protected:
  nsresult ProcessElement(nsIWebServiceErrorHandler* aErrorHandler,
                          nsSchema* aSchema, 
                          nsIDOMElement* aElement,
                          nsISchemaElement** aSchemaElement);
  nsresult ProcessComplexType(nsIWebServiceErrorHandler* aErrorHandler,
                              nsSchema* aSchema, 
                              nsIDOMElement* aElement,
                              nsISchemaComplexType** aComplexType);
  nsresult ProcessComplexTypeBody(nsIWebServiceErrorHandler* aErrorHandler,
                                  nsSchema* aSchema, 
                                  nsIDOMElement* aElement,
                                  nsSchemaComplexType* aComplexType,
                                  nsSchemaModelGroup* aSequence,
                                  PRUint16* aContentModel);
  nsresult ProcessSimpleContent(nsIWebServiceErrorHandler* aErrorHandler,
                                nsSchema* aSchema, 
                                nsIDOMElement* aElement,
                                nsSchemaComplexType* aComplexType,
                                PRUint16* aDerivation,
                                nsISchemaType** aBaseType);
  nsresult ProcessSimpleContentRestriction(nsIWebServiceErrorHandler* aErrorHandler,
                                           nsSchema* aSchema, 
                                           nsIDOMElement* aElement,
                                           nsSchemaComplexType* aComplexType, 
                                           nsISchemaType* aBaseType,
                                           nsISchemaSimpleType** aSimpleBaseType);
  nsresult ProcessSimpleContentExtension(nsIWebServiceErrorHandler* aErrorHandler,
                                         nsSchema* aSchema, 
                                         nsIDOMElement* aElement,
                                         nsSchemaComplexType* aComplexType,
                                         nsISchemaType* aBaseType,
                                         nsISchemaSimpleType** aSimpleBaseType);
  nsresult ProcessComplexContent(nsIWebServiceErrorHandler* aErrorHandler,
                                 nsSchema* aSchema, 
                                 nsIDOMElement* aElement,
                                 nsSchemaComplexType* aComplexType,
                                 PRUint16* aContentModel,
                                 PRUint16* aDerivation,
                                 nsISchemaType** aBaseType);
  nsresult ProcessSimpleType(nsIWebServiceErrorHandler* aErrorHandler,
                             nsSchema* aSchema, 
                             nsIDOMElement* aElement,
                             nsISchemaSimpleType** aSimpleType);
  nsresult ProcessSimpleTypeRestriction(nsIWebServiceErrorHandler* aErrorHandler,
                                        nsSchema* aSchema, 
                                        nsIDOMElement* aElement,
                                        const nsAString& aName,
                                        nsISchemaSimpleType** aSimpleType);
  nsresult ProcessSimpleTypeList(nsIWebServiceErrorHandler* aErrorHandler,
                                 nsSchema* aSchema, 
                                 nsIDOMElement* aElement,
                                 const nsAString& aName,
                                 nsISchemaSimpleType** aSimpleType);
  nsresult ProcessSimpleTypeUnion(nsIWebServiceErrorHandler* aErrorHandler,
                                  nsSchema* aSchema, 
                                  nsIDOMElement* aElement,
                                  const nsAString& aName,
                                  nsISchemaSimpleType** aSimpleType);
  nsresult ProcessAttribute(nsIWebServiceErrorHandler* aErrorHandler,
                            nsSchema* aSchema, 
                            nsIDOMElement* aElement,
                            nsISchemaAttribute** aAttribute);
  nsresult ProcessAttributeGroup(nsIWebServiceErrorHandler* aErrorHandler,
                                 nsSchema* aSchema, 
                                 nsIDOMElement* aElement,
                                 nsISchemaAttributeGroup** aAttributeGroup);
  nsresult ProcessAttributeComponent(nsIWebServiceErrorHandler* aErrorHandler,
                                     nsSchema* aSchema, 
                                     nsIDOMElement* aElement,
                                     nsIAtom* aTagName,
                                     nsISchemaAttributeComponent** aAttribute);
  nsresult ProcessModelGroup(nsIWebServiceErrorHandler* aErrorHandler,
                             nsSchema* aSchema, 
                             nsIDOMElement* aElement,
                             nsIAtom* aTagName,
                             nsSchemaModelGroup* aParentSequence,
                             nsISchemaModelGroup** aModelGroup);
  nsresult ProcessParticle(nsIWebServiceErrorHandler* aErrorHandler,
                           nsSchema* aSchema, 
                           nsIDOMElement* aElement,
                           nsIAtom* aTagName,
                           nsISchemaParticle** aModelGroup);
  nsresult ProcessFacet(nsIWebServiceErrorHandler* aErrorHandler,
                        nsSchema* aSchema, 
                        nsIDOMElement* aElement,
                        nsIAtom* aTagName,
                        nsISchemaFacet** aFacet);

  nsresult GetNewOrUsedType(nsSchema* aSchema,
                            nsIDOMElement* aContext,
                            const nsAString& aTypeName,
                            nsISchemaType** aType);

  void GetUse(nsIDOMElement* aElement, 
              PRUint16* aUse);
  void GetProcess(nsIDOMElement* aElement, 
                  PRUint16* aProcess);
  void GetMinAndMax(nsIDOMElement* aElement,
                    PRUint32* aMinOccurs,
                    PRUint32* aMaxOccurs);

  nsresult GetResolvedURI(const nsAString& aSchemaURI,
                          const char* aMethod, nsIURI** aURI);

  nsresult ParseArrayType(nsSchema* aSchema,
                          nsIDOMElement* aAttrElement,
                          const nsAString& aStr,
                          nsISchemaType** aType,
                          PRUint32* aDimension);
  nsresult ParseDimensions(nsSchema* aSchema,
                           nsIDOMElement* aAttrElement,
                           const nsAString& aStr,
                           nsISchemaType* aBaseType,
                           nsISchemaType** aArrayType,
                           PRUint32* aDimension);
  void ConstructArrayName(nsISchemaType* aType,
                          nsAString& aName);

protected:
  nsSupportsHashtable mSchemas;
  nsCOMPtr<nsISchemaCollection> mBuiltinCollection;
};

#endif // __nsSchemaLoader_h__
