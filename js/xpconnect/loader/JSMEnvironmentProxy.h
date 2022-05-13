/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_loader_JSMEnvironmentProxy_h
#define mozilla_loader_JSMEnvironmentProxy_h

#include "js/Id.h"          // JS::PropertyKey
#include "js/TypeDecls.h"   // JSContext, JSObject
#include "js/RootingAPI.h"  // JS::Handle

namespace mozilla {
namespace loader {

JSObject* ResolveModuleObjectPropertyById(JSContext* aCx,
                                          JS::Handle<JSObject*> aModObj,
                                          JS::Handle<JS::PropertyKey> aId);

JSObject* ResolveModuleObjectProperty(JSContext* aCx,
                                      JS::Handle<JSObject*> aModObj,
                                      const char* aName);

JSObject* CreateJSMEnvironmentProxy(JSContext* aCx,
                                    JS::Handle<JSObject*> aGlobalObj);

}  // namespace loader
}  // namespace mozilla

#endif  // mozilla_loader_JSMEnvironmentProxy_h
