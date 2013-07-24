/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_RegExpStatics_inl_h
#define vm_RegExpStatics_inl_h

#include "vm/RegExpStatics.h"

#include "jsinferinlines.h"

namespace js {

inline void
RegExpStatics::setMultiline(JSContext *cx, bool enabled)
{
    aboutToWrite();
    if (enabled) {
        flags = RegExpFlag(flags | MultilineFlag);
        markFlagsSet(cx);
    } else {
        flags = RegExpFlag(flags & ~MultilineFlag);
    }
}

inline void
RegExpStatics::markFlagsSet(JSContext *cx)
{
    /*
     * Flags set on the RegExp function get propagated to constructed RegExp
     * objects, which interferes with optimizations that inline RegExp cloning
     * or avoid cloning entirely. Scripts making this assumption listen to
     * type changes on RegExp.prototype, so mark a state change to trigger
     * recompilation of all such code (when recompiling, a stub call will
     * always be performed).
     */
    JS_ASSERT(this == cx->global()->getRegExpStatics());

    types::MarkTypeObjectFlags(cx, cx->global(), types::OBJECT_FLAG_REGEXP_FLAGS_SET);
}

inline void
RegExpStatics::reset(JSContext *cx, JSString *newInput, bool newMultiline)
{
    aboutToWrite();
    clear();
    pendingInput = newInput;
    setMultiline(cx, newMultiline);
    checkInvariants();
}

} /* namespace js */

#endif /* vm_RegExpStatics_inl_h */
