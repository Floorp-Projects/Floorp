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

#include "util.h"
#include "bcDefs.h"

size_t  type2size(bcXPType type) {
    size_t res = 0;
    switch (type) {
	case bc_T_CHAR : 
	    res = sizeof(char);
	    break;
	case bc_T_WCHAR:
	    res = 2; //nb 
	    break;
	case bc_T_I8:
	case bc_T_U8:
	    res = sizeof(PRInt8);
	    break;
	case bc_T_I16:
	case bc_T_U16:
	    res = sizeof(PRInt16);
	    break;
	case bc_T_I32:
	case bc_T_U32:
	    res = sizeof(PRInt32);
	    break;
	case bc_T_I64:
	case bc_T_U64:
	    res = sizeof(PRInt64);
	    break;
	case bc_T_FLOAT:
	    res = sizeof(float);
	    break;
	case bc_T_DOUBLE:
	    res = sizeof(double);
	    break;
	case bc_T_BOOL:
	    res = sizeof(PRBool);
	    break;
	case bc_T_IID:
	    res = sizeof(nsID);
	    break;
	case bc_T_INTERFACE:
	    res = sizeof(bcOID);
	    break;
	default:
	    res = 0;
    }
    return res;
}


