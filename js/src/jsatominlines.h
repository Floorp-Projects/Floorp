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

#ifndef jsatominlines_h___
#define jsatominlines_h___

#include "mozilla/RangedPtr.h"

#include "jsatom.h"
#include "jsnum.h"
#include "jsobj.h"

inline bool
js_ValueToAtom(JSContext *cx, const js::Value &v, JSAtom **atomp)
{
    if (!v.isString()) {
        JSString *str = js_ValueToString(cx, v);
        if (!str)
            return false;
        JS::Anchor<JSString *> anchor(str);
        *atomp = js_AtomizeString(cx, str);
        return !!*atomp;
    }

    JSString *str = v.toString();
    if (str->isAtom()) {
        *atomp = &str->asAtom();
        return true;
    }

    *atomp = js_AtomizeString(cx, str);
    return !!*atomp;
}

inline bool
js_ValueToStringId(JSContext *cx, const js::Value &v, jsid *idp)
{
    JSAtom *atom;
    if (js_ValueToAtom(cx, v, &atom)) {
        *idp = ATOM_TO_JSID(atom);
        return true;
    }
    return false;
}

inline bool
js_InternNonIntElementId(JSContext *cx, JSObject *obj, const js::Value &idval,
                         jsid *idp)
{
    JS_ASSERT(!idval.isInt32() || !INT_FITS_IN_JSID(idval.toInt32()));

#if JS_HAS_XML_SUPPORT
    extern bool js_InternNonIntElementIdSlow(JSContext *, JSObject *,
                                             const js::Value &, jsid *);
    if (idval.isObject())
        return js_InternNonIntElementIdSlow(cx, obj, idval, idp);
#endif

    return js_ValueToStringId(cx, idval, idp);
}

inline bool
js_InternNonIntElementId(JSContext *cx, JSObject *obj, const js::Value &idval,
                         jsid *idp, js::Value *vp)
{
    JS_ASSERT(!idval.isInt32() || !INT_FITS_IN_JSID(idval.toInt32()));

#if JS_HAS_XML_SUPPORT
    extern bool js_InternNonIntElementIdSlow(JSContext *, JSObject *,
                                             const js::Value &,
                                             jsid *, js::Value *);
    if (idval.isObject())
        return js_InternNonIntElementIdSlow(cx, obj, idval, idp, vp);
#endif

    JSAtom *atom;
    if (js_ValueToAtom(cx, idval, &atom)) {
        *idp = ATOM_TO_JSID(atom);
        vp->setString(atom);
        return true;
    }
    return false;
}

inline bool
js_Int32ToId(JSContext* cx, int32 index, jsid* id)
{
    if (INT_FITS_IN_JSID(index)) {
        *id = INT_TO_JSID(index);
        return true;
    }

    JSString* str = js_NumberToString(cx, index);
    if (!str)
        return false;

    return js_ValueToStringId(cx, js::StringValue(str), id);
}

namespace js {

/*
 * Write out character representing |index| to the memory just before |end|.
 * Thus |*end| is not touched, but |end[-1]| and earlier are modified as
 * appropriate.  There must be at least js::UINT32_CHAR_BUFFER_LENGTH elements
 * before |end| to avoid buffer underflow.  The start of the characters written
 * is returned and is necessarily before |end|.
 */
template <typename T>
inline mozilla::RangedPtr<T>
BackfillIndexInCharBuffer(uint32 index, mozilla::RangedPtr<T> end)
{
#ifdef DEBUG
    /*
     * Assert that the buffer we're filling will hold as many characters as we
     * could write out, by dereferencing the index that would hold the most
     * significant digit.
     */
    (void) *(end - UINT32_CHAR_BUFFER_LENGTH);
#endif

    do {
        uint32 next = index / 10, digit = index % 10;
        *--end = '0' + digit;
        index = next;
    } while (index > 0);

    return end;
}

inline bool
IndexToId(JSContext *cx, uint32 index, jsid *idp)
{
    if (index <= JSID_INT_MAX) {
        *idp = INT_TO_JSID(index);
        return true;
    }

    extern bool IndexToIdSlow(JSContext *cx, uint32 index, jsid *idp);
    return IndexToIdSlow(cx, index, idp);
}

static JS_ALWAYS_INLINE JSString *
IdToString(JSContext *cx, jsid id)
{
    if (JSID_IS_STRING(id))
        return JSID_TO_STRING(id);
    if (JS_LIKELY(JSID_IS_INT(id)))
        return js_IntToString(cx, JSID_TO_INT(id));
    return js_ValueToString(cx, IdToValue(id));
}

} // namespace js

#endif /* jsatominlines_h___ */
