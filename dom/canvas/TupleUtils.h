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

template <class Tup>
struct SizeofTupleArgs;

// c++17 fold expressions make this easy, once we pull out the Args parameter
// pack by constraining the default template:
template <class... Args>
struct SizeofTupleArgs<std::tuple<Args...>>
    : std::integral_constant<size_t, (... + sizeof(Args))> {};

static_assert(SizeofTupleArgs<std::tuple<size_t, char, char, char>>::value ==
              sizeof(size_t) + 3);

}  // namespace mozilla

#endif  // DOM_CANVAS_TUPLE_UTILS_H
