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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Roy Yokoyama <yokoyama@netscape.com>
 */

#ifndef nsFontPackageHandler_h__
#define nsFontPackageHandler_h__

#include "nsString.h"

class nsFontPackageHandler : public nsIFontPackageHandler
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIFONTPACKAGEHANDLER

  nsFontPackageHandler();
  virtual ~nsFontPackageHandler();

private:
  nsresult CreateURLString(const char *aFontPackID, char **aURL);
};


#endif  /* nsFontPackageHandler_h__ */
