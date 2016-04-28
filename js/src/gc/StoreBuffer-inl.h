/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gc_StoreBuffer_inl_h
#define gc_StoreBuffer_inl_h

namespace js {
namespace gc {

inline void
StoreBuffer::putWholeCell(Cell* cell)
{
    MOZ_ASSERT(cell->isTenured());

    if (cell->getTraceKind() == JS::TraceKind::Object) {
        JSObject *obj = static_cast<JSObject*>(cell);
        if (obj->is<NativeObject>())
            obj->as<NativeObject>().setInWholeCellBuffer();
    }

    put(bufferWholeCell, WholeCellEdges(cell));
}

} // namespace gc
} // namespace js

#endif // gc_StoreBuffer_inl_h
