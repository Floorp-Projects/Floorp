/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#ifndef nsJSSecurityManager_h___
#define nsJSSecurityManager_h___

#include "nsIScriptSecurityManager.h"
#include "nsIScriptContext.h"
#include "jsapi.h"

class nsIPref;

class nsJSSecurityManager : public nsIScriptSecurityManager {
public:
  nsJSSecurityManager();
  virtual ~nsJSSecurityManager();

  NS_DECL_ISUPPORTS

  NS_IMETHOD Init();

  NS_IMETHOD CheckScriptAccess(nsIScriptContext* aContext, 
                               void* aObj, 
                               const char* aProp, 
                               PRBool* aResult);

private:
  //Helper funcs
  char* AddSecPolicyPrefix(JSContext *cx, char *pref_str);
  char* GetSitePolicy(const char *org);
  JSBool CheckForPrivilege(JSContext *cx, char *prop_name, int priv_code);
  JSBool ContinueOnViolation(JSContext *cx, int pref_code);
  JSBool CheckForPrivilegeContinue(JSContext *cx, char *prop_name, int priv_code, int pref_code);

  //XXX temporarily
  char * ParseURL (const char *url, int parts_requested);
  char * SACopy (char *destination, const char *source);
  char * SACat (char *destination, const char *source);

  //Local vars
  nsIPref* mPrefs;
};


#define NS_SECURITY_FLAG_SAME_ORIGINS 0x0001
#define NS_SECURITY_FLAG_NO_ACCESS    0x0002
#define NS_SECURITY_FLAG_READ_ONLY    0x0004
//xxx break into privilege levels
#define NS_SECURITY_FLAG_SIGNED       0x0008

//XXX temporarily bit flags for determining what we want to parse from the URL
#define GET_ALL_PARTS               127
#define GET_PASSWORD_PART           64
#define GET_USERNAME_PART           32
#define GET_PROTOCOL_PART           16
#define GET_HOST_PART               8
#define GET_PATH_PART               4
#define GET_HASH_PART               2
#define GET_SEARCH_PART             1

#endif /* nsJSSecurityManager_h___ */
