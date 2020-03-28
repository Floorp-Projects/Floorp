/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DDLoggedTypeTraits_h_
#define DDLoggedTypeTraits_h_

#include <type_traits>

#include "mozilla/TypeTraits.h"

namespace mozilla {

// Templated struct carrying compile-time information about C++ types that may
// log messages (including about their lifetime and links to other objects.)
// Classes should declare a specialization by using one of the macros below.
template <typename T>
struct DDLoggedTypeTraits;

#define DDLoggedTypeName(TYPE)                                 \
  template <>                                                  \
  struct DDLoggedTypeTraits<TYPE> {                            \
    using Type = TYPE;                                         \
    static constexpr const char* Name() { return #TYPE; }      \
    using HasBase = std::false_type;                           \
    using BaseType = TYPE;                                     \
    static constexpr const char* BaseTypeName() { return ""; } \
  }

#define DDLoggedTypeNameAndBase(TYPE, BASE)               \
  template <>                                             \
  struct DDLoggedTypeTraits<TYPE> {                       \
    using Type = TYPE;                                    \
    static constexpr const char* Name() { return #TYPE; } \
    using HasBase = TrueType;                             \
    using BaseType = BASE;                                \
    static constexpr const char* BaseTypeName() {         \
      return DDLoggedTypeTraits<BASE>::Name();            \
    }                                                     \
  }

#define DDLoggedTypeCustomName(TYPE, NAME)                     \
  template <>                                                  \
  struct DDLoggedTypeTraits<TYPE> {                            \
    using Type = TYPE;                                         \
    static constexpr const char* Name() { return #NAME; }      \
    using HasBase = std::false_type;                           \
    using BaseType = TYPE;                                     \
    static constexpr const char* BaseTypeName() { return ""; } \
  }

#define DDLoggedTypeCustomNameAndBase(TYPE, NAME, BASE)   \
  template <>                                             \
  struct DDLoggedTypeTraits<TYPE> {                       \
    using Type = TYPE;                                    \
    static constexpr const char* Name() { return #NAME; } \
    using HasBase = TrueType;                             \
    using BaseType = BASE;                                \
    static constexpr const char* BaseTypeName() {         \
      return DDLoggedTypeTraits<BASE>::Name();            \
    }                                                     \
  }

// Variants with built-in forward-declaration, will only work
// - in mozilla namespace,
// - with unqualified mozilla-namespace type names.
#define DDLoggedTypeDeclName(TYPE) \
  class TYPE;                      \
  DDLoggedTypeName(TYPE);
#define DDLoggedTypeDeclNameAndBase(TYPE, BASE) \
  class TYPE;                                   \
  DDLoggedTypeNameAndBase(TYPE, BASE);
#define DDLoggedTypeDeclCustomName(TYPE, NAME) \
  class TYPE;                                  \
  DDLoggedTypeCustomName(TYPE, NAME);
#define DDLoggedTypeDeclCustomNameAndBase(TYPE, NAME, BASE) \
  class TYPE;                                               \
  DDLoggedTypeCustomNameAndBase(TYPE, NAME, BASE);

}  // namespace mozilla

// Some essential types that live outside of the media stack.
class nsPIDOMWindowInner;
class nsPIDOMWindowOuter;

namespace mozilla {

namespace dom {
class Document;
class HTMLAudioElement;
class HTMLMediaElement;
class HTMLVideoElement;
}  // namespace dom

DDLoggedTypeName(nsPIDOMWindowInner);
DDLoggedTypeName(nsPIDOMWindowOuter);
DDLoggedTypeName(dom::Document);
DDLoggedTypeName(dom::HTMLMediaElement);
DDLoggedTypeNameAndBase(dom::HTMLAudioElement, dom::HTMLMediaElement);
DDLoggedTypeNameAndBase(dom::HTMLVideoElement, dom::HTMLMediaElement);

}  // namespace mozilla

#endif  // DDLoggedTypeTraits_h_
