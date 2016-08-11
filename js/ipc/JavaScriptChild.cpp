/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=80:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "JavaScriptChild.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/ipc/MessageChannel.h"
#include "nsContentUtils.h"
#include "xpcprivate.h"
#include "jsfriendapi.h"
#include "AccessCheck.h"

using namespace JS;
using namespace mozilla;
using namespace mozilla::jsipc;

using mozilla::AutoSafeJSContext;

static void
UpdateChildWeakPointersBeforeSweepingZoneGroup(JSContext* cx, void* data)
{
    static_cast<JavaScriptChild*>(data)->updateWeakPointers();
}

JavaScriptChild::~JavaScriptChild()
{
    JSContext* cx = dom::danger::GetJSContext();
    JS_RemoveWeakPointerZoneGroupCallback(cx, UpdateChildWeakPointersBeforeSweepingZoneGroup);
}

bool
JavaScriptChild::init()
{
    if (!WrapperOwner::init())
        return false;
    if (!WrapperAnswer::init())
        return false;

    JSContext* cx = dom::danger::GetJSContext();
    JS_AddWeakPointerZoneGroupCallback(cx, UpdateChildWeakPointersBeforeSweepingZoneGroup, this);
    return true;
}

void
JavaScriptChild::updateWeakPointers()
{
    objects_.sweep();
    unwaivedObjectIds_.sweep();
    waivedObjectIds_.sweep();
}

JSObject*
JavaScriptChild::scopeForTargetObjects()
{
    // CPOWs from the parent need to point into the child's privileged junk
    // scope so that they can benefit from XrayWrappers in the child.
    return xpc::PrivilegedJunkScope();
}

PJavaScriptChild*
mozilla::jsipc::NewJavaScriptChild()
{
    JavaScriptChild* child = new JavaScriptChild();
    if (!child->init()) {
        delete child;
        return nullptr;
    }
    return child;
}

void
mozilla::jsipc::ReleaseJavaScriptChild(PJavaScriptChild* child)
{
    static_cast<JavaScriptChild*>(child)->decref();
}
