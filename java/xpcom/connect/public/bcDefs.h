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
#ifndef __bcDefs_h
#define __bcDefs_h
#include "prtypes.h"
#include "nsID.h"

enum bcXPType { 
    bc_T_I8 = 1, bc_T_U8, bc_T_I16, bc_T_U16,
    bc_T_I32, bc_T_U32, bc_T_I64, bc_T_U64, 
    bc_T_FLOAT, bc_T_DOUBLE, bc_T_BOOL, 
    bc_T_CHAR, bc_T_WCHAR,  
    bc_T_IID ,
    bc_T_CHAR_STR, bc_T_WCHAR_STR,
    bc_T_ARRAY,
    bc_T_INTERFACE,
    bc_T_UNDEFINED
};
    
typedef long bcOID;
typedef nsID bcIID;
typedef long bcTID ;

typedef unsigned int bcMID;
typedef unsigned int size_t;

#endif


































