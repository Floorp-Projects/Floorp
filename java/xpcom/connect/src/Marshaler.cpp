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

#include <iostream.h>

#include <string.h>
#include "Marshaler.h"
#include "util.h"

Marshaler::Marshaler(ostream *_out) {
    out = _out;
}

Marshaler::~Marshaler() {
}

int Marshaler::WriteSimple(void *ptr, bcXPType type) {
    out->write((const char*)ptr, type2size(type));
    return 0;
}

int Marshaler::WriteString(void *ptr, size_t size) { 
    if (!size 
        && ptr) {
        size = 1;
    }
    WriteArray(ptr,size, bc_T_CHAR);
    return 0;
}

int Marshaler::WriteArray(void *ptr, size_t length, bcXPType type) {
    if (!ptr) {
        length = 0;
    }
    cout<<"Marshaler::WriteArray("<<length<<")\n";
    out->write((const char*)&length, sizeof(size_t));
    switch (type) {
        case bc_T_CHAR_STR:
        case bc_T_WCHAR_STR:
            {
                for (unsigned int i = 0; i < length; i++) {
                    char *str = ((char**)ptr)[i];
                    size_t size = (!str) ? 0 : strlen(str)+1; //we want to store \0
                    WriteString((void *)str, size);
                }
                break;
            }
        default:
            if (length) {
                out->write((const char*)ptr,type2size(type)*length);
            }
    }
    return 0;
}







