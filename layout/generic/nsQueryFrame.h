/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsQueryFrame_h
#define nsQueryFrame_h

#include "nscore.h"
#include "mozilla/Assertions.h"
#include "mozilla/TypeTraits.h"

// NOTE: the long lines in this file are intentional to make compiler error
// messages more readable.

#define NS_DECL_QUERYFRAME_TARGET(classname)      \
  static const nsQueryFrame::FrameIID kFrameIID = \
      nsQueryFrame::classname##_id;               \
  typedef classname Has_NS_DECL_QUERYFRAME_TARGET;

#define NS_DECL_QUERYFRAME void* QueryFrame(FrameIID id) const override;

#define NS_QUERYFRAME_HEAD(class)               \
  void* class ::QueryFrame(FrameIID id) const { \
    switch (id) {
#define NS_QUERYFRAME_ENTRY(class)                                            \
  case class ::kFrameIID: {                                                   \
    static_assert(                                                            \
        mozilla::IsSame<class, class ::Has_NS_DECL_QUERYFRAME_TARGET>::value, \
        #class " must declare itself as a queryframe target");                \
    return const_cast<class*>(static_cast<const class*>(this));               \
  }

#define NS_QUERYFRAME_ENTRY_CONDITIONAL(class, condition)                \
  case class ::kFrameIID:                                                \
    if (condition) {                                                     \
      static_assert(                                                     \
          mozilla::IsSame<class,                                         \
                          class ::Has_NS_DECL_QUERYFRAME_TARGET>::value, \
          #class " must declare itself as a queryframe target");         \
      return const_cast<class*>(static_cast<const class*>(this));        \
    }                                                                    \
    break;

#define NS_QUERYFRAME_TAIL_INHERITING(class) \
  default:                                   \
    break;                                   \
    }                                        \
    return class ::QueryFrame(id);           \
    }

#define NS_QUERYFRAME_TAIL_INHERITANCE_ROOT                          \
  default:                                                           \
    break;                                                           \
    }                                                                \
    MOZ_ASSERT(id != GetFrameId(),                                   \
               "A frame failed to QueryFrame to its *own type*. "    \
               "It may be missing NS_DECL_QUERYFRAME, or a "         \
               "NS_QUERYFRAME_ENTRY() line with its own type name"); \
    return nullptr;                                                  \
    }

class nsQueryFrame {
 public:
  enum FrameIID {
#define FRAME_ID(classname, ...) classname##_id,
#define ABSTRACT_FRAME_ID(classname) classname##_id,
#include "nsFrameIdList.h"
#undef FRAME_ID
#undef ABSTRACT_FRAME_ID

    // This marker allows mozilla::ArenaObjectID to "extend" this enum
    // with additional sequential values for use in nsPresArena and
    // PresShell::{Allocate,Free}ByObjectId
    NON_FRAME_MARKER
  };

  // A strict subset of FrameIID above for frame classes that we instantiate.
  enum class ClassID : uint8_t {
#define FRAME_ID(classname, ...) classname##_id,
#define ABSTRACT_FRAME_ID(classname)
#include "nsFrameIdList.h"
#undef FRAME_ID
#undef ABSTRACT_FRAME_ID
  };

  virtual void* QueryFrame(FrameIID id) const = 0;
};

class nsIFrame;

template <class Source>
class do_QueryFrameHelper {
 public:
  explicit do_QueryFrameHelper(Source* s) : mRawPtr(s) {}

  // The return and argument types here are arbitrarily selected so no
  // corresponding member function exists.
  typedef void (do_QueryFrameHelper::*MatchNullptr)(double, float);
  // Implicit constructor for nullptr, trick borrowed from already_AddRefed.
  MOZ_IMPLICIT do_QueryFrameHelper(MatchNullptr aRawPtr) : mRawPtr(nullptr) {}

  template <class Dest>
  operator Dest*() {
    static_assert(
        mozilla::IsSame<typename mozilla::RemoveConst<Dest>::Type,
                        typename Dest::Has_NS_DECL_QUERYFRAME_TARGET>::value,
        "Dest must declare itself as a queryframe target");
    if (!mRawPtr) {
      return nullptr;
    }
    if (Dest* f = FastQueryFrame<Source, Dest>::QueryFrame(mRawPtr)) {
      MOZ_ASSERT(
          f == reinterpret_cast<Dest*>(mRawPtr->QueryFrame(Dest::kFrameIID)),
          "fast and slow paths should give the same result");
      return f;
    }
    return reinterpret_cast<Dest*>(mRawPtr->QueryFrame(Dest::kFrameIID));
  }

 private:
  // For non-nsIFrame types there is no fast-path.
  template <class Src, class Dst, typename = void, typename = void>
  struct FastQueryFrame {
    static Dst* QueryFrame(Src* aFrame) { return nullptr; }
  };

  // Specialization for any nsIFrame type to any nsIFrame type -- if the source
  // instance's mClass matches kFrameIID of the destination type then
  // downcasting is safe.
  template <class Src, class Dst>
  struct FastQueryFrame<
      Src, Dst,
      typename mozilla::EnableIf<mozilla::IsBaseOf<nsIFrame, Src>::value>::Type,
      typename mozilla::EnableIf<
          mozilla::IsBaseOf<nsIFrame, Dst>::value>::Type> {
    static Dst* QueryFrame(Src* aFrame) {
      return nsQueryFrame::FrameIID(aFrame->mClass) == Dst::kFrameIID
                 ? reinterpret_cast<Dst*>(aFrame)
                 : nullptr;
    }
  };

  Source* mRawPtr;
};

template <class T>
inline do_QueryFrameHelper<T> do_QueryFrame(T* s) {
  return do_QueryFrameHelper<T>(s);
}

#endif  // nsQueryFrame_h
