/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __ChromeObjectWrapper_h__
#define __ChromeObjectWrapper_h__

#include "mozilla/Attributes.h"

#include "FilteringWrapper.h"

namespace xpc {

struct OpaqueWithSilentFailing;

// When a vanilla chrome JS object is exposed to content, we use a wrapper that
// fails silently on GET, ENUMERATE, and GET_PROPERTY_DESCRIPTOR for legacy
// reasons. For extra security, we override the traps that allow content to pass
// an object to chrome, and perform extra security checks on them.
#define ChromeObjectWrapperBase \
  FilteringWrapper<js::CrossCompartmentSecurityWrapper, OpaqueWithSilentFailing>

class ChromeObjectWrapper : public ChromeObjectWrapperBase {
 public:
  constexpr ChromeObjectWrapper() : ChromeObjectWrapperBase(0) {}

  virtual bool defineProperty(JSContext* cx, JS::Handle<JSObject*> wrapper,
                              JS::Handle<jsid> id,
                              JS::Handle<JS::PropertyDescriptor> desc,
                              JS::ObjectOpResult& result) const override;
  virtual bool set(JSContext* cx, JS::HandleObject wrapper, JS::HandleId id,
                   JS::HandleValue v, JS::HandleValue receiver,
                   JS::ObjectOpResult& result) const override;

  static const ChromeObjectWrapper singleton;
};

} /* namespace xpc */

#endif /* __ChromeObjectWrapper_h__ */
