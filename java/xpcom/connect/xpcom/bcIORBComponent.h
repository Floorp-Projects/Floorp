/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Sun Microsystems,
 * Inc. Portions created by Sun are
 * Copyright (C) 1999 Sun Microsystems, Inc. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Igor Kushnirskiy <idk@eng.sun.com>
 */

#ifndef __bcIORBCOMPONENT_h__
#define __bcIORBCOMPONENT_h_
#include "nsISupports.h"

/*29bde10c-1dd2-11b2-ab23-ebe06c6baec5*/
#define BC_ORBCOMPONENT_IID \
   { 0x29bde10c, 0x1dd2, 0x11b2, \
    {0xab, 0x23, 0xeb, 0xe0, 0x6c, 0x6b, 0xae, 0xc5}}

class bcIORB;
class bcIORBComponent : public nsISupports {
 public:
    NS_DEFINE_STATIC_IID_ACCESSOR(BC_ORBCOMPONENT_IID) 
    NS_IMETHOD GetORB(bcIORB **orb) = 0;
};
#endif
