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
 * Copyright (C) 1998-1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#ifndef _NS_SCRIPT_SECURITY_MANAGER_H_
#define _NS_SCRIPT_SECURITY_MANAGER_H_

#include "nsIScriptSecurityManager.h"
#include "nsIPrincipal.h"
#include "nsIURI.h"
#include "jsapi.h"
#include "jsdbgapi.h"
#include "nsIScriptContext.h"

#define NS_SCRIPTSECURITYMANAGER_CID \
{ 0x7ee2a4c0, 0x4b93, 0x17d3, \
{ 0xba, 0x18, 0x00, 0x60, 0xb0, 0xf1, 0x99, 0xa2 }}

class nsScriptSecurityManager : public nsIScriptSecurityManager {
public:
  nsScriptSecurityManager();
  virtual ~nsScriptSecurityManager();
  
  NS_DEFINE_STATIC_CID_ACCESSOR(NS_SCRIPTSECURITYMANAGER_CID)
  
  NS_DECL_ISUPPORTS
  NS_DECL_NSISCRIPTSECURITYMANAGER
  
  static nsScriptSecurityManager *
  GetScriptSecurityManager();

private:
  char * GetCanonicalizedOrigin(JSContext *cx, const char* aUrlString);
  NS_IMETHOD GetOriginFromSourceURL(nsIURI * origin, char * * result);
  PRBool SameOrigins(JSContext *aCx, const char* aOrigin1, const char* aOrigin2);
  PRInt32 CheckForPrivilege(JSContext *cx, char *prop_name, int priv_code);
  char* FindOriginURL(JSContext *aCx, JSObject *aGlobal);
  char* AddSecPolicyPrefix(JSContext *cx, char *pref_str);
  char* GetSitePolicy(const char *org);
};
#endif /*_NS_SCRIPT_SECURITY_MANAGER_H_*/
