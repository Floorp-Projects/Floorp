/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_loader_ModuleEnvironmentProxy_h
#define mozilla_loader_ModuleEnvironmentProxy_h

#include "js/TypeDecls.h"   // JSContext, JSObject
#include "js/RootingAPI.h"  // JS::Handle

namespace mozilla {
namespace loader {

// Create an object that works in the same way as global object returned by
// `Cu.import`. This proxy exposes all global variables, including lexical
// variables.
//
// This is a temporary workaround to support not-in-tree code that depends on
// `Cu.import` return value.
//
// This will eventually be removed once ESM-ification finishes.
JSObject* CreateModuleEnvironmentProxy(JSContext* aCx,
                                       JS::Handle<JSObject*> aModuleObj);

}  // namespace loader
}  // namespace mozilla

#endif  // mozilla_loader_ModuleEnvironmentProxy_h
