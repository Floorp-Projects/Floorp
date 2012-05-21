/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=78:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BooleanObject_h___
#define BooleanObject_h___

#include "jsbool.h"

namespace js {

class BooleanObject : public JSObject
{
    /* Stores this Boolean object's [[PrimitiveValue]]. */
    static const unsigned PRIMITIVE_VALUE_SLOT = 0;

  public:
    static const unsigned RESERVED_SLOTS = 1;

    /*
     * Creates a new Boolean object boxing the given primitive bool.  The
     * object's [[Prototype]] is determined from context.
     */
    static inline BooleanObject *create(JSContext *cx, bool b);

    /*
     * Identical to create(), but uses |proto| as [[Prototype]].  This method
     * must not be used to create |Boolean.prototype|.
     */
    static inline BooleanObject *createWithProto(JSContext *cx, bool b, JSObject &proto);

    bool unbox() const {
        return getFixedSlot(PRIMITIVE_VALUE_SLOT).toBoolean();
    }

  private:
    inline void setPrimitiveValue(bool b) {
        setFixedSlot(PRIMITIVE_VALUE_SLOT, BooleanValue(b));
    }

    /* For access to init, as Boolean.prototype is special. */
    friend JSObject *
    ::js_InitBooleanClass(JSContext *cx, JSObject *global);
};

} // namespace js

#endif /* BooleanObject_h__ */
