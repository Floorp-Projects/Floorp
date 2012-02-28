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
 * The Original Code is SpiderMonkey boolean object code.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Terrence Cole <tcole+code@mozilla.com> (original author)
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

#ifndef BooleanObject_h___
#define BooleanObject_h___

#include "mozilla/Attributes.h"

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

  private:
    BooleanObject() MOZ_DELETE;
    BooleanObject &operator=(const BooleanObject &bo) MOZ_DELETE;
};

} // namespace js

#endif /* BooleanObject_h__ */
