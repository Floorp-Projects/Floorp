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
    size_t length;
    *(char**)ptr = NULL;
    in->read((char*)size,sizeof(size_t));
    if (*size) {
        *(char**)ptr = (char *)allocator->Alloc(*size * type2size(bc_T_CHAR));
        in->read(*(char**)ptr,*size * type2size(bc_T_CHAR));
    }
    if (*size == 1) {
        if (!(*(char**)ptr)[0]) {
            *size = 0;
        }
    }
    return 0;
}
