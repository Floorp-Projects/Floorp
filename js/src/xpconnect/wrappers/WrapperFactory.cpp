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

#include "jsobj.h"
#include "jsvalue.h"

#include "WrapperFactory.h"
#include "CrossOriginWrapper.h"
#include "FilteringWrapper.h"
#include "XrayWrapper.h"
#include "AccessCheck.h"

#include "xpcprivate.h"

namespace xpc {

// When chrome pulls a naked property across the membrane using
// .wrappedJSObject, we want it to cross the membrane into the
// chrome compartment without automatically being wrapped into an
// X-ray wrapper. We achieve this by wrapping it into a special
// transparent wrapper in the origin (non-chrome) compartment. When
// an object with that special wrapper applied crosses into chrome,
// we know to not apply an X-ray wrapper.
JSWrapper WaiveXrayWrapperWrapper(WrapperFactory::WAIVE_XRAY_WRAPPER_FLAG);

// When objects for which we waived the X-ray wrapper cross into
// chrome, we wrap them into a special cross-compartment wrapper
// that transitively extends the waiver to all properties we get
// off it.
CrossOriginWrapper XrayWrapperWaivedWrapper(WrapperFactory::WAIVE_XRAY_WRAPPER_FLAG);

JSObject *
WrapperFactory::Rewrap(JSContext *cx, JSObject *obj, JSObject *wrappedProto, JSObject *parent,
                       uintN flags)
{
    NS_ASSERTION(!obj->isWrapper() || obj->getClass()->ext.innerObject,
                 "wrapped object passed to rewrap");
    NS_ASSERTION(JS_GET_CLASS(cx, obj) != &XrayUtils::HolderClass, "trying to wrap a holder");

    if (IS_SLIM_WRAPPER(obj) && !MorphSlimWrapper(cx, obj))
        return nsnull;

    JSCompartment *origin = obj->getCompartment();
    JSCompartment *target = cx->compartment;
    JSObject *xrayHolder = nsnull;

    JSWrapper *wrapper;
    if (AccessCheck::isChrome(target)) {
        if (AccessCheck::isChrome(origin)) {
            wrapper = &JSCrossCompartmentWrapper::singleton;
        } else if (flags & WAIVE_XRAY_WRAPPER_FLAG) {
            // If we waived the X-ray wrapper for this object, wrap it into a
            // special wrapper to transitively maintain the X-ray waiver.
            wrapper = &XrayWrapperWaivedWrapper;
        } else {
            // Native objects must be wrapped into an X-ray wrapper.
            if (!obj->getGlobal()->isSystem() &&
                (IS_WN_WRAPPER(obj) || obj->getClass()->ext.innerObject)) {
                typedef XrayWrapper<JSCrossCompartmentWrapper, CrossCompartmentXray> Xray;
                wrapper = &Xray::singleton;
                xrayHolder = Xray::createHolder(cx, obj, parent);
                if (!xrayHolder)
                    return nsnull;
            } else {
                wrapper = &JSCrossCompartmentWrapper::singleton;
            }
        }
    } else if (AccessCheck::isChrome(origin)) {
        // If an object that needs a system only wrapper crosses into content
        // from chrome, we have to wrap it into a system only wrapper on the
        // fly. In this case we don't need to restrict to exposed properties
        // since only privileged content will be allowed to touch it anyway.
        if (AccessCheck::needsSystemOnlyWrapper(obj)) {
            wrapper = &FilteringWrapper<JSCrossCompartmentWrapper,
                                        OnlyIfSubjectIsSystem>::singleton;
        } else {
            wrapper = &FilteringWrapper<JSCrossCompartmentWrapper,
                                        ExposedPropertiesOnly>::singleton;
        }
    } else if (AccessCheck::isSameOrigin(origin, target)) {
        // Same origin we use a transparent wrapper;
        wrapper = &JSCrossCompartmentWrapper::singleton;
    } else {
        // Cross origin we want to disallow scripting and limit access to
        // a predefined set of properties. XrayWrapper adds a property
        // (.wrappedJSObject) which allows bypassing the XrayWrapper, but
        // we filter out access to that property.
        if (!IS_WN_WRAPPER(obj)) {
            wrapper = &FilteringWrapper<JSCrossCompartmentWrapper,
                                        CrossOriginAccessiblePropertiesOnly>::singleton;
        } else {
            typedef XrayWrapper<JSCrossCompartmentWrapper, CrossCompartmentXray> Xray;

            // Location objects can become same origin after navigation, so we might
            // have to grant transparent access later on.
            if (IsLocationObject(obj)) {
                wrapper = &FilteringWrapper<Xray,
                    SameOriginOrCrossOriginAccessiblePropertiesOnly>::singleton;
            } else {
                wrapper= &FilteringWrapper<Xray,
                    CrossOriginAccessiblePropertiesOnly>::singleton;
            }

            xrayHolder = Xray::createHolder(cx, obj, parent);
            if (!xrayHolder)
                return nsnull;
        }
    }

    JSObject *wrapperObj = JSWrapper::New(cx, obj, wrappedProto, parent, wrapper);
    if (!wrapperObj || !xrayHolder)
        return wrapperObj;
    wrapperObj->setProxyExtra(js::ObjectValue(*xrayHolder));
    xrayHolder->setSlot(XrayUtils::JSSLOT_PROXY_OBJ, js::ObjectValue(*wrapperObj));
    return wrapperObj;
}

typedef FilteringWrapper<XrayWrapper<JSWrapper, SameCompartmentXray>,
                         SameOriginOrCrossOriginAccessiblePropertiesOnly> LW;

bool
WrapperFactory::IsLocationObject(JSObject *obj)
{
    const char *name = obj->getClass()->name;
    return name[0] == 'L' && !strcmp(name, "Location");
}

JSObject *
WrapperFactory::WrapLocationObject(JSContext *cx, JSObject *obj)
{
    JSObject *xrayHolder = LW::createHolder(cx, obj, obj->getParent());
    if (!xrayHolder)
        return NULL;
    JSObject *wrapperObj = JSWrapper::New(cx, obj, obj->getProto(), NULL, &LW::singleton);
    if (!wrapperObj)
        return NULL;
    wrapperObj->setProxyExtra(js::ObjectValue(*xrayHolder));
    xrayHolder->setSlot(XrayUtils::JSSLOT_PROXY_OBJ, js::ObjectValue(*wrapperObj));
    return wrapperObj;
}

}
