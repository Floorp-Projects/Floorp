/* -*- Mode: c++; c-basic-offset: 4; tab-width: 40; indent-tabs-mode: nil -*- */
/* vim: set ts=40 sw=4 et tw=99: */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla WebGL impl
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Nikhil Marathe <nsm.nikhil@gmail.com>
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

#ifndef jstypedarrayinlines_h
#define jstypedarrayinlines_h

#include "jsapi.h"
#include "jsvalue.h"
#include "jsobj.h"

inline uint32
JSObject::arrayBufferByteLength()
{
    JS_ASSERT(isArrayBuffer());
    return *((uint32*) slots);
}

inline uint8 *
JSObject::arrayBufferDataOffset()
{
    uint64 *base = ((uint64*)slots) + 1;
    return (uint8*) base;
}

namespace js {

static inline int32
ClampIntForUint8Array(int32 x)
{
    if (x < 0)
        return 0;
    if (x > 255)
        return 255;
    return x;
}

inline JSUint32
TypedArray::getLength(JSObject *obj) {
    return obj->getFixedSlot(FIELD_LENGTH).toInt32();
}

inline JSUint32
TypedArray::getByteOffset(JSObject *obj) {
    return obj->getFixedSlot(FIELD_BYTEOFFSET).toInt32();
}

inline JSUint32
TypedArray::getByteLength(JSObject *obj) {
    return obj->getFixedSlot(FIELD_BYTELENGTH).toInt32();
}

inline JSUint32
TypedArray::getType(JSObject *obj) {
    return obj->getFixedSlot(FIELD_TYPE).toInt32();
}

inline JSObject *
TypedArray::getBuffer(JSObject *obj) {
    return &obj->getFixedSlot(FIELD_BUFFER).toObject();
}

inline void *
TypedArray::getDataOffset(JSObject *obj) {
    return (void *)obj->getPrivate();
}

}
#endif /* jstypedarrayinlines_h */
