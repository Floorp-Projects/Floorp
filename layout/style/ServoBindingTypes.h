/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ServoBindingTypes_h
#define mozilla_ServoBindingTypes_h

#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtr.h"

struct RawServoStyleSet;

#define SERVO_ARC_TYPE(name_, type_) struct type_;
#include "mozilla/ServoArcTypeList.h"
#undef SERVO_ARC_TYPE

namespace mozilla {
  class ServoElementSnapshot;
namespace dom {
class Element;
class StyleChildrenIterator;
} // namespace dom
} // namespace mozilla

class nsCSSValue;
class nsIDocument;
class nsINode;
class nsPresContext;

using mozilla::dom::StyleChildrenIterator;
using mozilla::ServoElementSnapshot;

typedef nsINode RawGeckoNode;
typedef mozilla::dom::Element RawGeckoElement;
typedef nsIDocument RawGeckoDocument;
typedef nsPresContext RawGeckoPresContext;

// We have these helper types so that we can directly generate
// things like &T or Borrowed<T> on the Rust side in the function, providing
// additional safety benefits.
//
// FFI has a problem with templated types, so we just use raw pointers here.
//
// The "Borrowed" types generate &T or Borrowed<T> in the nullable case.
//
// The "Owned" types generate Owned<T> or OwnedOrNull<T>. Some of these
// are Servo-managed and can be converted to Box<ServoType> on the
// Servo side.
//
// The "Arc" types are Servo-managed Arc<ServoType>s, which are passed
// over FFI as Strong<T> (which is nullable).
// Note that T != ServoType, rather T is ArcInner<ServoType>
#define DECL_BORROWED_REF_TYPE_FOR(type_) typedef type_ const* type_##Borrowed;
#define DECL_NULLABLE_BORROWED_REF_TYPE_FOR(type_) typedef type_ const* type_##BorrowedOrNull;
#define DECL_BORROWED_MUT_REF_TYPE_FOR(type_) typedef type_* type_##BorrowedMut;
#define DECL_NULLABLE_BORROWED_MUT_REF_TYPE_FOR(type_) typedef type_* type_##BorrowedMutOrNull;

#define SERVO_ARC_TYPE(name_, type_)         \
  DECL_NULLABLE_BORROWED_REF_TYPE_FOR(type_) \
  DECL_BORROWED_REF_TYPE_FOR(type_)          \
  struct MOZ_MUST_USE_TYPE type_##Strong     \
  {                                          \
    type_* mPtr;                             \
    already_AddRefed<type_> Consume();       \
  };
#include "mozilla/ServoArcTypeList.h"
#undef SERVO_ARC_TYPE

#define DECL_OWNED_REF_TYPE_FOR(type_)    \
  typedef type_* type_##Owned;            \
  DECL_BORROWED_REF_TYPE_FOR(type_)       \
  DECL_BORROWED_MUT_REF_TYPE_FOR(type_)

#define DECL_NULLABLE_OWNED_REF_TYPE_FOR(type_)    \
  typedef type_* type_##OwnedOrNull;               \
  DECL_NULLABLE_BORROWED_REF_TYPE_FOR(type_)       \
  DECL_NULLABLE_BORROWED_MUT_REF_TYPE_FOR(type_)

// This is a reference to a reference of RawServoDeclarationBlock, which
// corresponds to Option<&Arc<RawServoDeclarationBlock>> in Servo side.
DECL_NULLABLE_BORROWED_REF_TYPE_FOR(RawServoDeclarationBlockStrong)

DECL_OWNED_REF_TYPE_FOR(RawServoStyleSet)
DECL_NULLABLE_OWNED_REF_TYPE_FOR(StyleChildrenIterator)
DECL_OWNED_REF_TYPE_FOR(StyleChildrenIterator)
DECL_OWNED_REF_TYPE_FOR(ServoElementSnapshot)

// We don't use BorrowedMut because the nodes may alias
// Servo itself doesn't directly read or mutate these;
// it only asks Gecko to do so. In case we wish to in
// the future, we should ensure that things being mutated
// are protected from noalias violations by a cell type
DECL_BORROWED_REF_TYPE_FOR(RawGeckoNode)
DECL_NULLABLE_BORROWED_REF_TYPE_FOR(RawGeckoNode)
DECL_BORROWED_REF_TYPE_FOR(RawGeckoElement)
DECL_NULLABLE_BORROWED_REF_TYPE_FOR(RawGeckoElement)
DECL_BORROWED_REF_TYPE_FOR(RawGeckoDocument)
DECL_NULLABLE_BORROWED_REF_TYPE_FOR(RawGeckoDocument)
DECL_BORROWED_MUT_REF_TYPE_FOR(StyleChildrenIterator)
DECL_BORROWED_MUT_REF_TYPE_FOR(ServoElementSnapshot)
DECL_BORROWED_REF_TYPE_FOR(nsCSSValue)
DECL_BORROWED_MUT_REF_TYPE_FOR(nsCSSValue)
DECL_BORROWED_REF_TYPE_FOR(RawGeckoPresContext)

#undef DECL_ARC_REF_TYPE_FOR
#undef DECL_OWNED_REF_TYPE_FOR
#undef DECL_NULLABLE_OWNED_REF_TYPE_FOR
#undef DECL_BORROWED_REF_TYPE_FOR
#undef DECL_NULLABLE_BORROWED_REF_TYPE_FOR
#undef DECL_BORROWED_MUT_REF_TYPE_FOR
#undef DECL_NULLABLE_BORROWED_MUT_REF_TYPE_FOR

#define SERVO_ARC_TYPE(name_, type_)                 \
  extern "C" {                                       \
  void Servo_##name_##_AddRef(type_##Borrowed ptr);  \
  void Servo_##name_##_Release(type_##Borrowed ptr); \
  }                                                  \
  namespace mozilla {                                \
  template<> struct RefPtrTraits<type_> {            \
    static void AddRef(type_* aPtr) {                \
      Servo_##name_##_AddRef(aPtr);                  \
    }                                                \
    static void Release(type_* aPtr) {               \
      Servo_##name_##_Release(aPtr);                 \
    }                                                \
  };                                                 \
  }
#include "mozilla/ServoArcTypeList.h"
#undef SERVO_ARC_TYPE

extern "C" void Servo_StyleSet_Drop(RawServoStyleSetOwned ptr);

namespace mozilla {
template<>
class DefaultDelete<RawServoStyleSet>
{
public:
  void operator()(RawServoStyleSet* aPtr) const
  {
    Servo_StyleSet_Drop(aPtr);
  }
};
}

#endif // mozilla_ServoBindingTypes_h
