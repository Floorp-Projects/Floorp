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

#ifndef nsP3PReference_h__
#define nsP3PReference_h__

#include "nsIP3PReference.h"

#include <nsAutoLock.h>

#include <nsHashtable.h>
#include <nsSupportsArray.h>


class nsP3PReference : public nsIP3PReference {
public:
  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIP3PReference
  NS_DECL_NSIP3PREFERENCE

  // nsP3PReference methods
  nsP3PReference( );
  virtual ~nsP3PReference( );

  NS_METHOD             Init( nsString&  aHost );

protected:
  nsString              mHostPort;                    // The host name and port combination that this object represents
  nsCString             mcsHostPort;                  // ...for logging

  PRLock               *mLock;                        // A lock to serialize on PolicyRefFile object access

  nsStringArray         mPolicyRefFileURISpecs;       // List of PolicyRefFile URI specs associated with this object
  nsSupportsHashtable   mPolicyRefFiles;              // Collection of PolicyRefFile objects associated with this object
};

extern
NS_EXPORT NS_METHOD     NS_NewP3PReference( nsString&         aHost,
                                            nsIP3PReference **aReference );

#endif                                           /* nsP3PReference_h__        */
