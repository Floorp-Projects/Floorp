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

#ifndef nsIP3PPreferences_h__
#define nsIP3PPreferences_h__

#include <nsCOMPtr.h>
#include <nsISupports.h>

#include <nsString.h>

#define NS_IP3PPREFERENCES_IID_STR "31430e57-d43d-11d3-9781-002035aee991"
#define NS_IP3PPREFERENCES_IID     {0x31430e57, 0xd43d, 0x11d3, { 0x97, 0x81, 0x00, 0x20, 0x35, 0xae, 0xe9, 0x91 }}

class nsIP3PPreferences : public nsISupports {
public:
  NS_DEFINE_STATIC_IID_ACCESSOR( NS_IP3PPREFERENCES_IID )

  NS_IMETHOD           PrefChanged( const char  *aPrefName = 0 ) = 0;
  NS_IMETHOD           GetTimePrefsLastChanged( PRTime *aResult ) = 0;
  NS_IMETHOD_( PRBool) PrefsChanged( PRTime time ) = 0;
  NS_IMETHOD           GetBoolPref( const char * aPrefName,
                                    PRBool *     aResult) = 0;
  NS_IMETHOD           GetBoolPref( nsString  aCategory,
                                    nsString  aPurpose,
                                    nsString  aRecipient,
                                    PRBool *  aResult ) = 0;
  NS_IMETHOD           GetBoolPref( nsCString aCategory,
                                    nsCString aPurpose,
                                    nsCString aRecipient,
                                    PRBool *  aResult ) = 0;
};

#define NS_DECL_NSIP3PPREFERENCES                                      \
  NS_IMETHOD           PrefChanged( const char  *aPrefName = 0 );      \
  NS_IMETHOD           GetTimePrefsLastChanged( PRTime *aResult );     \
  NS_IMETHOD_( PRBool) PrefsChanged( PRTime time );                    \
  NS_IMETHOD           GetBoolPref( const char * aPrefName,            \
                                    PRBool *     aResult);             \
  NS_IMETHOD           GetBoolPref( nsString  aCategory,               \
                                    nsString  aPurpose,                \
                                    nsString  aRecipient,              \
                                    PRBool *  aResult );               \
  NS_IMETHOD           GetBoolPref( nsCString aCategory,               \
                                    nsCString aPurpose,                \
                                    nsCString aRecipient,              \
                                    PRBool *  aResult );

#endif             /* nsIP3PPreferences_h___            */
