/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsPermission_h__
#define nsPermission_h__

#include "nsIPermission.h"
#include "nsWeakReference.h"

////////////////////////////////////////////////////////////////////////////////

class nsPermission : public nsIPermission,
                        public nsSupportsWeakReference {
public:

  // nsISupports
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPERMISSION

  // Note: following constructor takes ownership of the host string so the caller
  // of the constructor must not free them
  nsPermission(char * host, PRInt32 type, PRBool capability);
  nsPermission();
  virtual ~nsPermission(void);
  
protected:
  char * permissionHost;
  PRInt32 permissionType;
  PRBool permissionCapability;
};

#endif /* nsPermission_h__ */
