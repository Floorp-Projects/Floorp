/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_AUTO_MAPPABLE_H
#define MOZILLA_AUTO_MAPPABLE_H

// Here be dragons.

#include <functional>
#include <tuple>

namespace mozilla {

template <class T>
size_t StdHash(const T& t) {
  return std::hash<T>()(t);
}

//-
// From Boost:
// https://www.boost.org/doc/libs/1_37_0/doc/html/hash/reference.html#boost.hash_combine

inline size_t HashCombine(size_t seed, const size_t hash) {
  seed ^= hash + 0x9e3779b9 + (seed << 6) + (seed >> 2);
  return seed;
}

// -

namespace detail {
template <class... Args, size_t... Ids>
size_t StdHashTupleN(const std::tuple<Args...>& tup,
                     const std::index_sequence<Ids...>&) {
  size_t seed = 0;
  for (const auto& hash : {StdHash(std::get<Ids>(tup))...}) {
    seed = HashCombine(seed, hash);
  }
  return seed;
}
}  // namespace detail

// -

template <class T>
struct StdHashMembers {
  size_t operator()(const T& t) const {
    const auto members = t.Members();
    return StdHash(members);
  }
};

// -

#define MOZ_MIXIN_DERIVE_CMP_OP_BY_MEMBERS(OP, T) \
  bool operator OP(const T& rhs) const { return Members() OP rhs.Members(); }

#define MOZ_MIXIN_DERIVE_CMP_OPS_BY_MEMBERS(T) \
  MOZ_MIXIN_DERIVE_CMP_OP_BY_MEMBERS(==, T)    \
  MOZ_MIXIN_DERIVE_CMP_OP_BY_MEMBERS(!=, T)    \
  MOZ_MIXIN_DERIVE_CMP_OP_BY_MEMBERS(<, T)     \
  MOZ_MIXIN_DERIVE_CMP_OP_BY_MEMBERS(<=, T)    \
  MOZ_MIXIN_DERIVE_CMP_OP_BY_MEMBERS(>, T)     \
  MOZ_MIXIN_DERIVE_CMP_OP_BY_MEMBERS(>=, T)

template <class T>
struct DeriveCmpOpMembers {
 private:
  auto Members() const { return reinterpret_cast<const T*>(this)->Members(); }

 public:
  MOZ_MIXIN_DERIVE_CMP_OPS_BY_MEMBERS(T)
};

}  // namespace mozilla

template <class... Args>
struct std::hash<std::tuple<Args...>> {
  size_t operator()(const std::tuple<Args...>& t) const {
    return mozilla::detail::StdHashTupleN(
        t, std::make_index_sequence<sizeof...(Args)>());
  }
};

#endif  // MOZILLA_AUTO_MAPPABLE_H
