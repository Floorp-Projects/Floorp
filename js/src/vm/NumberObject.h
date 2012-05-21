/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=78:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NumberObject_h___
#define NumberObject_h___

#include "jsnum.h"

namespace js {

class NumberObject : public JSObject
{
    /* Stores this Number object's [[PrimitiveValue]]. */
    static const unsigned PRIMITIVE_VALUE_SLOT = 0;

  public:
    static const unsigned RESERVED_SLOTS = 1;

    /*
     * Creates a new Number object boxing the given number.  The object's
     * [[Prototype]] is determined from context.
     */
    static inline NumberObject *create(JSContext *cx, double d);

    /*
     * Identical to create(), but uses |proto| as [[Prototype]].  This method
     * must not be used to create |Number.prototype|.
     */
    static inline NumberObject *createWithProto(JSContext *cx, double d, JSObject &proto);

    double unbox() const {
        return getFixedSlot(PRIMITIVE_VALUE_SLOT).toNumber();
    }

  private:
    inline void setPrimitiveValue(double d) {
        setFixedSlot(PRIMITIVE_VALUE_SLOT, NumberValue(d));
    }

    /* For access to init, as Number.prototype is special. */
    friend JSObject *
    ::js_InitNumberClass(JSContext *cx, JSObject *global);
};

} // namespace js

#endif /* NumberObject_h__ */
