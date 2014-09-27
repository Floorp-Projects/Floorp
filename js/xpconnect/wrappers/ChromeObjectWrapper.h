/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __ChromeObjectWrapper_h__
#define __ChromeObjectWrapper_h__

#include "mozilla/Attributes.h"

#include "FilteringWrapper.h"

namespace xpc {

struct ExposedPropertiesOnly;

// When chrome JS objects are exposed to content, they get a ChromeObjectWrapper.
//
// The base filtering wrapper here does most of the work for us. We define a
// custom class here to introduce custom behavior with respect to the prototype
// chain.
#define ChromeObjectWrapperBase \
  FilteringWrapper<js::CrossCompartmentSecurityWrapper, ExposedPropertiesOnly>

class ChromeObjectWrapper : public ChromeObjectWrapperBase
{
  public:
    MOZ_CONSTEXPR ChromeObjectWrapper() : ChromeObjectWrapperBase(0) {}

    /* Custom traps. */
    virtual bool getPropertyDescriptor(JSContext *cx, JS::Handle<JSObject*> wrapper,
                                       JS::Handle<jsid> id,
                                       JS::MutableHandle<JSPropertyDescriptor> desc) const MOZ_OVERRIDE;
    virtual bool defineProperty(JSContext *cx, JS::Handle<JSObject*> wrapper,
                                JS::Handle<jsid> id,
                                JS::MutableHandle<JSPropertyDescriptor> desc) const MOZ_OVERRIDE;
    virtual bool set(JSContext *cx, JS::Handle<JSObject*> wrapper,
                     JS::Handle<JSObject*> receiver, JS::Handle<jsid> id,
                     bool strict, JS::MutableHandle<JS::Value> vp) const MOZ_OVERRIDE;

    virtual bool has(JSContext *cx, JS::Handle<JSObject*> wrapper,
                     JS::Handle<jsid> id, bool *bp) const MOZ_OVERRIDE;
    virtual bool get(JSContext *cx, JS::Handle<JSObject*> wrapper, JS::Handle<JSObject*> receiver,
                     JS::Handle<jsid> id, JS::MutableHandle<JS::Value> vp) const MOZ_OVERRIDE;

    virtual bool call(JSContext *cx, JS::Handle<JSObject*> wrapper,
                      const JS::CallArgs &args) const MOZ_OVERRIDE;
    virtual bool construct(JSContext *cx, JS::Handle<JSObject*> wrapper,
                           const JS::CallArgs &args) const MOZ_OVERRIDE;

    virtual bool objectClassIs(JS::Handle<JSObject*> obj, js::ESClassValue classValue,
                               JSContext *cx) const MOZ_OVERRIDE;

    virtual bool enter(JSContext *cx, JS::Handle<JSObject*> wrapper, JS::Handle<jsid> id,
                       js::Wrapper::Action act, bool *bp) const MOZ_OVERRIDE;

    // NB: One might think we'd need to implement enumerate(), keys(), iterate(),
    // and getPropertyKeys() here. However, ES5 built-in properties aren't
    // enumerable (and SpiderMonkey's implementation seems to match the spec
    // modulo Error.prototype.fileName and Error.prototype.lineNumber). Since
    // we're only remapping the prototypes of standard objects, there would
    // never be anything more to enumerate up the prototype chain. So we can
    // actually skip these.

    static const ChromeObjectWrapper singleton;
};

} /* namespace xpc */

#endif /* __ChromeObjectWrapper_h__ */
