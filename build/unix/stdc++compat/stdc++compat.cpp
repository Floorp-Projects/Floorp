/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <ostream>
#include <istream>
#include <string>
#include <stdarg.h>
#include <stdio.h>
#include <mozilla/Assertions.h>

/* GLIBCXX_3.4.16 is from gcc 4.6.1 (172240)
   GLIBCXX_3.4.17 is from gcc 4.7.0 (174383)
   GLIBCXX_3.4.18 is from gcc 4.8.0 (190787)
   GLIBCXX_3.4.19 is from gcc 4.8.1 (199309)
   GLIBCXX_3.4.20 is from gcc 4.9.0 (199307)
   GLIBCXX_3.4.21 is from gcc 5.0 (210290)
   GLIBCXX_3.4.22 is from gcc 6.0 (222482)
   GLIBCXX_3.4.23 is from gcc 7.0

This file adds the necessary compatibility tricks to avoid symbols with
version GLIBCXX_3.4.17 and bigger, keeping binary compatibility with
libstdc++ 4.7.

WARNING: all symbols from this file must be defined weak when they
overlap with libstdc++.
*/

#define GLIBCXX_VERSION(a, b, c) (((a) << 16) | ((b) << 8) | (c))

#if MOZ_LIBSTDCXX_VERSION >= GLIBCXX_VERSION(3, 4, 18)
// Implementation of utility functions for the prime rehash policy used in
// unordered_map and unordered_set.
#  include <unordered_map>
#  include <tr1/unordered_map>
namespace std {
size_t __attribute__((weak))
__detail::_Prime_rehash_policy::_M_next_bkt(size_t __n) const {
  tr1::__detail::_Prime_rehash_policy policy(_M_max_load_factor);
  size_t ret = policy._M_next_bkt(__n);
  _M_next_resize = policy._M_next_resize;
  return ret;
}

pair<bool, size_t> __attribute__((weak))
__detail::_Prime_rehash_policy::_M_need_rehash(size_t __n_bkt, size_t __n_elt,
                                               size_t __n_ins) const {
  tr1::__detail::_Prime_rehash_policy policy(_M_max_load_factor);
  policy._M_next_resize = _M_next_resize;
  pair<bool, size_t> ret = policy._M_need_rehash(__n_bkt, __n_elt, __n_ins);
  _M_next_resize = policy._M_next_resize;
  return ret;
}
}  // namespace std
#endif

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
/* Technically, this symbol is not in GLIBCXX_3.4.20, but in CXXABI_1.3.8,
   but that's equivalent, version-wise. Those calls are added by the compiler
   itself on `new Class[n]` calls. */
extern "C" void __attribute__((weak)) __cxa_throw_bad_array_new_length() {
  MOZ_CRASH();
}
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
}  // namespace std
#endif

#if MOZ_LIBSTDCXX_VERSION >= GLIBCXX_VERSION(3, 4, 21)
namespace std {
/* Instantiate this template to avoid GLIBCXX_3.4.21 symbol versions
 * depending on optimization level */
template basic_ios<char, char_traits<char> >::operator bool() const;
}  // namespace std
#endif

#if MOZ_LIBSTDCXX_VERSION >= GLIBCXX_VERSION(3, 4, 23)
namespace std {
/* Instantiate this template to avoid GLIBCXX_3.4.23 symbol versions
 * depending on optimization level */
template basic_string<char, char_traits<char>, allocator<char>>::basic_string(const basic_string&, size_t, const allocator<char>&);
} // namespace std
#endif
