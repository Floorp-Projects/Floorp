/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
/* describes principals by their orginating uris*/
#ifndef _NS_JSPRINCIPALS_H_
#define _NS_JSPRINCIPALS_H_
#include "jsapi.h"
#include "nsIPrincipal.h"

struct nsJSPrincipals : JSPrincipals {
  static nsresult Startup();
  nsJSPrincipals();
  nsresult Init(char *prin);
  ~nsJSPrincipals(void);

  nsIPrincipal *nsIPrincipalPtr;
};

#endif /* _NS_JSPRINCIPALS_H_ */

