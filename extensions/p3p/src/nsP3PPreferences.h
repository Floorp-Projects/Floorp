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

#ifndef nsP3PPreferences_h__
#define nsP3PPreferences_h__

#include "nsIP3PPreferences.h"
#include "nsIP3PCService.h"

#include <nsIPref.h>
#include <nsHashtable.h>


class nsP3PPreferences : public nsIP3PPreferences {
public:
  // nsISupports methods
  NS_DECL_ISUPPORTS

  // nsIP3PPreferences methods
  NS_DECL_NSIP3PPREFERENCES

  // Contructor
  nsP3PPreferences( );

  // Destructor
  virtual ~nsP3PPreferences( );

  NS_METHOD                  Init( );

protected:
  PRBool                     mInitialized;

  PRBool                     mP3Penabled;  // Cached value of frequently used pref.
  nsSupportsHashtable *      mPrefCache;

  nsCOMPtr<nsIPref>          mPrefServ;
  nsCOMPtr<nsIP3PCService>   mP3PService;

  PRTime                     mTimePrefsLastChanged;
};

extern
NS_EXPORT NS_METHOD     NS_NewP3PPreferences( nsIP3PPreferences **aP3PPreferences );

// Preference change callback routine
PRInt32 PrefChangedCallback( const char  *aPrefName,
                             void        *aInstanceData );

#endif                                           /* nsP3PPreferences_h__      */
