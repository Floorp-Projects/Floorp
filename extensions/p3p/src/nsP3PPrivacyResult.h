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

#ifndef nsP3PPrivacyResult_h__
#define nsP3PPrivacyResult_h__

#include "nsIP3PPrivacyResult.h"
#include "nsIP3PCService.h"

#include <nsIRDFService.h>
#include <nsIRDFCompositeDataSource.h>
#include <nsIRDFResource.h>
#include <nsIRDFContainerUtils.h>


class nsP3PPrivacyResult : public nsIP3PPrivacyResult {
public:
  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIP3PPrivacyResult
  NS_DECL_NSIP3PPRIVACYRESULT

  // nsP3PPrivacyResult methods
  nsP3PPrivacyResult( );
  virtual ~nsP3PPrivacyResult( );

  NS_METHOD             Init( nsString&  aURISpec );

protected:
  NS_METHOD             AddRDFEntry( nsString&        aName,
                                     nsString&        aTitle,
                                     nsString&        aValue,
                                     nsIRDFResource  *aResourceType,
                                     nsIRDFResource  *aContainer,
                                     nsIRDFResource **aResource );


  nsString                             mURISpec;      // The URI spec associated with this object
  nsCString                            mcsURISpec;    // ... for logging

  PRBool                               mIndirectlyCreated;  // An indicator as whether or not this object was indirectly created

  nsCOMPtr<nsIRDFService>              mRDFService;   // The RDF Service

  nsCOMPtr<nsIRDFContainerUtils>       mRDFContainerUtils;  // The RDF Container Utilities Service

  nsCOMPtr<nsIRDFResource>             mRDFResourceRDFType, // Variouse RDF resources
                                       mRDFResourceP3PEntries,
                                       mRDFResourceP3PPolicy,
                                       mRDFResourceP3PTitle,
                                       mRDFResourceP3PValue;

  nsCOMPtr<nsIP3PCService>             mP3PService;   // The P3P Service

  nsCOMPtr<nsIP3PPolicy>               mPolicy;       // The Policy object associated with this object

  nsresult                             mPrivacyResult;         // The result of comparison between the Policy object and the user's preferences
  nsresult                             mChildrenPrivacyResult; // The privacy result of all the objects children
  nsresult                             mOverallPrivacyResult;  // The overall privacy result combining this object and it's children

  nsCOMPtr<nsIRDFCompositeDataSource>  mRDFCombinedDataSource; // The data source used to combine this objects data source and it's child data sources

  nsCOMPtr<nsIRDFDataSource>           mRDFDataSource;  // The data source used to describe this objects privacy information

  nsCOMPtr<nsIRDFResource>             mRootDescription,
                                       mDescription;

  nsCOMPtr<nsIRDFResource>             mRootContainer,
                                       mContainer;

  nsAutoString                         mRDFAbout;     // The PrivacyResult RDF resource name

  nsCOMPtr<nsIP3PPrivacyResult>        mParent;       // The parent PrivacyResult object

  nsCOMPtr<nsISupportsArray>           mChildren;     // The children PrivacyResult objects
};

extern
NS_EXPORT NS_METHOD     NS_NewP3PPrivacyResult( nsString&             aURISpec,
                                                nsIP3PPrivacyResult **aPrivacyResult );

#endif                                           /* nsP3PPrivacyResult_h__    */
