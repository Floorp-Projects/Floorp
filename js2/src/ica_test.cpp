/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is the JavaScript 2 Prototype.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

#include <stdio.h>
#include <string>

#include "icodeasm.h"

int
main (int , char **)
{
    JavaScript::ICodeASM::ICodeParser icp;
    bool b;
    string str;
    
#define BOOL_TEST(s, expect)                                                \
    {                                                                       \
        b = false;                                                          \
        str = s;                                                            \
        icp.ParseBool (str.begin(), str.end(), &b);                         \
        if (b == expect)                                                    \
            fprintf (stderr, "PASS: ");                                     \
        else                                                                \
            fprintf (stderr, "FAIL: ");                                     \
        fprintf (stderr, "string %s bool parsed as %i\n", str.c_str(), b);  \
    }                                                                       \

    BOOL_TEST ("true", true);
    BOOL_TEST ("True", true);
    BOOL_TEST ("tRue", true);
    BOOL_TEST ("TRUE", true);
    BOOL_TEST ("True", true);
    BOOL_TEST ("false", false);
    BOOL_TEST ("False", false); 
    BOOL_TEST ("fAlSe", false);
    BOOL_TEST ("FALSE", false);
    BOOL_TEST ("False", false);

    return 0;
}

    

