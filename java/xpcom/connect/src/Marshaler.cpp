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
    out->write((const char*)&size, sizeof(size_t));
    if (size) {
        out->write((const char*)ptr,type2size(bc_T_CHAR)*size);
    }
    
    return 0;
}
