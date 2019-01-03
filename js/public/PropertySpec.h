/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Property descriptors and flags. */

#ifndef js_PropertySpec_h
#define js_PropertySpec_h

#include "mozilla/Assertions.h"  // MOZ_ASSERT{,_IF}

#include <stddef.h>     // size_t
#include <stdint.h>     // uint8_t, uint16_t, int32_t, uint32_t, uintptr_t
#include <type_traits>  // std::enable_if

#include "jstypes.h"  // JS_PUBLIC_API

#include "js/CallArgs.h"            // JSNative
#include "js/PropertyDescriptor.h"  // JSPROP_*
#include "js/RootingAPI.h"          // JS::MutableHandle
#include "js/Value.h"               // JS::Value

struct JSContext;
struct JSJitInfo;

/**
 * Wrapper to relace JSNative for JSPropertySpecs and JSFunctionSpecs. This will
 * allow us to pass one JSJitInfo per function with the property/function spec,
 * without additional field overhead.
 */
struct JSNativeWrapper {
  JSNative op;
  const JSJitInfo* info;
};

/**
 * Macro static initializers which make it easy to pass no JSJitInfo as part of
 * a JSPropertySpec or JSFunctionSpec.
 */
#define JSNATIVE_WRAPPER(native) \
  {                              \
    { native, nullptr }          \
  }

/**
 * Description of a property. JS_DefineProperties and JS_InitClass take arrays
 * of these and define many properties at once. JS_PSG, JS_PSGS and JS_PS_END
 * are helper macros for defining such arrays.
 */
struct JSPropertySpec {
  struct SelfHostedWrapper {
    void* unused;
    const char* funname;
  };

  struct ValueWrapper {
    uintptr_t type;
    union {
      const char* string;
      int32_t int32;
    };
  };

  const char* name;
  uint8_t flags;
  union {
    struct {
      union {
        JSNativeWrapper native;
        SelfHostedWrapper selfHosted;
      } getter;
      union {
        JSNativeWrapper native;
        SelfHostedWrapper selfHosted;
      } setter;
    } accessors;
    ValueWrapper value;
  };

  bool isAccessor() const { return !(flags & JSPROP_INTERNAL_USE_BIT); }
  JS_PUBLIC_API bool getValue(JSContext* cx,
                              JS::MutableHandle<JS::Value> value) const;

  bool isSelfHosted() const {
    MOZ_ASSERT(isAccessor());

#ifdef DEBUG
    // Verify that our accessors match our JSPROP_GETTER flag.
    if (flags & JSPROP_GETTER) {
      checkAccessorsAreSelfHosted();
    } else {
      checkAccessorsAreNative();
    }
#endif
    return (flags & JSPROP_GETTER);
  }

  static_assert(sizeof(SelfHostedWrapper) == sizeof(JSNativeWrapper),
                "JSPropertySpec::getter/setter must be compact");
  static_assert(offsetof(SelfHostedWrapper, funname) ==
                    offsetof(JSNativeWrapper, info),
                "JS_SELF_HOSTED* macros below require that "
                "SelfHostedWrapper::funname overlay "
                "JSNativeWrapper::info");

 private:
  void checkAccessorsAreNative() const {
    MOZ_ASSERT(accessors.getter.native.op);
    // We may not have a setter at all.  So all we can assert here, for the
    // native case is that if we have a jitinfo for the setter then we have
    // a setter op too.  This is good enough to make sure we don't have a
    // SelfHostedWrapper for the setter.
    MOZ_ASSERT_IF(accessors.setter.native.info, accessors.setter.native.op);
  }

  void checkAccessorsAreSelfHosted() const {
    MOZ_ASSERT(!accessors.getter.selfHosted.unused);
    MOZ_ASSERT(!accessors.setter.selfHosted.unused);
  }
};

namespace JS {
namespace detail {

/* NEVER DEFINED, DON'T USE.  For use by JS_CAST_STRING_TO only. */
template <size_t N>
inline int CheckIsCharacterLiteral(const char (&arr)[N]);

/* NEVER DEFINED, DON'T USE.  For use by JS_CAST_INT32_TO only. */
inline int CheckIsInt32(int32_t value);

}  // namespace detail
}  // namespace JS

#define JS_CAST_STRING_TO(s, To)                                      \
  (static_cast<void>(sizeof(JS::detail::CheckIsCharacterLiteral(s))), \
   reinterpret_cast<To>(s))

#define JS_CAST_INT32_TO(s, To)                            \
  (static_cast<void>(sizeof(JS::detail::CheckIsInt32(s))), \
   reinterpret_cast<To>(s))

#define JS_CHECK_ACCESSOR_FLAGS(flags)                                         \
  (static_cast<std::enable_if<((flags) & ~(JSPROP_ENUMERATE |                  \
                                           JSPROP_PERMANENT)) == 0>::type>(0), \
   (flags))

#define JS_PS_ACCESSOR_SPEC(name, getter, setter, flags, extraFlags) \
  {                                                                  \
    name, uint8_t(JS_CHECK_ACCESSOR_FLAGS(flags) | extraFlags), {    \
      { getter, setter }                                             \
    }                                                                \
  }
#define JS_PS_VALUE_SPEC(name, value, flags)          \
  {                                                   \
    name, uint8_t(flags | JSPROP_INTERNAL_USE_BIT), { \
      { value, JSNATIVE_WRAPPER(nullptr) }            \
    }                                                 \
  }

#define SELFHOSTED_WRAPPER(name)                           \
  {                                                        \
    { nullptr, JS_CAST_STRING_TO(name, const JSJitInfo*) } \
  }
#define STRINGVALUE_WRAPPER(value)                   \
  {                                                  \
    {                                                \
      reinterpret_cast<JSNative>(JSVAL_TYPE_STRING), \
          JS_CAST_STRING_TO(value, const JSJitInfo*) \
    }                                                \
  }
#define INT32VALUE_WRAPPER(value)                   \
  {                                                 \
    {                                               \
      reinterpret_cast<JSNative>(JSVAL_TYPE_INT32), \
          JS_CAST_INT32_TO(value, const JSJitInfo*) \
    }                                               \
  }

/*
 * JSPropertySpec uses JSNativeWrapper.  These macros encapsulate the definition
 * of JSNative-backed JSPropertySpecs, by defining the JSNativeWrappers for
 * them.
 */
#define JS_PSG(name, getter, flags)                   \
  JS_PS_ACCESSOR_SPEC(name, JSNATIVE_WRAPPER(getter), \
                      JSNATIVE_WRAPPER(nullptr), flags, 0)
#define JS_PSGS(name, getter, setter, flags)          \
  JS_PS_ACCESSOR_SPEC(name, JSNATIVE_WRAPPER(getter), \
                      JSNATIVE_WRAPPER(setter), flags, 0)
#define JS_SYM_GET(symbol, getter, flags)                                    \
  JS_PS_ACCESSOR_SPEC(                                                       \
      reinterpret_cast<const char*>(uint32_t(::JS::SymbolCode::symbol) + 1), \
      JSNATIVE_WRAPPER(getter), JSNATIVE_WRAPPER(nullptr), flags, 0)
#define JS_SELF_HOSTED_GET(name, getterName, flags)         \
  JS_PS_ACCESSOR_SPEC(name, SELFHOSTED_WRAPPER(getterName), \
                      JSNATIVE_WRAPPER(nullptr), flags, JSPROP_GETTER)
#define JS_SELF_HOSTED_GETSET(name, getterName, setterName, flags) \
  JS_PS_ACCESSOR_SPEC(name, SELFHOSTED_WRAPPER(getterName),        \
                      SELFHOSTED_WRAPPER(setterName), flags,       \
                      JSPROP_GETTER | JSPROP_SETTER)
#define JS_SELF_HOSTED_SYM_GET(symbol, getterName, flags)                    \
  JS_PS_ACCESSOR_SPEC(                                                       \
      reinterpret_cast<const char*>(uint32_t(::JS::SymbolCode::symbol) + 1), \
      SELFHOSTED_WRAPPER(getterName), JSNATIVE_WRAPPER(nullptr), flags,      \
      JSPROP_GETTER)
#define JS_STRING_PS(name, string, flags) \
  JS_PS_VALUE_SPEC(name, STRINGVALUE_WRAPPER(string), flags)
#define JS_STRING_SYM_PS(symbol, string, flags)                              \
  JS_PS_VALUE_SPEC(                                                          \
      reinterpret_cast<const char*>(uint32_t(::JS::SymbolCode::symbol) + 1), \
      STRINGVALUE_WRAPPER(string), flags)
#define JS_INT32_PS(name, value, flags) \
  JS_PS_VALUE_SPEC(name, INT32VALUE_WRAPPER(value), flags)
#define JS_PS_END                                         \
  JS_PS_ACCESSOR_SPEC(nullptr, JSNATIVE_WRAPPER(nullptr), \
                      JSNATIVE_WRAPPER(nullptr), 0, 0)

/**
 * To define a native function, set call to a JSNativeWrapper. To define a
 * self-hosted function, set selfHostedName to the name of a function
 * compiled during JSRuntime::initSelfHosting.
 */
struct JSFunctionSpec {
  const char* name;
  JSNativeWrapper call;
  uint16_t nargs;
  uint16_t flags;
  const char* selfHostedName;
};

/*
 * Terminating sentinel initializer to put at the end of a JSFunctionSpec array
 * that's passed to JS_DefineFunctions or JS_InitClass.
 */
#define JS_FS_END JS_FN(nullptr, nullptr, 0, 0)

/*
 * Initializer macros for a JSFunctionSpec array element. JS_FNINFO allows the
 * simple adding of JSJitInfos. JS_SELF_HOSTED_FN declares a self-hosted
 * function. JS_INLINABLE_FN allows specifying an InlinableNative enum value for
 * natives inlined or specialized by the JIT. Finally JS_FNSPEC has slots for
 * all the fields.
 *
 * The _SYM variants allow defining a function with a symbol key rather than a
 * string key. For example, use JS_SYM_FN(iterator, ...) to define an
 * @@iterator method.
 */
#define JS_FN(name, call, nargs, flags) \
  JS_FNSPEC(name, call, nullptr, nargs, flags, nullptr)
#define JS_INLINABLE_FN(name, call, nargs, flags, native) \
  JS_FNSPEC(name, call, &js::jit::JitInfo_##native, nargs, flags, nullptr)
#define JS_SYM_FN(symbol, call, nargs, flags) \
  JS_SYM_FNSPEC(symbol, call, nullptr, nargs, flags, nullptr)
#define JS_FNINFO(name, call, info, nargs, flags) \
  JS_FNSPEC(name, call, info, nargs, flags, nullptr)
#define JS_SELF_HOSTED_FN(name, selfHostedName, nargs, flags) \
  JS_FNSPEC(name, nullptr, nullptr, nargs, flags, selfHostedName)
#define JS_SELF_HOSTED_SYM_FN(symbol, selfHostedName, nargs, flags) \
  JS_SYM_FNSPEC(symbol, nullptr, nullptr, nargs, flags, selfHostedName)
#define JS_SYM_FNSPEC(symbol, call, info, nargs, flags, selfHostedName)      \
  JS_FNSPEC(                                                                 \
      reinterpret_cast<const char*>(uint32_t(::JS::SymbolCode::symbol) + 1), \
      call, info, nargs, flags, selfHostedName)
#define JS_FNSPEC(name, call, info, nargs, flags, selfHostedName) \
  { name, {call, info}, nargs, flags, selfHostedName }

#endif  // js_PropertySpec_h
