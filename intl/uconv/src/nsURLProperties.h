/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef nsURLProperties_h__
#define nsURLProperties_h__

#include "nsIPersistentProperties2.h"
#include "nsString.h"
#include "nsIIOService.h"

class nsURLProperties {
public:
  nsURLProperties(nsString& aUrl);
  virtual ~nsURLProperties();

  NS_IMETHOD DidLoad(PRBool&);
  NS_IMETHOD Get( const nsString& aKey, nsString& value);

private:
  static nsIIOService*  gIOService;    // to speed up creating URLs
  static nsrefcnt       gRefCnt;

  nsIPersistentProperties *mDelegate;

};

#endif /* nsURLProperties_h__ */

