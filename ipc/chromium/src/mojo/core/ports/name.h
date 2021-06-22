// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_PORTS_NAME_H_
#define MOJO_CORE_PORTS_NAME_H_

#include <stdint.h>

#include <ostream>
#include <tuple>

#include "base/logging.h"
#include "mozilla/HashFunctions.h"
#include "nsHashKeys.h"

namespace mojo {
namespace core {
namespace ports {

struct Name {
  constexpr Name(uint64_t v1, uint64_t v2) : v1(v1), v2(v2) {}
  uint64_t v1, v2;
};

inline bool operator==(const Name& a, const Name& b) {
  return a.v1 == b.v1 && a.v2 == b.v2;
}

inline bool operator!=(const Name& a, const Name& b) { return !(a == b); }

inline bool operator<(const Name& a, const Name& b) {
  return std::tie(a.v1, a.v2) < std::tie(b.v1, b.v2);
}

std::ostream& operator<<(std::ostream& stream, const Name& name);
mozilla::Logger& operator<<(mozilla::Logger& log, const Name& name);

struct PortName : Name {
  constexpr PortName() : Name(0, 0) {}
  constexpr PortName(uint64_t v1, uint64_t v2) : Name(v1, v2) {}
};

constexpr PortName kInvalidPortName{0, 0};

struct NodeName : Name {
  constexpr NodeName() : Name(0, 0) {}
  constexpr NodeName(uint64_t v1, uint64_t v2) : Name(v1, v2) {}
};

constexpr NodeName kInvalidNodeName{0, 0};

}  // namespace ports
}  // namespace core
}  // namespace mojo

namespace mozilla {

template <>
inline PLDHashNumber Hash<mojo::core::ports::PortName>(
    const mojo::core::ports::PortName& aValue) {
  return mozilla::HashGeneric(aValue.v1, aValue.v2);
}

template <>
inline PLDHashNumber Hash<mojo::core::ports::NodeName>(
    const mojo::core::ports::NodeName& aValue) {
  return mozilla::HashGeneric(aValue.v1, aValue.v2);
}

using PortNameHashKey = nsGenericHashKey<mojo::core::ports::PortName>;
using NodeNameHashKey = nsGenericHashKey<mojo::core::ports::NodeName>;

}  // namespace mozilla

namespace std {

template <>
struct hash<mojo::core::ports::PortName> {
  std::size_t operator()(const mojo::core::ports::PortName& name) const {
    // FIXME: PLDHashNumber is only 32-bits
    return mozilla::Hash(name);
  }
};

template <>
struct hash<mojo::core::ports::NodeName> {
  std::size_t operator()(const mojo::core::ports::NodeName& name) const {
    // FIXME: PLDHashNumber is only 32-bits
    return mozilla::Hash(name);
  }
};

}  // namespace std

class PickleIterator;

namespace IPC {

template <typename T>
struct ParamTraits;
class Message;

template <>
struct ParamTraits<mojo::core::ports::PortName> {
  using paramType = mojo::core::ports::PortName;
  static void Write(Message* aMsg, const paramType& aParam);
  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult);
};

template <>
struct ParamTraits<mojo::core::ports::NodeName> {
  using paramType = mojo::core::ports::NodeName;
  static void Write(Message* aMsg, const paramType& aParam);
  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult);
};

}  // namespace IPC

#endif  // MOJO_CORE_PORTS_NAME_H_
