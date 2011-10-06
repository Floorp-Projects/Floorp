/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   John Bandhauer <jband@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* local header for xpconnect tests components */

#ifndef xpctest_private_h___
#define xpctest_private_h___

#include "nsISupports.h"
#include "nsMemory.h"
#include "jsapi.h"
#include "nsStringGlue.h"
#include "xpctest_attributes.h"
#include "xpctest_params.h"

class xpcTestObjectReadOnly : public nsIXPCTestObjectReadOnly {
 public: 
  NS_DECL_ISUPPORTS
  NS_DECL_NSIXPCTESTOBJECTREADONLY
  xpcTestObjectReadOnly();
   
 private:
    bool    boolProperty;
    PRInt16 shortProperty;
    PRInt32 longProperty;
    float   floatProperty;
    char    charProperty;
};

class xpcTestObjectReadWrite : public nsIXPCTestObjectReadWrite {
  public: 
  NS_DECL_ISUPPORTS
  NS_DECL_NSIXPCTESTOBJECTREADWRITE

  xpcTestObjectReadWrite();
  ~xpcTestObjectReadWrite();

 private:
     bool boolProperty;
     PRInt16 shortProperty;
     PRInt32 longProperty;
     float floatProperty;
     char charProperty;
     char *stringProperty;
};

class nsXPCTestParams : public nsIXPCTestParams
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIXPCTESTPARAMS

    nsXPCTestParams();

private:
    ~nsXPCTestParams();
};

#endif /* xpctest_private_h___ */
