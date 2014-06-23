/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_Symbol_h
#define vm_Symbol_h

#include "mozilla/Attributes.h"

#include "gc/Barrier.h"

#include "js/RootingAPI.h"
#include "js/TypeDecls.h"

namespace JS {

class Symbol : public js::gc::BarrieredCell<Symbol>
{
  private:
    uint32_t unused1_;  // This field will be used before long.
    JSAtom *description_;

    // The minimum allocation size is sizeof(JSString): 16 bytes on 32-bit
    // architectures and 24 bytes on 64-bit.  8 bytes of padding makes Symbol
    // the minimum size on both.
    uint64_t unused2_;

    explicit Symbol(JSAtom *desc) : description_(desc) {}
    Symbol(const Symbol &) MOZ_DELETE;
    void operator=(const Symbol &) MOZ_DELETE;

  public:
    static Symbol *new_(js::ExclusiveContext *cx, JSString *description);

    JSAtom *description() const { return description_; }

    static inline js::ThingRootKind rootKind() { return js::THING_ROOT_SYMBOL; }
    inline void markChildren(JSTracer *trc);
    inline void finalize(js::FreeOp *) {}
};

} /* namespace JS */

#endif /* vm_Symbol_h */
