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

#ifndef StringObject_inl_h___
#define StringObject_inl_h___

#include "StringObject.h"

inline js::StringObject *
JSObject::asString()
{
    JS_ASSERT(isString());
    return static_cast<js::StringObject *>(const_cast<JSObject *>(this));
}

namespace js {

inline StringObject *
StringObject::create(JSContext *cx, JSString *str)
{
    JSObject *obj = NewBuiltinClassInstance(cx, &js_StringClass);
    if (!obj)
        return NULL;
    StringObject *strobj = obj->asString();
    if (!strobj->init(cx, str))
        return NULL;
    return strobj;
}

inline StringObject *
StringObject::createWithProto(JSContext *cx, JSString *str, JSObject &proto)
{
    JS_ASSERT(gc::FINALIZE_OBJECT2 == gc::GetGCObjectKind(JSCLASS_RESERVED_SLOTS(&js_StringClass)));
    JSObject *obj = NewObjectWithClassProto(cx, &js_StringClass, &proto, gc::FINALIZE_OBJECT2);
    if (!obj)
        return NULL;
    StringObject *strobj = obj->asString();
    if (!strobj->init(cx, str))
        return NULL;
    return strobj;
}

} // namespace js

#endif /* StringObject_inl_h__ */
