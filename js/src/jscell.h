/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is SpiderMonkey code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Gregor Wagner <anygregor@gmail.com>
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

#ifndef jscell_h___
#define jscell_h___

struct JSCompartment;

namespace js {
namespace gc {

template <typename T> struct Arena;
struct ArenaBitmap;
struct MarkingDelay;
struct Chunk;
struct FreeCell;

/*
 * A GC cell is the base class for GC Things like JSObject, JSShortString,
 * JSFunction, JSXML and for an empty cell called FreeCell. It helps avoiding 
 * casts from an Object to a Cell whenever we call GC related mark functions.
 * Cell is not the base Class for JSString because static initialization
 * (used for unitStringTables) does not work with inheritance.
 */

struct Cell {
    static const size_t CellShift = 3;
    static const size_t CellSize = size_t(1) << CellShift;
    static const size_t CellMask = CellSize - 1;

    inline Arena<Cell> *arena() const;
    inline Chunk *chunk() const;
    inline ArenaBitmap *bitmap() const;
    JS_ALWAYS_INLINE size_t cellIndex() const;

    JS_ALWAYS_INLINE void mark(uint32 color) const;
    JS_ALWAYS_INLINE bool isMarked(uint32 color) const;
    JS_ALWAYS_INLINE bool markIfUnmarked(uint32 color) const;

    inline JSCompartment *compartment() const;

    /* Needed for compatibility reasons because Cell can't be a base class of JSString */
    JS_ALWAYS_INLINE js::gc::Cell *asCell() { return this; }

    JS_ALWAYS_INLINE js::gc::FreeCell *asFreeCell() {
        return reinterpret_cast<FreeCell *>(this);
    }
};

/* FreeCell has always size 8 */
struct FreeCell : Cell {
    union {
        FreeCell *link;
        double data;
    };
};

JS_STATIC_ASSERT(sizeof(FreeCell) == 8);

} /* namespace gc */
} /* namespace js */

#endif /* jscell_h___ */
