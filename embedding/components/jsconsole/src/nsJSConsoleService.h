/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef __nsJSConsoleService_h
#define __nsJSConsoleService_h

// {35807600-35bd-11d5-bb6f-b9f2e9fee03c}
#define NS_JSCONSOLESERVICE_CID \
 { 0x35807600, 0x35bd, 0x11d5, {0xbb, 0x6f, 0xb9, 0xf2, 0xe9, 0xfe, 0xe0, 0x3c}}
#define NS_JSCONSOLESERVICE_CONTRACTID \
 "@mozilla.org/embedcomp/jsconsole-service;1"

#include "nsIJSConsoleService.h"

class nsIDOMWindow;

class nsJSConsoleService: public nsIJSConsoleService
{
public:

  nsJSConsoleService();
  virtual ~nsJSConsoleService();

  NS_DECL_NSIJSCONSOLESERVICE
  NS_DECL_ISUPPORTS

};


#endif

