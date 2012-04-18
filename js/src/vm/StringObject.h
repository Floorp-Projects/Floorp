/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=78:
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
 * The Original Code is SpiderMonkey string object code.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jeff Walden <jwalden+code@mit.edu> (original author)
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

#ifndef StringObject_h___
#define StringObject_h___

#include "mozilla/Attributes.h"

#include "jsobj.h"
#include "jsstr.h"

namespace js {

class StringObject : public JSObject
{
    static const unsigned PRIMITIVE_VALUE_SLOT = 0;
    static const unsigned LENGTH_SLOT = 1;

  public:
    static const unsigned RESERVED_SLOTS = 2;

    /*
     * Creates a new String object boxing the given string.  The object's
     * [[Prototype]] is determined from context.
     */
    static inline StringObject *create(JSContext *cx, HandleString str);

    /*
     * Identical to create(), but uses |proto| as [[Prototype]].  This method
     * must not be used to create |String.prototype|.
     */
    static inline StringObject *createWithProto(JSContext *cx, HandleString str, JSObject &proto);

    JSString *unbox() const {
        return getFixedSlot(PRIMITIVE_VALUE_SLOT).toString();
    }

    inline size_t length() const {
        return size_t(getFixedSlot(LENGTH_SLOT).toInt32());
    }

    static size_t getPrimitiveValueOffset() {
        return getFixedSlotOffset(PRIMITIVE_VALUE_SLOT);
    }

  private:
    inline bool init(JSContext *cx, HandleString str);

    void setStringThis(JSString *str) {
        JS_ASSERT(getReservedSlot(PRIMITIVE_VALUE_SLOT).isUndefined());
        setFixedSlot(PRIMITIVE_VALUE_SLOT, StringValue(str));
        setFixedSlot(LENGTH_SLOT, Int32Value(int32_t(str->length())));
    }

    /* For access to init, as String.prototype is special. */
    friend JSObject *
    ::js_InitStringClass(JSContext *cx, JSObject *global);

    /*
     * Compute the initial shape to associate with fresh String objects, which
     * encodes the initial length property. Return the shape after changing
     * this String object's last property to it.
     */
    Shape *assignInitialShape(JSContext *cx);

  private:
    StringObject() MOZ_DELETE;
    StringObject &operator=(const StringObject &so) MOZ_DELETE;
};

} // namespace js

#endif /* StringObject_h__ */
