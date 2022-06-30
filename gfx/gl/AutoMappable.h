/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_AUTO_MAPPABLE_H
#define MOZILLA_AUTO_MAPPABLE_H

// Here be dragons.

#include <functional>

namespace mozilla::gfx {

template<class T>
size_t Hash(const T&);

template<class T>
struct StaticStdHasher {
    static auto HashImpl(const T& v) {
        return std::hash<T>()(v);
    }
};

template<class T>
struct StaticHasher {
    static auto HashImpl(const T& v) {
        return v.hash();
    }
};
template<class T>
struct StaticHasher<std::optional<T>> {
    static size_t HashImpl(const std::optional<T>& v) {
       if (!v) return 0;
       return Hash(*v);
    }
};
template<> struct StaticHasher<int> : public StaticStdHasher<int> {};
template<> struct StaticHasher<bool> : public StaticStdHasher<bool> {};
template<> struct StaticHasher<float> : public StaticStdHasher<float> {};

template<class T>
size_t Hash(const T& v) {
    return StaticHasher<T>::HashImpl(v);
}

//-
// From Boost: https://www.boost.org/doc/libs/1_37_0/doc/html/hash/reference.html#boost.hash_combine

inline size_t HashCombine(size_t seed, const size_t hash) {
    seed ^= hash + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    return seed;
}

// -
// See https://codereview.stackexchange.com/questions/136770/hashing-a-tuple-in-c17

template<class... Args, size_t... Ids>
size_t HashTupleN(const std::tuple<Args...>& tup, const std::index_sequence<Ids...>&) {
   size_t seed = 0;
   for (const auto& hash : {Hash(std::get<Ids>(tup))...}) {
       seed = HashCombine(seed, hash);
   }
   return seed;
}

template<class... Args>
size_t HashTuple(const std::tuple<Args...>& tup) {
    return HashTupleN(tup, std::make_index_sequence<sizeof...(Args)>());
}

// -

template<class T>
auto MembersEq(const T& a, const T& b) {
    const auto atup = a.Members();
    const auto btup = b.Members();
    return atup == btup;
}

template<class T>
auto MembersLt(const T& a, const T& b) {
    const auto atup = a.Members();
    const auto btup = b.Members();
    return atup == btup;
}

template<class T>
auto MembersHash(const T& a) {
    const auto atup = a.Members();
    return HashTuple(atup);
}

template<class T>
struct MembersHasher final {
    auto operator()(const T& v) const {
        return v.hash();
    }
};

/** E.g.:
struct Foo {
    int i;
    bool b;

   auto Members() const { return std::tie(i, b); }
    INLINE_AUTO_MAPPABLE(Foo)
};
std::unordered_set<T, T::Hasher> easy;
**/
#define INLINE_DERIVE_MEMBERS_EQ(T) \
    friend bool operator==(const T& a, const T& b) { \
        return mozilla::gfx::MembersEq(a, b); \
    } \
    friend bool operator!=(const T& a, const T& b) { \
        return !operator==(a, b); \
    }
#define INLINE_AUTO_MAPPABLE(T) \
    friend bool operator<(const T& a, const T& b) { \
        return mozilla::gfx::MembersLt(a, b); \
    } \
    INLINE_DERIVE_MEMBERS_EQ(T) \
    size_t hash() const { \
        return mozilla::gfx::MembersHash(*reinterpret_cast<const T*>(this)); \
    } \
    using Hasher = mozilla::gfx::MembersHasher<T>;

// -

/** E.g.:
```
struct Foo : public AutoMappable<Foo> {
    int i;
    bool b;

    auto Members() const { return std::tie(i, b); }
};
std::unordered_set<T, T::Hasher> easy;
```
`easy.insert({{}, 2, true});`
The initial {} is needed for aggregate initialization of AutoMappable<Foo>.
Use INLINE_AUTO_MAPPABLE if this is too annoying.
**/
template<class T>
struct AutoMappable {
    INLINE_AUTO_MAPPABLE(T)
};

}  // namespace mozilla::gfx

#endif  // MOZILLA_AUTO_MAPPABLE_H
