/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef nsCookieHTTPNotify_h___
#define nsCookieHTTPNotify_h___

#include "nsIFactory.h"
#include "nsIHttpNotify.h"

// {6BC1F522-1F45-11d3-8AD4-00105A1B8860}
#define NS_COOKIEHTTPNOTIFY_CID \
{ 0x6bc1f522, 0x1f45, 0x11d3, { 0x8a, 0xd4, 0x0, 0x10, 0x5a, 0x1b, 0x88, 0x60 } };


class nsCookieHTTPNotify : public nsIHTTPNotify {
public:

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIHttpNotify methods:
  NS_IMETHOD ModifyRequest(nsISupports *aContext);
  NS_IMETHOD AsyncExamineResponse(nsISupports *aContext);
   
  // nsCookieHTTPNotify methods:
  nsCookieHTTPNotify();
  virtual ~nsCookieHTTPNotify();
 
private:
   
};

class nsCookieHTTPNotifyFactory : public nsIFactory {
public:

  NS_DECL_ISUPPORTS

  // nsIFactory methods:
  NS_IMETHOD CreateInstance(nsISupports *aOuter, REFNSIID aIID, void **aResult);
  NS_IMETHOD LockFactory(PRBool aLock);

  // nsCookieHTTPNotifyFactory methods:
  nsCookieHTTPNotifyFactory(void);
  virtual ~nsCookieHTTPNotifyFactory(void);
};

extern NS_EXPORT nsresult NS_NewCookieHTTPNotify(nsIHTTPNotify** aHTTPNotify);

#endif /* nsCookieHTTPNotify_h___ */
