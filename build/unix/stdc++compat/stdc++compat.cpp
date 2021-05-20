/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <ostream>
#include <istream>
#include <sstream>
#include <memory>
#include <string>
#include <stdarg.h>
#include <stdio.h>
#include <mozilla/Assertions.h>
#include <cxxabi.h>

/*
   GLIBCXX_3.4.19 is from gcc 4.8.1 (199309)
   GLIBCXX_3.4.20 is from gcc 4.9.0 (199307)
   GLIBCXX_3.4.21 is from gcc 5.0 (210290)
   GLIBCXX_3.4.22 is from gcc 6.0 (222482)
   GLIBCXX_3.4.23 is from gcc 7
   GLIBCXX_3.4.24 is from gcc 8
   GLIBCXX_3.4.25 is from gcc 8
   GLIBCXX_3.4.26 is from gcc 9
   GLIBCXX_3.4.27 is from gcc 10
   GLIBCXX_3.4.28 is from gcc 10
   GLIBCXX_3.4.29 is from gcc 11

This file adds the necessary compatibility tricks to avoid symbols with
version GLIBCXX_3.4.20 and bigger, keeping binary compatibility with
libstdc++ 4.8.1.

WARNING: all symbols from this file must be defined weak when they
overlap with libstdc++.
*/

#define GLIBCXX_VERSION(a, b, c) (((a) << 16) | ((b) << 8) | (c))

#if MOZ_LIBSTDCXX_VERSION >= GLIBCXX_VERSION(3, 4, 20)
namespace std {

/* We shouldn't be throwing exceptions at all, but it sadly turns out
   we call STL (inline) functions that do. */
void __attribute__((weak)) __throw_out_of_range_fmt(char const* fmt, ...) {
  va_list ap;
  char buf[1024];  // That should be big enough.

  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  buf[sizeof(buf) - 1] = 0;
  va_end(ap);

  __throw_range_error(buf);
}

}  // namespace std
#endif

#if MOZ_LIBSTDCXX_VERSION >= GLIBCXX_VERSION(3, 4, 20)
namespace __cxxabiv1 {
/* Technically, this symbol is not in GLIBCXX_3.4.20, but in CXXABI_1.3.8,
   but that's equivalent, version-wise. Those calls are added by the compiler
   itself on `new Class[n]` calls. */
extern "C" void __attribute__((weak)) __cxa_throw_bad_array_new_length() {
  MOZ_CRASH();
}
}  // namespace __cxxabiv1
#endif

#if MOZ_LIBSTDCXX_VERSION >= GLIBCXX_VERSION(3, 4, 29)
namespace std {

void __attribute__((weak)) __throw_bad_array_new_length() { MOZ_CRASH(); }

}  // namespace std
#endif

#if MOZ_LIBSTDCXX_VERSION >= GLIBCXX_VERSION(3, 4, 21)
/* While we generally don't build with exceptions, we have some host tools
 * that do use them. libstdc++ from GCC 5.0 added exception constructors with
 * char const* argument. Older versions only have a constructor with
 * std::string. */
namespace std {
__attribute__((weak)) runtime_error::runtime_error(char const* s)
    : runtime_error(std::string(s)) {}
}  // namespace std
#endif

#if MOZ_LIBSTDCXX_VERSION >= GLIBCXX_VERSION(3, 4, 21)
/* Expose the definitions for the old ABI, allowing us to call its functions */
#  define _GLIBCXX_THREAD_ABI_COMPAT 1
#  include <thread>

namespace std {
/* The old ABI has a thread::_M_start_thread(shared_ptr<_Impl_base>),
 * while the new has thread::_M_start_thread(unique_ptr<_State>, void(*)()).
 * There is an intermediate ABI at version 3.4.21, with
 * thread::_M_start_thread(shared_ptr<_Impl_base>, void(*)()).
 * The void(*)() parameter is only there to keep a reference to pthread_create
 * on the caller side, and is unused in the implementation
 * We're creating an entry point for the new and intermediate ABIs, and make
 * them call the old ABI. */

__attribute__((weak)) void thread::_M_start_thread(shared_ptr<_Impl_base> impl,
                                                   void (*)()) {
  _M_start_thread(std::move(impl));
}

#  if MOZ_LIBSTDCXX_VERSION >= GLIBCXX_VERSION(3, 4, 22)
/* We need a _Impl_base-derived class wrapping a _State to call the old ABI
 * from what we got by diverting the new API */
struct StateWrapper : public thread::_Impl_base {
  unique_ptr<thread::_State> mState;

  StateWrapper(unique_ptr<thread::_State> aState) : mState(std::move(aState)) {}

  void _M_run() override { mState->_M_run(); }
};

__attribute__((weak)) void thread::_M_start_thread(unique_ptr<_State> aState,
                                                   void (*)()) {
  auto impl = std::make_shared<StateWrapper>(std::move(aState));
  _M_start_thread(std::move(impl));
}

/* For some reason this is a symbol exported by new versions of libstdc++,
 * even though the destructor is default there too */
__attribute__((weak)) thread::_State::~_State() = default;
#  endif

#  if MOZ_LIBSTDCXX_VERSION >= GLIBCXX_VERSION(3, 4, 26)
// Ideally we'd define
//    bool _Sp_make_shared_tag::_S_eq(const type_info& ti) noexcept
// but we wouldn't be able to change its visibility because of the existing
// definition in C++ headers. We do need to change its visibility because we
// don't want it to be shadowing the one provided by libstdc++ itself, because
// it doesn't support RTTI. Not supporting RTTI doesn't matter for Firefox
// itself because it's built with RTTI disabled.
// So we define via the mangled symbol.
extern "C" __attribute__((visibility("hidden"))) bool
_ZNSt19_Sp_make_shared_tag5_S_eqERKSt9type_info(const type_info*) noexcept {
  return false;
}
#  endif

}  // namespace std
#endif

#if MOZ_LIBSTDCXX_VERSION >= GLIBCXX_VERSION(3, 4, 21)
namespace std {
/* Instantiate this template to avoid GLIBCXX_3.4.21 symbol versions
 * depending on optimization level */
template basic_ios<char, char_traits<char>>::operator bool() const;
}  // namespace std

#  if !defined(MOZ_ASAN) && !defined(MOZ_TSAN)
/* operator delete with size is only available in CXXAPI_1.3.9, equivalent to
 * GLIBCXX_3.4.21. */
void operator delete(void* ptr, size_t size) noexcept(true) {
  ::operator delete(ptr);
}
#  endif
#endif

#if MOZ_LIBSTDCXX_VERSION >= GLIBCXX_VERSION(3, 4, 23)
namespace std {
/* Instantiate this template to avoid GLIBCXX_3.4.23 symbol versions
 * depending on optimization level */
template basic_string<char, char_traits<char>, allocator<char>>::basic_string(
    const basic_string&, size_t, const allocator<char>&);

#  if MOZ_LIBSTDCXX_VERSION >= GLIBCXX_VERSION(3, 4, 26)
template basic_stringstream<char, char_traits<char>,
                            allocator<char>>::basic_stringstream();

template basic_ostringstream<char, char_traits<char>,
                             allocator<char>>::basic_ostringstream();
#  endif

#  if MOZ_LIBSTDCXX_VERSION >= GLIBCXX_VERSION(3, 4, 29)
template void basic_string<char, char_traits<char>, allocator<char>>::reserve();

template void
basic_string<wchar_t, char_traits<wchar_t>, allocator<wchar_t>>::reserve();
#  endif

}  // namespace std
#endif

/* The __cxa_thread_atexit_impl symbol is only available on GLIBC 2.18, but we
 * want things to keep working on 2.17. It's not actually used directly from
 * C++ code, but through __cxa_thead_atexit in libstdc++. The problem we have,
 * though, is that rust's libstd also uses it, introducing a dependency we
 * don't actually want. Fortunately, we can fall back to libstdc++'s wrapper
 * (which, on systems without __cxa_thread_atexit_impl, has its own compatible
 * implementation).
 * The __cxa_thread_atexit symbol itself is marked CXXABI_1.3.7, which is
 * equivalent to GLIBCXX_3.4.18.
 */
extern "C" int __cxa_thread_atexit_impl(void (*dtor)(void*), void* obj,
                                        void* dso_handle) {
  return __cxxabiv1::__cxa_thread_atexit(dtor, obj, dso_handle);
}
