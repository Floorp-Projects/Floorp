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

#ifndef _bcORB_h
#define _bcORB_h
#include "nsISupports.h"
#include "bcIORB.h"


/*29bde10c-1dd2-11b2-ab23-ebe06c6baec5*/
#define BC_ORB_IID \
   { 0x29bde10c, 0x1dd2, 0x11b2, \
    {0xab, 0x23, 0xeb, 0xe0, 0x6c, 0x6b, 0xae, 0xc5}}


#define BC_ORB_PROGID "component://netscape/blackwood/blackconnect/orb"

/*ffa0d768-1dd1-11b2-8bf2-ab56f26ea844*/
#define BC_ORB_CID \
   { 0xffa0d768, 0x1dd1, 0x11b2, \
     {0x8b, 0xf2, 0xab, 0x56, 0xf2, 0x6e, 0xa8, 0x44 }}


class bcORB : public nsISupports {
    NS_DECL_ISUPPORTS
    NS_DEFINE_STATIC_IID_ACCESSOR(BC_ORB_IID) 
    NS_IMETHOD GetORB(bcIORB **orb);
    bcORB();
    virtual ~bcORB();
 private:
    bcIORB *orb;
};

#endif
 
