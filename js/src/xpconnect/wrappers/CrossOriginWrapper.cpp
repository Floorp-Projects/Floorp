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

#include "nsJSPrincipals.h"

#include "XPCWrapper.h"

#include "CrossOriginWrapper.h"
#include "AccessCheck.h"
#include "WrapperFactory.h"

namespace xpc {

NoWaiverWrapper::NoWaiverWrapper(uintN flags) : JSCrossCompartmentWrapper(flags)
{
}

NoWaiverWrapper::~NoWaiverWrapper()
{
}

CrossOriginWrapper::CrossOriginWrapper(uintN flags) : NoWaiverWrapper(flags)
{
}

CrossOriginWrapper::~CrossOriginWrapper()
{
}

bool
CrossOriginWrapper::getPropertyDescriptor(JSContext *cx, JSObject *wrapper, jsid id,
                                          bool set, js::PropertyDescriptor *desc)
{
    return JSCrossCompartmentWrapper::getPropertyDescriptor(cx, wrapper, id, set, desc) &&
           WrapperFactory::WaiveXrayAndWrap(cx, js::Jsvalify(&desc->value));
}

bool
CrossOriginWrapper::getOwnPropertyDescriptor(JSContext *cx, JSObject *wrapper, jsid id,
                                          bool set, js::PropertyDescriptor *desc)
{
    return JSCrossCompartmentWrapper::getOwnPropertyDescriptor(cx, wrapper, id, set, desc) &&
           WrapperFactory::WaiveXrayAndWrap(cx, js::Jsvalify(&desc->value));
}

bool
CrossOriginWrapper::get(JSContext *cx, JSObject *wrapper, JSObject *receiver, jsid id,
                        js::Value *vp)
{
    return JSCrossCompartmentWrapper::get(cx, wrapper, receiver, id, vp) &&
           WrapperFactory::WaiveXrayAndWrap(cx, js::Jsvalify(vp));
}

bool
CrossOriginWrapper::call(JSContext *cx, JSObject *wrapper, uintN argc, js::Value *vp)
{
    return JSCrossCompartmentWrapper::call(cx, wrapper, argc, vp) &&
           WrapperFactory::WaiveXrayAndWrap(cx, js::Jsvalify(vp));
}

bool
CrossOriginWrapper::construct(JSContext *cx, JSObject *wrapper,
                              uintN argc, js::Value *argv, js::Value *rval)
{
    return JSCrossCompartmentWrapper::construct(cx, wrapper, argc, argv, rval) &&
           WrapperFactory::WaiveXrayAndWrap(cx, js::Jsvalify(rval));
}

bool
NoWaiverWrapper::enter(JSContext *cx, JSObject *wrapper, jsid id, Action act, bool *bp)
{
    *bp = true; // always allowed
    nsIScriptSecurityManager *ssm = XPCWrapper::GetSecurityManager();
    if (!ssm) {
        return true;
    }

    // Note: By the time enter is called here, JSCrossCompartmentWrapper has
    // already pushed the fake stack frame onto cx. Because of this, the frame
    // that we're clamping is the one that we want (the one in our compartment).
    JSStackFrame *fp = NULL;
    nsIPrincipal *principal = GetCompartmentPrincipal(wrappedObject(wrapper)->compartment());
    nsresult rv = ssm->PushContextPrincipal(cx, JS_FrameIterator(cx, &fp), principal);
    if (NS_FAILED(rv)) {
        NS_WARNING("Not allowing call because we're out of memory");
        JS_ReportOutOfMemory(cx);
        return false;
    }
    return true;
}

void
NoWaiverWrapper::leave(JSContext *cx, JSObject *wrapper)
{
    nsIScriptSecurityManager *ssm = XPCWrapper::GetSecurityManager();
    if (ssm) {
        ssm->PopContextPrincipal(cx);
    }
}

}
