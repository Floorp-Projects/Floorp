/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_WindowNamedPropertiesHandler_h
#define mozilla_dom_WindowNamedPropertiesHandler_h

#include "mozilla/dom/DOMJSProxyHandler.h"

namespace mozilla {
namespace dom {

class WindowNamedPropertiesHandler : public BaseDOMProxyHandler
{
public:
  WindowNamedPropertiesHandler() : BaseDOMProxyHandler(nullptr)
  {
    setHasPrototype(true);
  }
  virtual bool
  preventExtensions(JSContext* aCx, JS::Handle<JSObject*> aProxy) MOZ_OVERRIDE
  {
    // Throw a TypeError, per WebIDL.
    JS_ReportErrorNumber(aCx, js_GetErrorMessage, nullptr,
                         JSMSG_CANT_CHANGE_EXTENSIBILITY);
    return false;
  }
  virtual bool
  getOwnPropertyDescriptor(JSContext* aCx, JS::Handle<JSObject*> aProxy,
                           JS::Handle<jsid> aId,
                           JS::MutableHandle<JSPropertyDescriptor> aDesc,
                           unsigned aFlags) MOZ_OVERRIDE;
  virtual bool
  defineProperty(JSContext* aCx, JS::Handle<JSObject*> aProxy,
                 JS::Handle<jsid> aId,
                 JS::MutableHandle<JSPropertyDescriptor> aDesc) MOZ_OVERRIDE;
  virtual bool
  ownPropNames(JSContext* aCx, JS::Handle<JSObject*> aProxy, unsigned flags,
               JS::AutoIdVector& aProps) MOZ_OVERRIDE;
  virtual bool
  delete_(JSContext* aCx, JS::Handle<JSObject*> aProxy, JS::Handle<jsid> aId,
          bool* aBp) MOZ_OVERRIDE;
  virtual bool
  isExtensible(JSContext* aCx, JS::Handle<JSObject*> aProxy,
               bool* aIsExtensible) MOZ_OVERRIDE
  {
    *aIsExtensible = true;
    return true;
  }
  virtual const char*
  className(JSContext *aCx, JS::Handle<JSObject*> aProxy) MOZ_OVERRIDE
  {
    return "WindowProperties";
  }

  static WindowNamedPropertiesHandler*
  getInstance()
  {
    static WindowNamedPropertiesHandler instance;
    return &instance;
  }

  // For Install, aProto is the proto of the Window we're associated with.
  static void
  Install(JSContext *aCx, JS::Handle<JSObject*> aProto);
};

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_WindowNamedPropertiesHandler_h */
