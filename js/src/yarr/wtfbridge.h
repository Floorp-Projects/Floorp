/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99 ft=cpp: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jswtfbridge_h__
#define jswtfbridge_h__

/*
 * WTF compatibility layer. This file provides various type and data
 * definitions for use by Yarr.
 */

#include <stdio.h>
#include <stdarg.h>
#include "jsstr.h"
#include "jsprvtd.h"
#include "vm/String.h"
#include "assembler/wtf/Platform.h"
#include "assembler/jit/ExecutableAllocator.h"
#include "CheckedArithmetic.h"
#include "js/TemplateLib.h"

namespace JSC { namespace Yarr {

/*
 * Basic type definitions.
 */

typedef char LChar;
typedef jschar UChar;
typedef JSLinearString UString;
typedef JSLinearString String;


class Unicode {
  public:
    static UChar toUpper(UChar c) { return js::unicode::ToUpperCase(c); }
    static UChar toLower(UChar c) { return js::unicode::ToLowerCase(c); }
};

/*
 * Do-nothing smart pointer classes. These have a compatible interface
 * with the smart pointers used by Yarr, but they don't actually do
 * reference counting.
 */
template<typename T>
class RefCounted {
};

template<typename T>
class RefPtr {
    T *ptr;
  public:
    RefPtr(T *p) { ptr = p; }
    operator bool() const { return ptr != NULL; }
    const T *operator ->() const { return ptr; }
    T *get() { return ptr; }
};

template<typename T>
class PassRefPtr {
    T *ptr;
  public:
    PassRefPtr(T *p) { ptr = p; }
    operator T*() { return ptr; }
};

template<typename T>
class PassOwnPtr {
    T *ptr;
  public:
    PassOwnPtr(T *p) { ptr = p; }

    T *get() { return ptr; }
};

template<typename T>
class OwnPtr {
    T *ptr;
  public:
    OwnPtr() : ptr(NULL) { }
    OwnPtr(PassOwnPtr<T> p) : ptr(p.get()) { }

    ~OwnPtr() {
        if (ptr)
            js_delete(ptr);
    }

    OwnPtr<T> &operator=(PassOwnPtr<T> p) {
        ptr = p.get();
        return *this;
    }

    T *operator ->() { return ptr; }

    T *get() { return ptr; }

    T *release() {
        T *result = ptr;
        ptr = NULL;
        return result;
    }
};

template<typename T>
PassRefPtr<T> adoptRef(T *p) { return PassRefPtr<T>(p); }

template<typename T>
PassOwnPtr<T> adoptPtr(T *p) { return PassOwnPtr<T>(p); }

// Dummy wrapper.
#define WTF_MAKE_FAST_ALLOCATED void make_fast_allocated_()

template<typename T>
class Ref {
    T &val;
  public:
    Ref(T &val) : val(val) { }
    operator T&() const { return val; }
};

/*
 * Vector class for Yarr. This wraps js::Vector and provides all
 * the API method signatures used by Yarr.
 */
template<typename T, size_t N = 0>
class Vector {
  public:
    js::Vector<T, N, js::SystemAllocPolicy> impl;
  public:
    Vector() {}

    Vector(const Vector &v) {
        // XXX yarr-oom
        (void) append(v);
    }

    size_t size() const {
        return impl.length();
    }

    T &operator[](size_t i) {
        return impl[i];
    }

    const T &operator[](size_t i) const {
        return impl[i];
    }

    T &at(size_t i) {
        return impl[i];
    }

    const T *begin() const {
        return impl.begin();
    }

    T &last() {
        return impl.back();
    }

    bool isEmpty() const {
        return impl.empty();
    }

    template <typename U>
    void append(const U &u) {
        // XXX yarr-oom
        (void) impl.append(static_cast<T>(u));
    }

    template <size_t M>
    void append(const Vector<T,M> &v) {
        // XXX yarr-oom
        (void) impl.append(v.impl);
    }

    void insert(size_t i, const T& t) {
        // XXX yarr-oom
        (void) impl.insert(&impl[i], t);
    }

    void remove(size_t i) {
        impl.erase(&impl[i]);
    }

    void clear() {
        return impl.clear();
    }

    void shrink(size_t newLength) {
        // XXX yarr-oom
        JS_ASSERT(newLength <= impl.length());
        (void) impl.resize(newLength);
    }

    void swap(Vector &other) {
        impl.swap(other.impl);
    }

    void deleteAllValues() {
        for (T *p = impl.begin(); p != impl.end(); ++p)
            js_delete(*p);
    }

    bool reserve(size_t capacity) {
        return impl.reserve(capacity);
    }
};

template<typename T>
class Vector<OwnPtr<T> > {
  public:
    js::Vector<T *, 0, js::SystemAllocPolicy> impl;
  public:
    Vector() {}

    size_t size() const {
        return impl.length();
    }

    void append(T *t) {
        // XXX yarr-oom
        (void) impl.append(t);
    }

    PassOwnPtr<T> operator[](size_t i) {
        return PassOwnPtr<T>(impl[i]);
    }

    void clear() {
        for (T **p = impl.begin(); p != impl.end(); ++p)
            delete_(*p);
        return impl.clear();
    }

    void reserve(size_t capacity) {
        // XXX yarr-oom
        (void) impl.reserve(capacity);
    }
};

template <typename T, size_t N>
inline void
deleteAllValues(Vector<T, N> &v) {
    v.deleteAllValues();
}

static inline void
dataLog(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}

#if ENABLE_YARR_JIT

/*
 * Minimal JSGlobalData. This used by Yarr to get the allocator.
 */
class JSGlobalData {
  public:
    ExecutableAllocator *regexAllocator;

    JSGlobalData(ExecutableAllocator *regexAllocator)
     : regexAllocator(regexAllocator) { }
};

#endif

 /*
  * Do-nothing version of a macro used by WTF to avoid unused
  * parameter warnings.
  */
#define UNUSED_PARAM(e)

} /* namespace Yarr */

/*
 * Replacements for std:: functions used in Yarr. We put them in 
 * namespace JSC::std so that they can still be called as std::X
 * in Yarr.
 */
namespace std {

/*
 * windows.h defines a 'min' macro that would mangle the function
 * name.
 */
#if WTF_COMPILER_MSVC
# undef min
# undef max
#endif

#define NO_RETURN_DUE_TO_ASSERT

template<typename T>
inline T
min(T t1, T t2)
{
    return js::Min(t1, t2);
}

template<typename T>
inline T
max(T t1, T t2)
{
    return js::Max(t1, t2);
}

template<typename T>
inline void
swap(T &t1, T &t2)
{
    T tmp = t1;
    t1 = t2;
    t2 = tmp;
}
} /* namespace std */

} /* namespace JSC */

namespace WTF {

/*
 * Sentinel value used in Yarr.
 */
const size_t notFound = size_t(-1);

}

#define JS_EXPORT_PRIVATE

#endif
