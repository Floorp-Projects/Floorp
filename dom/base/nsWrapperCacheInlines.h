/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Gecko DOM code.
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *    Peter Van der Beken <peterv@propagandism.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef nsWrapperCacheInline_h___
#define nsWrapperCacheInline_h___

#include "nsWrapperCache.h"
#include "xpcprivate.h"

inline JSObject*
nsWrapperCache::GetWrapperPreserveColor() const
{
  JSObject *obj = GetJSObjectFromBits();
  return !IsProxy() || !obj || js::IsProxy(obj) ? obj : nsnull;
}

inline JSObject*
nsWrapperCache::GetWrapper() const
{
    JSObject* obj = GetWrapperPreserveColor();
    xpc_UnmarkGrayObject(obj);
    return obj;
}

inline void
nsWrapperCache::SetWrapper(JSObject* aWrapper)
{
    NS_ASSERTION(!PreservingWrapper(), "Clearing a preserved wrapper!");
    NS_ASSERTION(aWrapper, "Use ClearWrapper!");

    JSObject *obj = GetJSObjectFromBits();
    if (obj && mozilla::dom::binding::isExpandoObject(obj)) {
        NS_ASSERTION(mozilla::dom::binding::instanceIsProxy(aWrapper),
                     "We have an expando but this isn't a DOM proxy?");
        js::SetProxyExtra(aWrapper, mozilla::dom::binding::JSPROXYSLOT_EXPANDO,
                          js::ObjectValue(*obj));
    }

    SetWrapperBits(aWrapper);
}

inline JSObject*
nsWrapperCache::GetExpandoObjectPreserveColor() const
{
    JSObject *obj = GetJSObjectFromBits();
    if (!obj) {
        return NULL;
    }

    if (!IsProxy()) {
        // If we support non-proxy dom binding objects then this should be:
        //return mozilla::dom::binding::isExpandoObject(obj) ? obj : js::GetSlot(obj, EXPANDO_SLOT);
        return NULL;
    }

    // FIXME unmark gray?
    if (mozilla::dom::binding::instanceIsProxy(obj)) {
        return GetExpandoFromSlot(obj);
    }

    return mozilla::dom::binding::isExpandoObject(obj) ? obj : NULL;
}

inline JSObject*
nsWrapperCache::GetExpandoFromSlot(JSObject *obj)
{
    NS_ASSERTION(mozilla::dom::binding::instanceIsProxy(obj),
                 "Asking for an expando but this isn't a DOM proxy?");
    const js::Value &v = js::GetProxyExtra(obj, mozilla::dom::binding::JSPROXYSLOT_EXPANDO);
    return v.isUndefined() ? NULL : v.toObjectOrNull();
}

inline void
nsWrapperCache::ClearWrapper()
{
    NS_ASSERTION(!PreservingWrapper(), "Clearing a preserved wrapper!");
    JSObject *obj = GetJSObjectFromBits();
    if (!obj) {
        return;
    }

    JSObject *expando;
    if (mozilla::dom::binding::instanceIsProxy(obj)) {
        expando = GetExpandoFromSlot(obj);
    }
    else {
        // If we support non-proxy dom binding objects then this should be:
        //expando = js::GetSlot(obj, EXPANDO_SLOT);
        expando = NULL;
    }

    SetWrapperBits(expando);
}

inline void
nsWrapperCache::ClearWrapperIfProxy()
{
    if (!IsProxy()) {
        return;
    }

    RemoveExpandoObject();

    SetWrapperBits(NULL);
}

#endif /* nsWrapperCache_h___ */
