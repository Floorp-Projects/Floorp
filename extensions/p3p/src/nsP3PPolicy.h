/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and imitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is International
 * Business Machines Corporation. Portions created by IBM
 * Corporation are Copyright (C) 2000 International Business
 * Machines Corporation. All Rights Reserved.
 *
 * Contributor(s): IBM Corporation.
 *
 */

#ifndef nsP3PPolicy_h__
#define nsP3PPolicy_h__

#include "nsIP3PPolicy.h"
#include "nsIP3PDataSchema.h"
#include "nsIP3PXMLListener.h"

#include <nsIRDFService.h>
#include <nsIRDFDataSource.h>
#include <nsIRDFResource.h>
#include <nsIRDFContainerUtils.h>

#include <nsHashtable.h>
#include <nsVoidArray.h>
#include <nsSupportsArray.h>


class nsP3PPolicy : public nsIP3PPolicy,
                    public nsIP3PXMLListener {
public:
  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIP3PPolicy
  NS_DECL_NSIP3PPOLICY

  // nsIP3PXMLListener
  NS_DECL_NSIP3PXMLLISTENER

  // nsP3PXMLProcessor method overrides
  NS_IMETHOD            PostInit( nsString&  aURISpec );

  NS_IMETHOD            PreRead( PRInt32   aReadType,
                                 nsIURI   *aReadForURI );

  NS_IMETHOD            ProcessContent( PRInt32   aReadType );

  NS_IMETHOD_( void )   CheckForComplete( );

  // nsP3PPolicy methods
  nsP3PPolicy( );
  virtual ~nsP3PPolicy( );

  NS_METHOD             AddRDFDataTag( nsString&       aDescription,
                                       nsStringArray  *aFailedPrefsCombinations );

protected:
  NS_METHOD_( void )    ProcessPolicy( PRInt32   aReadType );

  NS_METHOD             ProcessExpiration( );

  NS_METHOD             ProcessExpiryTagExpiry( nsIP3PTag  *aTag );

  NS_METHOD             ProcessDataSchemas( PRInt32   aReadType );

  NS_METHOD             ProcessEntityTagDataSchemas( nsIP3PTag  *aTag,
                                                     PRInt32     aReadType );

  NS_METHOD             ProcessStatementTagDataSchemas( nsIP3PTag  *aTag,
                                                        PRInt32     aReadType );

  NS_METHOD             ProcessDataGroupTagDataSchema( nsIP3PTag  *aTag,
                                                       PRInt32     aReadType );

  NS_METHOD             ProcessDataTagDataSchema( nsIP3PTag  *aTag,
                                                  nsString&   aDefaultDataSchemaURISpec,
                                                  PRInt32     aReadType );

  NS_METHOD             AddDataSchema( nsString&  aDataSchema,
                                       PRInt32    aReadType );

  NS_METHOD_( void )    SetNotValid( );

  NS_METHOD             CheckExpiration( PRBool&  aExpired );

  NS_METHOD_( void )    CompareToPrefs( );

  NS_METHOD             CreateRDFPolicyDescription( nsIP3PTag  *aEntityTag,
                                                    nsString&   aDescURISpec );

  NS_METHOD             CreateRDFEntityDescription( nsIP3PTag  *aEntityTag );

  NS_METHOD             CreateRDFDataTagsDescription( );

  NS_METHOD             CreateRDFDataCombinationsDescription( nsString&       aName,
                                                              nsStringArray  *aFailedPrefsCombinations );

  NS_METHOD             AddRDFEntry( nsString&        aResourceName,
                                     nsString&        aTitle,
                                     nsString&        aValue,
                                     nsIRDFResource  *aResourceType,
                                     nsIRDFResource  *aContainer,
                                     nsIRDFResource **aResource );

  NS_METHOD             CompareStatementTagToPrefs( nsIP3PTag  *aTag );

  NS_METHOD             CompareDataGroupTagToPrefs( nsIP3PTag  *aTag,
                                                    nsIP3PTag  *aPurposeTag,
                                                    nsIP3PTag  *aRecipientTag );

  NS_METHOD             CompareDataTagToPrefs( nsIP3PTag  *aTag,
                                               nsIP3PTag  *aPurposeTag,
                                               nsIP3PTag  *aRecipientTag );

  NS_METHOD             CheckPref( nsString&   aRefDataSchemaURISpec,
                                   nsString&   aRefDataStructName,
                                   nsIP3PTag  *aPurposeTag,
                                   nsIP3PTag  *aRecipientTag,
                                   nsIP3PTag  *aCategoriesTag );

  NS_METHOD             CheckPref( nsString&   aRefDataSchemaURISpec,
                                   nsString&   aRefDataStructName,
                                   nsString&   aPurposeValue,
                                   nsIP3PTag  *aRecipientTag,
                                   nsIP3PTag  *aCategoriesTag );

  NS_METHOD             CheckPref( nsString&   aRefDataSchemaURISpec,
                                   nsString&   aRefDataStructName,
                                   nsString&   aPurposeValue,
                                   nsString&   aRecipientValue,
                                   nsIP3PTag  *aCategoriesTag );

  NS_METHOD             CheckPref( nsString&   aDataStructName,
                                   nsString&   aShortDescription,
                                   nsString&   aPurposeValue,
                                   nsString&   aRecipientValue,
                                   nsString&   aCategoryValue );

  NS_METHOD             SetFailedPrefsCombination( nsString&   aDataStructName,
                                                   nsString&   aShortDescription,
                                                   nsString&   aPurposeValue,
                                                   nsString&   aRecipientValue,
                                                   nsString&   aCategoryValue );


  nsCOMPtr<nsIRDFService>         mRDFService;             // The RDF Service

  nsCOMPtr<nsIRDFContainerUtils>  mRDFContainerUtils;      // The RDF Container Utils Service

  nsCOMPtr<nsIRDFResource>        mRDFResourceRDFType,     // Various RDF resources
                                  mRDFResourceP3PEntries,
                                  mRDFResourceP3PEntity,
                                  mRDFResourceP3PEntityInformation,
                                  mRDFResourceP3PPolicyURISpec,
                                  mRDFResourceP3PPolicyDescriptionURISpec,
                                  mRDFResourceP3PResult,
                                  mRDFResourceP3PDataTags,
                                  mRDFResourceP3PDataTag,
                                  mRDFResourceP3PDataCombination,
                                  mRDFResourceP3PTitle,
                                  mRDFResourceP3PValue;

  nsCOMPtr<nsIRDFDataSource>      mRDFDataSource;          // The Policy RDF datasource

  nsCOMPtr<nsIRDFResource>        mPolicyDescription,      // The Policy RDF resources
                                  mEntityDescription,
                                  mDataTagsDescription,
                                  mDataCombinationsDescription;

  nsCOMPtr<nsIRDFResource>        mPolicyContainer,        // The Policy RDF resource containers
                                  mEntityContainer,
                                  mDataTagsContainer,
                                  mDataCombinationsContainer;

  nsAutoString                    mRDFAbout;               // The Policy RDF resource name

  nsCOMPtr<nsIP3PTag>             mDataSchemaTag;          // The inline <DATASCHEMA> Tag object

  nsStringArray                   mDataSchemasToRead;      // The list of DataSchemas required for this DataSchema object

  nsSupportsHashtable             mDataSchemas;            // The collection of DataSchema objects created for this Policy object

  PRTime                          mPrefsTime;              // The last time this Policy was compared to the user's preferences

  PRInt32                         mPrefsResult;            // The result of the comparison to the user's preferences

  nsSupportsHashtable             mFailedDataTagRefs;      // The collection of <DATA> Tag objects that didn't meet the user's preferences
};

extern
NS_EXPORT NS_METHOD     NS_NewP3PPolicy( nsString&      aPolicyURISpec,
                                         nsIP3PPolicy **aPolicy );

PRBool PR_CALLBACK      AddRDFDataTagEnumerate( nsHashKey  *aHashKey,
                                                void       *aData,
                                                void       *aClosure );


class nsP3PPolicyDataTagResult : public nsISupports {
public:
  // nsISupports
  NS_DECL_ISUPPORTS

  // nsP3PServiceReadData methods
  nsP3PPolicyDataTagResult( );
  virtual ~nsP3PPolicyDataTagResult( );

  nsString              mDescription;                      // <DATA> Tag object name or "short-description"
  nsStringArray         mFailedPrefsCombinations;          // The list of preference combinations that didn't meet the user's preferences
};

extern
NS_EXPORT NS_METHOD     NS_NewP3PPolicyDataTagResult( nsString&     aDescription,
                                                      nsISupports **aPolicyDataTagResult );

#endif                                           /* nsP3Policy_h__            */
