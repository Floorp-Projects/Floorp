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
#include "jsobj.h"
#include "jstypedarray.h"

#include "jsobjinlines.h"

inline uint32_t
js::ArrayBufferObject::byteLength() const
{
    JS_ASSERT(isArrayBuffer());
    return getElementsHeader()->length;
}

inline uint8_t *
js::ArrayBufferObject::dataPointer() const
{
    return (uint8_t *) elements;
}

inline js::ArrayBufferObject &
JSObject::asArrayBuffer()
{
    JS_ASSERT(isArrayBuffer());
    return *static_cast<js::ArrayBufferObject *>(this);
}

inline js::DataViewObject &
JSObject::asDataView()
{
    JS_ASSERT(isDataView());
    return *static_cast<js::DataViewObject *>(this);
}

namespace js {

inline bool
ArrayBufferObject::hasData() const
{
    return getClass() == &ArrayBufferClass;
}

static inline int32_t
ClampIntForUint8Array(int32_t x)
{
    if (x < 0)
        return 0;
    if (x > 255)
        return 255;
    return x;
}

inline uint32_t
TypedArray::getLength(JSObject *obj) {
    JS_ASSERT(obj->isTypedArray());
    return obj->getFixedSlot(FIELD_LENGTH).toInt32();
}

inline uint32_t
TypedArray::getByteOffset(JSObject *obj) {
    JS_ASSERT(obj->isTypedArray());
    return obj->getFixedSlot(FIELD_BYTEOFFSET).toInt32();
}

inline uint32_t
TypedArray::getByteLength(JSObject *obj) {
    JS_ASSERT(obj->isTypedArray());
    return obj->getFixedSlot(FIELD_BYTELENGTH).toInt32();
}

inline uint32_t
TypedArray::getType(JSObject *obj) {
    JS_ASSERT(obj->isTypedArray());
    return obj->getFixedSlot(FIELD_TYPE).toInt32();
}

inline ArrayBufferObject *
TypedArray::getBuffer(JSObject *obj) {
    JS_ASSERT(obj->isTypedArray());
    return &obj->getFixedSlot(FIELD_BUFFER).toObject().asArrayBuffer();
}

inline void *
TypedArray::getDataOffset(JSObject *obj) {
    JS_ASSERT(obj->isTypedArray());
    return (void *)obj->getPrivate(NUM_FIXED_SLOTS);
}

inline DataViewObject *
DataViewObject::create(JSContext *cx, uint32_t byteOffset, uint32_t byteLength,
                       Handle<ArrayBufferObject*> arrayBuffer)
{
    JS_ASSERT(byteOffset <= INT32_MAX);
    JS_ASSERT(byteLength <= INT32_MAX);

    RootedVarObject obj(cx, NewBuiltinClassInstance(cx, &DataViewClass));
    if (!obj)
        return NULL;

    JS_ASSERT(arrayBuffer->isArrayBuffer());

    DataViewObject &dvobj = obj->asDataView();
    dvobj.setFixedSlot(BYTEOFFSET_SLOT, Int32Value(byteOffset));
    dvobj.setFixedSlot(BYTELENGTH_SLOT, Int32Value(byteLength));
    dvobj.setFixedSlot(BUFFER_SLOT, ObjectValue(*arrayBuffer));
    dvobj.setPrivate(arrayBuffer->dataPointer() + byteOffset);

    JS_ASSERT(dvobj.numFixedSlots() == RESERVED_SLOTS);

    return &dvobj;
}

inline uint32_t
DataViewObject::byteLength()
{
    JS_ASSERT(isDataView());
    int32_t length = getReservedSlot(BYTELENGTH_SLOT).toInt32();
    JS_ASSERT(length >= 0);
    return static_cast<uint32_t>(length);
}

inline uint32_t
DataViewObject::byteOffset()
{
    JS_ASSERT(isDataView());
    int32_t offset = getReservedSlot(BYTEOFFSET_SLOT).toInt32();
    JS_ASSERT(offset >= 0);
    return static_cast<uint32_t>(offset);
}

inline void *
DataViewObject::dataPointer()
{
    JS_ASSERT(isDataView());
    return getPrivate();
}

inline JSObject &
DataViewObject::arrayBuffer()
{
    JS_ASSERT(isDataView());
    return getReservedSlot(BUFFER_SLOT).toObject();
}

inline bool
DataViewObject::hasBuffer() const
{
    JS_ASSERT(isDataView());
    return getReservedSlot(BUFFER_SLOT).isObject();
}

} /* namespace js */

#endif /* jstypedarrayinlines_h */
