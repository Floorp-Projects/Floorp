/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef js_GCVariant_h
#define js_GCVariant_h

#include "mozilla/Variant.h"

#include "js/GCPolicyAPI.h"
#include "js/RootingAPI.h"
#include "js/TracingAPI.h"

namespace JS {

// These template specializations allow Variant to be used inside GC wrappers.
//
// When matching on GC wrappers around Variants, matching should be done on
// the wrapper itself. The matcher class's methods should take Handles or
// MutableHandles. For example,
//
//   struct MyMatcher
//   {
//        using ReturnType = const char*;
//        ReturnType match(HandleObject o) { return "object"; }
//        ReturnType match(HandleScript s) { return "script"; }
//   };
//
//   Rooted<Variant<JSObject*, JSScript*>> v(cx, someScript);
//   MyMatcher mm;
//   v.match(mm);
//
// If you get compile errors about inability to upcast subclasses (e.g., from
// NativeObject* to JSObject*) and are inside js/src, be sure to also include
// "gc/Policy.h".

namespace detail {

template <typename... Ts>
struct GCVariantImplementation;

// The base case.
template <typename T>
struct GCVariantImplementation<T>
{
    template <typename ConcreteVariant>
    static void trace(JSTracer* trc, ConcreteVariant* v, const char* name) {
        T& thing = v->template as<T>();
        if (!mozilla::IsPointer<T>::value || thing)
            GCPolicy<T>::trace(trc, &thing, name);
    }

    template <typename Matcher, typename ConcreteVariant>
    static typename Matcher::ReturnType
    match(Matcher& matcher, Handle<ConcreteVariant> v) {
        const T& thing = v.get().template as<T>();
        return matcher.match(Handle<T>::fromMarkedLocation(&thing));
    }

    template <typename Matcher, typename ConcreteVariant>
    static typename Matcher::ReturnType
    match(Matcher& matcher, MutableHandle<ConcreteVariant> v) {
        T& thing = v.get().template as<T>();
        return matcher.match(MutableHandle<T>::fromMarkedLocation(&thing));
    }
};

// The inductive case.
template <typename T, typename... Ts>
struct GCVariantImplementation<T, Ts...>
{
    using Next = GCVariantImplementation<Ts...>;

    template <typename ConcreteVariant>
    static void trace(JSTracer* trc, ConcreteVariant* v, const char* name) {
        if (v->template is<T>()) {
            T& thing = v->template as<T>();
            if (!mozilla::IsPointer<T>::value || thing)
                GCPolicy<T>::trace(trc, &thing, name);
        } else {
            Next::trace(trc, v, name);
        }
    }

    template <typename Matcher, typename ConcreteVariant>
    static typename Matcher::ReturnType
    match(Matcher& matcher, Handle<ConcreteVariant> v) {
        if (v.get().template is<T>()) {
            const T& thing = v.get().template as<T>();
            return matcher.match(Handle<T>::fromMarkedLocation(&thing));
        }
        return Next::match(matcher, v);
    }

    template <typename Matcher, typename ConcreteVariant>
    static typename Matcher::ReturnType
    match(Matcher& matcher, MutableHandle<ConcreteVariant> v) {
        if (v.get().template is<T>()) {
            T& thing = v.get().template as<T>();
            return matcher.match(MutableHandle<T>::fromMarkedLocation(&thing));
        }
        return Next::match(matcher, v);
    }
};

} // namespace detail

template <typename... Ts>
struct GCPolicy<mozilla::Variant<Ts...>>
{
    using Impl = detail::GCVariantImplementation<Ts...>;

    // Variants do not provide initial(). They do not have a default initial
    // value and one must be provided.

    static void trace(JSTracer* trc, mozilla::Variant<Ts...>* v, const char* name) {
        Impl::trace(trc, v, name);
    }
};

} // namespace JS

namespace js {

template <typename Outer, typename... Ts>
class GCVariantOperations
{
    using Impl = JS::detail::GCVariantImplementation<Ts...>;
    using Variant = mozilla::Variant<Ts...>;

    const Variant& variant() const { return static_cast<const Outer*>(this)->get(); }

  public:
    template <typename T>
    bool is() const {
        return variant().template is<T>();
    }

    template <typename T>
    JS::Handle<T> as() const {
        return Handle<T>::fromMarkedLocation(&variant().template as<T>());
    }

    template <typename Matcher>
    typename Matcher::ReturnType
    match(Matcher& matcher) const {
        return Impl::match(matcher, JS::Handle<Variant>::fromMarkedLocation(&variant()));
    }
};

template <typename Outer, typename... Ts>
class MutableGCVariantOperations
  : public GCVariantOperations<Outer, Ts...>
{
    using Impl = JS::detail::GCVariantImplementation<Ts...>;
    using Variant = mozilla::Variant<Ts...>;

    const Variant& variant() const { return static_cast<const Outer*>(this)->get(); }
    Variant& variant() { return static_cast<Outer*>(this)->get(); }

  public:
    template <typename T>
    JS::MutableHandle<T> as() {
        return JS::MutableHandle<T>::fromMarkedLocation(&variant().template as<T>());
    }

    template <typename Matcher>
    typename Matcher::ReturnType
    match(Matcher& matcher) {
        return Impl::match(matcher, JS::MutableHandle<Variant>::fromMarkedLocation(&variant()));
    }
};

template <typename... Ts>
class RootedBase<mozilla::Variant<Ts...>>
  : public MutableGCVariantOperations<JS::Rooted<mozilla::Variant<Ts...>>, Ts...>
{ };

template <typename... Ts>
class MutableHandleBase<mozilla::Variant<Ts...>>
  : public MutableGCVariantOperations<JS::MutableHandle<mozilla::Variant<Ts...>>, Ts...>
{ };

template <typename... Ts>
class HandleBase<mozilla::Variant<Ts...>>
  : public GCVariantOperations<JS::Handle<mozilla::Variant<Ts...>>, Ts...>
{ };

template <typename... Ts>
class PersistentRootedBase<mozilla::Variant<Ts...>>
  : public MutableGCVariantOperations<JS::PersistentRooted<mozilla::Variant<Ts...>>, Ts...>
{ };

} // namespace js

#endif // js_GCVariant_h
