/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MaybeCrossOriginObject_h
#define mozilla_dom_MaybeCrossOriginObject_h

/**
 * Shared infrastructure for WindowProxy and Location objects.  These
 * are the objects that can be accessed cross-origin in the HTML
 * specification.
 *
 * This class can be inherited from by the relevant proxy handlers to
 * help implement spec algorithms.
 *
 * The algorithms this class implements come from
 * <https://html.spec.whatwg.org/multipage/browsers.html#shared-abstract-operations>,
 * <https://html.spec.whatwg.org/multipage/window-object.html#the-windowproxy-exotic-object>,
 * and
 * <https://html.spec.whatwg.org/multipage/history.html#the-location-interface>.
 *
 * The class is templated on its base so we can directly implement the things
 * that should have identical implementations for WindowProxy and Location.  The
 * templating is needed because WindowProxy needs to be a wrapper and Location
 * shouldn't be one.
 */

#include "js/Class.h"
#include "js/TypeDecls.h"
#include "nsStringFwd.h"
#include "mozilla/Maybe.h"

namespace mozilla {
namespace dom {

/**
 * "mAttributes" and "mMethods" are the cross-origin attributes and methods we
 * care about, which should get defined on holders.
 *
 * "mChromeOnlyAttributes" and "mChromeOnlyMethods" are the cross-origin
 * attributes and methods we care about, which should get defined on holders
 * for the chrome realm, in addition to the properties that are in
 * "mAttributes" and "mMethods".
 */
struct CrossOriginProperties {
  const JSPropertySpec* mAttributes;
  const JSFunctionSpec* mMethods;
  const JSPropertySpec* mChromeOnlyAttributes;
  const JSFunctionSpec* mChromeOnlyMethods;
};

// Methods that MaybeCrossOriginObject wants that do not depend on the "Base"
// template parameter.  We can avoid having multiple instantiations of them by
// pulling them out into this helper class.
class MaybeCrossOriginObjectMixins {
 public:
  /**
   * Implementation of
   * <https://html.spec.whatwg.org/multipage/browsers.html#isplatformobjectsameorigin-(-o-)>.
   * "cx" and "obj" may or may not be same-compartment and even when
   * same-compartment may not be same-Realm.  "obj" can be a WindowProxy, a
   * Window, or a Location.
   */
  static bool IsPlatformObjectSameOrigin(JSContext* cx, JSObject* obj);

 protected:
  /**
   * Implementation of
   * <https://html.spec.whatwg.org/multipage/browsers.html#crossorigingetownpropertyhelper-(-o,-p-)>.
   *
   * "cx" and "obj" are expected to be different-Realm here, and may be
   * different-compartment.  "obj" can be a "WindowProxy" or a "Location" or a
   * cross-process proxy for one of those.
   */
  bool CrossOriginGetOwnPropertyHelper(
      JSContext* cx, JS::Handle<JSObject*> obj, JS::Handle<jsid> id,
      JS::MutableHandle<Maybe<JS::PropertyDescriptor>> desc) const;

  /**
   * Implementation of
   * <https://html.spec.whatwg.org/multipage/browsers.html#crossoriginpropertyfallback-(-p-)>.
   *
   * This should be called at the end of getOwnPropertyDescriptor
   * methods in the cross-origin case.
   *
   * "cx" and "obj" are expected to be different-Realm here, and may
   * be different-compartment.  "obj" can be a "WindowProxy" or a
   * "Location" or a cross-process proxy for one of those.
   */
  static bool CrossOriginPropertyFallback(
      JSContext* cx, JS::Handle<JSObject*> obj, JS::Handle<jsid> id,
      JS::MutableHandle<Maybe<JS::PropertyDescriptor>> desc);

  /**
   * Implementation of
   * <https://html.spec.whatwg.org/multipage/browsers.html#crossoriginget-(-o,-p,-receiver-)>.
   *
   * "cx" and "obj" are expected to be different-Realm here and may be
   * different-compartment.  "obj" can be a "WindowProxy" or a
   * "Location" or a cross-process proxy for one of those.
   *
   * "receiver" will be in the compartment of "cx".  The return value will
   * be in the compartment of "cx".
   */
  static bool CrossOriginGet(JSContext* cx, JS::Handle<JSObject*> obj,
                             JS::Handle<JS::Value> receiver,
                             JS::Handle<jsid> id,
                             JS::MutableHandle<JS::Value> vp);

  /**
   * Implementation of
   * <https://html.spec.whatwg.org/multipage/browsers.html#crossoriginset-(-o,-p,-v,-receiver-)>.
   *
   * "cx" and "obj" are expected to be different-Realm here and may be
   * different-compartment.  "obj" can be a "WindowProxy" or a
   * "Location" or a cross-process proxy for one of those.
   *
   * "receiver" and "v" will be in the compartment of "cx".
   */
  static bool CrossOriginSet(JSContext* cx, JS::Handle<JSObject*> obj,
                             JS::Handle<jsid> id, JS::Handle<JS::Value> v,
                             JS::Handle<JS::Value> receiver,
                             JS::ObjectOpResult& result);

  /**
   * Utility method to ensure a holder for cross-origin properties for the
   * current global of the JSContext.
   *
   * When this is called, "cx" and "obj" are _always_ different-Realm, because
   * this is only used in cross-origin situations.  The "holder" return value is
   * always in the Realm of "cx".
   *
   * "obj" is the object which has space to store the collection of holders in
   * the given slot.
   *
   * "properties" are the cross-origin attributes and methods we care about,
   * which should get defined on holders.
   */
  static bool EnsureHolder(JSContext* cx, JS::Handle<JSObject*> obj,
                           size_t slot, const CrossOriginProperties& properties,
                           JS::MutableHandle<JSObject*> holder);

  /**
   * Ensures we have a holder object for the current Realm.  When this is
   * called, "obj" is guaranteed to not be same-Realm with "cx", because this
   * is only used for cross-origin cases.
   *
   * Subclasses are expected to implement this by calling our static
   * EnsureHolder with the appropriate arguments.
   */
  virtual bool EnsureHolder(JSContext* cx, JS::Handle<JSObject*> proxy,
                            JS::MutableHandle<JSObject*> holder) const = 0;

  /**
   * Report a cross-origin denial for a property named by aId.  Always
   * returns false, so it can be used as "return
   * ReportCrossOriginDenial(...);".
   */
  static bool ReportCrossOriginDenial(JSContext* aCx, JS::Handle<jsid> aId,
                                      const nsACString& aAccessType);
};

// A proxy handler for objects that may be cross-origin objects.  Whether they
// actually _are_ cross-origin objects can change dynamically if document.domain
// is set.
template <typename Base>
class MaybeCrossOriginObject : public Base,
                               public MaybeCrossOriginObjectMixins {
 protected:
  template <typename... Args>
  constexpr MaybeCrossOriginObject(Args&&... aArgs)
      : Base(std::forward<Args>(aArgs)...) {}

  /**
   * Implementation of [[GetPrototypeOf]] as defined in
   * <https://html.spec.whatwg.org/multipage/window-object.html#windowproxy-getprototypeof>
   * and
   * <https://html.spec.whatwg.org/multipage/history.html#location-getprototypeof>.
   *
   * Our prototype-storage model looks quite different from the spec's, so we
   * need to implement some hooks that don't directly map to the spec.
   *
   * "proxy" is the WindowProxy or Location involved.  It may or may not be
   * same-compartment with cx.
   *
   * "protop" is the prototype value (possibly null).  It is guaranteed to be
   * same-compartment with cx after this function returns successfully.
   */
  bool getPrototype(JSContext* cx, JS::Handle<JSObject*> proxy,
                    JS::MutableHandle<JSObject*> protop) const final;

  /**
   * Hook for doing the OrdinaryGetPrototypeOf bits that [[GetPrototypeOf]] does
   * in the spec.  Location and WindowProxy store that information somewhat
   * differently.
   *
   * The prototype should come from the Realm of "cx".
   */
  virtual JSObject* getSameOriginPrototype(JSContext* cx) const = 0;

  /**
   * Implementation of [[SetPrototypeOf]] as defined in
   * <https://html.spec.whatwg.org/multipage/window-object.html#windowproxy-setprototypeof>
   * and
   * <https://html.spec.whatwg.org/multipage/history.html#location-setprototypeof>.
   *
   * "proxy" is the WindowProxy or Location object involved.  It may or may not
   * be same-compartment with "cx".
   *
   * "proto" is the new prototype object (possibly null).  It must be
   * same-compartment with "cx".
   */
  bool setPrototype(JSContext* cx, JS::Handle<JSObject*> proxy,
                    JS::Handle<JSObject*> proto,
                    JS::ObjectOpResult& result) const final;

  /**
   * Our non-standard getPrototypeIfOrdinary hook.
   */
  bool getPrototypeIfOrdinary(JSContext* cx, JS::Handle<JSObject*> proxy,
                              bool* isOrdinary,
                              JS::MutableHandle<JSObject*> protop) const final;

  /**
   * Our non-standard setImmutablePrototype hook.
   */
  bool setImmutablePrototype(JSContext* cx, JS::Handle<JSObject*> proxy,
                             bool* succeeded) const final;

  /**
   * Implementation of [[IsExtensible]] as defined in
   * <https://html.spec.whatwg.org/multipage/window-object.html#windowproxy-isextensible>
   * and
   * <https://html.spec.whatwg.org/multipage/history.html#location-isextensible>.
   */
  bool isExtensible(JSContext* cx, JS::Handle<JSObject*> proxy,
                    bool* extensible) const final;

  /**
   * Implementation of [[PreventExtensions]] as defined in
   * <https://html.spec.whatwg.org/multipage/window-object.html#windowproxy-preventextensions>
   * and
   * <https://html.spec.whatwg.org/multipage/history.html#location-preventextensions>.
   */
  bool preventExtensions(JSContext* cx, JS::Handle<JSObject*> proxy,
                         JS::ObjectOpResult& result) const final;

  /**
   * Implementation of [[GetOwnProperty]] is completely delegated to subclasses.
   *
   * "proxy" is the WindowProxy or Location object involved.  It may or may not
   * be same-compartment with cx.
   */
  bool getOwnPropertyDescriptor(
      JSContext* cx, JS::Handle<JSObject*> proxy, JS::Handle<jsid> id,
      JS::MutableHandle<Maybe<JS::PropertyDescriptor>> desc) const override = 0;

  /**
   * Implementation of [[DefineOwnProperty]] as defined in
   * <https://html.spec.whatwg.org/multipage/window-object.html#windowproxy-defineownproperty>
   * and
   * <https://html.spec.whatwg.org/multipage/history.html#location-defineownproperty>.
   * "proxy" is the WindowProxy or Location object involved.  It may or may not
   * be same-compartment with cx.
   *
   */
  bool defineProperty(JSContext* cx, JS::Handle<JSObject*> proxy,
                      JS::Handle<jsid> id,
                      JS::Handle<JS::PropertyDescriptor> desc,
                      JS::ObjectOpResult& result) const final;

  /**
   * Some of our base classes define _another_ virtual defineProperty, and we
   * get overloaded-virtual warnings as a result due to us hiding it, if we
   * don't pull it in here.
   */
  using Base::defineProperty;

  /**
   * Hook for handling the same-origin case in defineProperty.
   *
   * "proxy" is the WindowProxy or Location object involved.  It will be
   * same-compartment with cx.
   *
   * "desc" is a the descriptor being defined.  It will be same-compartment with
   * cx.
   */
  virtual bool definePropertySameOrigin(JSContext* cx,
                                        JS::Handle<JSObject*> proxy,
                                        JS::Handle<jsid> id,
                                        JS::Handle<JS::PropertyDescriptor> desc,
                                        JS::ObjectOpResult& result) const = 0;

  /**
   * Implementation of [[Get]] is completely delegated to subclasses.
   *
   * "proxy" is the WindowProxy or Location object involved.  It may or may not
   * be same-compartment with "cx".
   *
   * "receiver" is the receiver ("this") for the get.  It will be
   * same-compartment with "cx"
   *
   * "vp" is the return value.  It will be same-compartment with "cx".
   */
  bool get(JSContext* cx, JS::Handle<JSObject*> proxy,
           JS::Handle<JS::Value> receiver, JS::Handle<jsid> id,
           JS::MutableHandle<JS::Value> vp) const override = 0;

  /**
   * Implementation of [[Set]] is completely delegated to subclasses.
   *
   * "proxy" is the WindowProxy or Location object involved.  It may or may not
   * be same-compartment with "cx".
   *
   * "v" is the value being set.  It will be same-compartment with "cx".
   *
   * "receiver" is the receiver ("this") for the set.  It will be
   * same-compartment with "cx".
   */
  bool set(JSContext* cx, JS::Handle<JSObject*> proxy, JS::Handle<jsid> id,
           JS::Handle<JS::Value> v, JS::Handle<JS::Value> receiver,
           JS::ObjectOpResult& result) const override = 0;

  /**
   * Implementation of [[Delete]] is completely delegated to subclasses.
   *
   * "proxy" is the WindowProxy or Location object involved.  It may or may not
   * be same-compartment with "cx".
   */
  bool delete_(JSContext* cx, JS::Handle<JSObject*> proxy, JS::Handle<jsid> id,
               JS::ObjectOpResult& result) const override = 0;

  /**
   * Spidermonkey-internal hook for enumerating objects.
   */
  bool enumerate(JSContext* cx, JS::Handle<JSObject*> proxy,
                 JS::MutableHandleVector<jsid> props) const final;

  /**
   * Spidermonkey-internal hook used for instanceof.  We need to override this
   * because otherwise we can end up doing instanceof work in the wrong
   * compartment.
   */
  bool hasInstance(JSContext* cx, JS::Handle<JSObject*> proxy,
                   JS::MutableHandle<JS::Value> v, bool* bp) const final;

  /**
   * Spidermonkey-internal hook used by Object.prototype.toString.  Subclasses
   * need to implement this, because we don't know what className they want.
   * Except in the cross-origin case, when we could maybe handle it...
   */
  const char* className(JSContext* cx,
                        JS::Handle<JSObject*> proxy) const override = 0;
};

}  // namespace dom
}  // namespace mozilla

#endif /* mozilla_dom_MaybeCrossOriginObject_h */
