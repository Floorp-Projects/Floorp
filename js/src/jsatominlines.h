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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#ifndef jsatom_inlines_h___
#define jsatom_inlines_h___

#include "jsatom.h"
#include "jsnum.h"

/*
 * Convert v to an atomized string and wrap it as an id.
 */
inline JSBool
js_ValueToStringId(JSContext *cx, jsval v, jsid *idp)
{
    JSString *str;
    JSAtom *atom;

    /*
     * Optimize for the common case where v is an already-atomized string. The
     * comment in jsstr.h before JSString::flatSetAtomized explains why this is
     * thread-safe. The extra rooting via lastAtom (which would otherwise be
     * done in js_js_AtomizeString) ensures the caller that the resulting id at
     * is least weakly rooted.
     */
    if (JSVAL_IS_STRING(v)) {
        str = JSVAL_TO_STRING(v);
        if (str->isAtomized()) {
            cx->weakRoots.lastAtom = v;
            *idp = ATOM_TO_JSID((JSAtom *) v);
            return JS_TRUE;
        }
    } else {
        str = js_ValueToString(cx, v);
        if (!str)
            return JS_FALSE;
    }
    atom = js_AtomizeString(cx, str, 0);
    if (!atom)
        return JS_FALSE;
    *idp = ATOM_TO_JSID(atom);
    return JS_TRUE;
}

inline JSBool
js_Int32ToId(JSContext* cx, int32 index, jsid* id)
{
    if (INT_FITS_IN_JSVAL(index)) {
        *id = INT_TO_JSID(index);
        JS_ASSERT(INT_JSID_TO_JSVAL(*id) == INT_TO_JSVAL(index));
        return JS_TRUE;
    }
    JSString* str = js_NumberToString(cx, index);
    if (!str)
        return JS_FALSE;
    return js_ValueToStringId(cx, STRING_TO_JSVAL(str), id);
}

#endif /* jsatom_inlines_h___ */
