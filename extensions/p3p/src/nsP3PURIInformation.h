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

#ifndef nsP3PURIInformation_h__
#define nsP3PURIInformation_h__

#include "nsIP3PURIInformation.h"

#include <nsHashtable.h>


class nsP3PURIInformation : public nsIP3PURIInformation {
public:
  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIP3PURIInformation
  NS_DECL_NSIP3PURIINFORMATION

  // nsP3PReference methods
  nsP3PURIInformation( );
  virtual ~nsP3PURIInformation( );

  NS_METHOD             Init( nsString&  aURISpec );

protected:
  nsString         mURISpec;                          // The URI spec for this object
  nsCString        mcsURISpec;                        // ... for logging
  nsCOMPtr<nsIURI> mURI;                              // The URI object for this object

  nsString         mRefererHeader;                    // The Associated "Referer" request header

  nsStringArray    mPolicyRefFileURISpecs;            // The collection of PolicyRefFile URI specs

  nsHashtable      mPolicyRefFileReferencePoints;     // The collection of where a PolicyRefFile was referenced

  nsString         mSetCookieHeader;                  // The Associated "Set-Cookie" response header
};

extern
NS_EXPORT NS_METHOD     NS_NewP3PURIInformation( nsString&              aURISpec,
                                                 nsIP3PURIInformation **aURIInformation );

#endif                                           /* nsP3PURIInformation_h__   */
