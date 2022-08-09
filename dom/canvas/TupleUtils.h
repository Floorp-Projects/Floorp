/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_CANVAS_TUPLE_UTILS_H
#define DOM_CANVAS_TUPLE_UTILS_H

#include <tuple>

namespace mozilla {

// -
// ToTupleOfConstRefs

template <class T>
constexpr const auto& ToConstRef1(T& ret) {
  return ret;
}

template <class Tup, size_t... Ids>
constexpr auto ToTupleOfConstRefsN(const Tup& tup,
                                   const std::index_sequence<Ids...>&) {
  return std::tie(ToConstRef1(std::get<Ids>(tup))...);
}

template <class... Args, size_t... Ids, class Tup = std::tuple<Args...>>
constexpr auto ToTupleOfConstRefs(const Tup& tup) {
  return ToTupleOfConstRefsN(
      tup, std::make_index_sequence<std::tuple_size_v<Tup>>());
}

// -
// MapTuple

template <class Tup, class Callable, size_t... Ids>
constexpr auto MapTupleN(Tup&& tup, Callable&& fn,
                         const std::index_sequence<Ids...>&) {
  return std::make_tuple(fn(std::get<Ids>(tup))...);
}

/// Callable::operator()(T) cannot return void or you will get weird errors.
template <class... Args, class Callable, class Tup = std::tuple<Args...>>
constexpr auto MapTuple(Tup&& t, Callable&& fn) {
  using Tup2 = typename std::remove_reference<Tup>::type;
  return MapTupleN(t, fn, std::make_index_sequence<std::tuple_size_v<Tup2>>());
}

// -

template <class... Args>
constexpr auto SizeofTupleArgs(const std::tuple<Args...>&) {
  size_t total = 0;
  for (const auto s : {sizeof(Args)...}) {
    total += s;
  }
  return total;
}

}  // namespace mozilla

#endif  // DOM_CANVAS_TUPLE_UTILS_H
