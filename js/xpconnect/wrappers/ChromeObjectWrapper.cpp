/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ChromeObjectWrapper.h"
#include "WrapperFactory.h"
#include "AccessCheck.h"
#include "xpcprivate.h"
#include "jsapi.h"
#include "jswrapper.h"
#include "nsXULAppAPI.h"

using namespace JS;

namespace xpc {

const ChromeObjectWrapper ChromeObjectWrapper::singleton;

bool
ChromeObjectWrapper::defineProperty(JSContext* cx, HandleObject wrapper,
                                    HandleId id,
                                    Handle<PropertyDescriptor> desc,
                                    ObjectOpResult& result) const
{
    if (!AccessCheck::checkPassToPrivilegedCode(cx, wrapper, desc.value()))
        return false;
    return ChromeObjectWrapperBase::defineProperty(cx, wrapper, id, desc, result);
}

bool
ChromeObjectWrapper::set(JSContext* cx, HandleObject wrapper, HandleId id, HandleValue v,
                         HandleValue receiver, ObjectOpResult& result) const
{
    if (!AccessCheck::checkPassToPrivilegedCode(cx, wrapper, v))
        return false;
    return ChromeObjectWrapperBase::set(cx, wrapper, id, v, receiver, result);
}

} // namespace xpc
