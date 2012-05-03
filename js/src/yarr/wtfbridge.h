/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99 ft=cpp:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla SpiderMonkey JavaScript 1.9 code, released
 * June 12, 2009.
 *
 * The Initial Developer of the Original Code is
 *   the Mozilla Corporation.
 *
 * Contributor(s):
 *   David Mandelin <dmandelin@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef jswtfbridge_h__
#define jswtfbridge_h__

/*
 * WTF compatibility layer. This file provides various type and data
 * definitions for use by Yarr.
 */

#include "jsstr.h"
#include "jsprvtd.h"
#include "vm/String.h"
#include "assembler/wtf/Platform.h"
#include "assembler/jit/ExecutableAllocator.h"

namespace JSC { namespace Yarr {

/*
 * Basic type definitions.
 */

typedef jschar UChar;
typedef JSLinearString UString;

using namespace js::unicode;

class Unicode {
  public:
    static UChar toUpper(UChar c) { return ToUpperCase(c); }
    static UChar toLower(UChar c) { return ToLowerCase(c); }
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
            js::Foreground::delete_(ptr);
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

#define WTF_MAKE_FAST_ALLOCATED

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

    void deleteAllValues() {
        for (T *p = impl.begin(); p != impl.end(); ++p)
            js::Foreground::delete_(*p);
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
            js::Foreground::delete_(*p);
        return impl.clear();
    }
};

template <typename T, size_t N>
inline void
deleteAllValues(Vector<T, N> &v) {
    v.deleteAllValues();
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
 * Sentinel value used in Yarr.
 */
const size_t notFound = size_t(-1);

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

template<typename T>
inline T
min(T t1, T t2)
{
    return JS_MIN(t1, t2);
}

template<typename T>
inline T
max(T t1, T t2)
{
    return JS_MAX(t1, t2);
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

#endif
