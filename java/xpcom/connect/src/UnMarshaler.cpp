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

#include "UnMarshaler.h"
#include "util.h"

UnMarshaler::UnMarshaler(istream *_in) {
    in = _in;
}

UnMarshaler::~UnMarshaler() {
}

int UnMarshaler::ReadSimple(void *ptr, bcXPType type) {
    char *p = (char *)ptr;
    int size = type2size(type);
    in->read(p,size );
    return 0;
}
int UnMarshaler::ReadString(void *ptr, size_t *size, bcIAllocator * allocator) {
    ReadArray(ptr, size, bc_T_CHAR, allocator);
    if (*size == 1) {
        if (!(*(char**)ptr)[0]) {
            *size = 0;
        }
    }
    return 0;
}

int UnMarshaler::ReadArray(void *ptr, size_t *length, bcXPType type, bcIAllocator * allocator) {
    in->read((char*)length,sizeof(size_t));
    cout<<"UnMarshaler *length "<<*length<<"\n";

    if (!length) {
        ptr = 0;
    }
    switch (type) {
        case bc_T_CHAR_STR:
        case bc_T_WCHAR_STR:
            {
                char **strArray = *(char***)ptr;
                *strArray = (char*)allocator->Alloc(*length * sizeof(char*));
                
                for (unsigned int i = 0; i < *length; i++) {
                    char * str;
                    size_t size;
                    ReadString((void*)&str, &size, allocator);
                    strArray[i] = str;
                }
                break;
            }
        default:
            char *p = *(char**)ptr = (char *)allocator->Alloc(*length * type2size(type));
            if (*length) {
                in->read(p,*length * type2size(type));
            }
    }
    return 0;
}



















































