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

#ifndef nsCookie_h__
#define nsCookie_h__

#include "nsICookie.h"
#include "nsWeakReference.h"

////////////////////////////////////////////////////////////////////////////////

class nsCookie : public nsICookie,
                        public nsSupportsWeakReference {
public:

  // nsISupports
  NS_DECL_ISUPPORTS
  NS_DECL_NSICOOKIE

  // Note: following constructor takes ownership of the four strings (name, value
  //       host, and path) passed to it so the caller of the constructor must not
  //       free them
  nsCookie
    (char * name,
     char * value,
     PRBool isDomain,
     char * host,
     char * path,
     PRBool isSecure,
     PRUint64 expires);
  nsCookie();
  virtual ~nsCookie(void);
  
protected:
  char * cookieName;
  char * cookieValue;
  PRBool cookieIsDomain;
  char * cookieHost;
  char * cookiePath;
  PRBool cookieIsSecure;
  PRUint64 cookieExpires;
};

#endif /* nsCookie_h__ */
