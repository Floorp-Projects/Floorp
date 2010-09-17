/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
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
 * The Original Code is mozilla.org code, released
 * June 24, 2010.
 *
 * The Initial Developer of the Original Code is
 *    The Mozilla Foundation
 *
 * Contributor(s):
 *    Andreas Gal <gal@mozilla.com>
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

#include "FilteringWrapper.h"
#include "AccessCheck.h"
#include "CrossOriginWrapper.h"
#include "XrayWrapper.h"

#include "XPCWrapper.h"

using namespace js;

namespace xpc {

template <typename Base, typename Policy>
FilteringWrapper<Base, Policy>::FilteringWrapper(uintN flags) : Base(flags)
{
}

template <typename Base, typename Policy>
FilteringWrapper<Base, Policy>::~FilteringWrapper()
{
}

typedef JSWrapper::Permission Permission;

static const Permission PermitObjectAccess = JSWrapper::PermitObjectAccess;
static const Permission PermitPropertyAccess = JSWrapper::PermitPropertyAccess;
static const Permission DenyAccess = JSWrapper::DenyAccess;

template <typename Policy>
static bool
Filter(JSContext *cx, JSObject *wrapper, AutoIdVector &props)
{
    size_t w = 0;
    for (size_t n = 0; n < props.length(); ++n) {
        jsid id = props[n];
        Permission perm;
        if (!Policy::check(cx, wrapper, id, JSWrapper::GET, perm))
            return false; // Error
        if (perm != DenyAccess)
            props[w++] = id;
    }
    props.resize(w);
    return true;
}

template <typename Policy>
static bool
CheckAndReport(JSContext *cx, JSObject *wrapper, jsid id, JSWrapper::Action act, Permission &perm)
{
    if (!Policy::check(cx, wrapper, id, act, perm)) {
        return false;
    }
    if (perm == DenyAccess) {
        // Reporting an error here indicates a problem entering the
        // compartment. Therefore, any errors that we throw should be
        // thrown in our *caller's* compartment, so they can inspect
        // the error object.
        JSAutoEnterCompartment ac;
        if (!ac.enter(cx, wrapper))
            return false;

        AccessCheck::deny(cx, id);
        return false;
    }
    return true;
}

template <typename Base, typename Policy>
bool
FilteringWrapper<Base, Policy>::getOwnPropertyNames(JSContext *cx, JSObject *wrapper, AutoIdVector &props)
{
    return Base::getOwnPropertyNames(cx, wrapper, props) &&
           Filter<Policy>(cx, wrapper, props);
}

template <typename Base, typename Policy>
bool
FilteringWrapper<Base, Policy>::enumerate(JSContext *cx, JSObject *wrapper, AutoIdVector &props)
{
    return Base::enumerate(cx, wrapper, props) &&
           Filter<Policy>(cx, wrapper, props);
}

template <typename Base, typename Policy>
bool
FilteringWrapper<Base, Policy>::enumerateOwn(JSContext *cx, JSObject *wrapper, AutoIdVector &props)
{
    return Base::enumerateOwn(cx, wrapper, props) &&
           Filter<Policy>(cx, wrapper, props);
}

template <typename Base, typename Policy>
bool
FilteringWrapper<Base, Policy>::iterate(JSContext *cx, JSObject *wrapper, uintN flags, Value *vp)
{
    // We refuse to trigger the iterator hook across chrome wrappers because
    // we don't know how to censor custom iterator objects. Instead we trigger
    // the default proxy iterate trap, which will ask enumerate() for the list
    // of (consored) ids.
    return JSProxyHandler::iterate(cx, wrapper, flags, vp);
}

template <typename Base, typename Policy>
bool
FilteringWrapper<Base, Policy>::enter(JSContext *cx, JSObject *wrapper, jsid id,
                                      JSWrapper::Action act)
{
    Permission perm;
    return CheckAndReport<Policy>(cx, wrapper, id, act, perm) &&
           Base::enter(cx, wrapper, id, act);
}

#define SOW FilteringWrapper<JSCrossCompartmentWrapper, OnlyIfSubjectIsSystem>
#define COW FilteringWrapper<JSCrossCompartmentWrapper, ExposedPropertiesOnly>
#define XOW FilteringWrapper<XrayWrapper<CrossOriginWrapper>, CrossOriginAccessiblePropertiesOnly>
#define NNXOW FilteringWrapper<JSCrossCompartmentWrapper, CrossOriginAccessiblePropertiesOnly>

template<> SOW SOW::singleton(0);
template<> COW COW::singleton(0);
template<> XOW XOW::singleton(0);
template<> NNXOW NNXOW::singleton(0);

template class SOW;
template class COW;
template class XOW;
template class NNXOW;

}
