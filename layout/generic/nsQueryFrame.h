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

#define NS_DECL_QUERYFRAME_TARGET(classname)                    \
  static const nsQueryFrame::FrameIID kFrameIID = nsQueryFrame::classname##_id; \
  typedef classname Has_NS_DECL_QUERYFRAME_TARGET;

#define NS_DECL_QUERYFRAME                                      \
  void* QueryFrame(FrameIID id) override;

#define NS_QUERYFRAME_HEAD(class)                               \
  void* class::QueryFrame(FrameIID id) { switch (id) {

#define NS_QUERYFRAME_ENTRY(class)                              \
  case class::kFrameIID: {                                      \
    static_assert(mozilla::IsSame<class, class::Has_NS_DECL_QUERYFRAME_TARGET>::value, \
                  #class " must declare itself as a queryframe target"); \
    return static_cast<class*>(this);                           \
  }

#define NS_QUERYFRAME_ENTRY_CONDITIONAL(class, condition)       \
  case class::kFrameIID:                                        \
  if (condition) {                                              \
    static_assert(mozilla::IsSame<class, class::Has_NS_DECL_QUERYFRAME_TARGET>::value, \
                  #class " must declare itself as a queryframe target"); \
    return static_cast<class*>(this);                           \
  }                                                             \
  break;

#define NS_QUERYFRAME_TAIL_INHERITING(class)                    \
  default: break;                                               \
  }                                                             \
  return class::QueryFrame(id);                                 \
}

#define NS_QUERYFRAME_TAIL_INHERITANCE_ROOT                     \
  default: break;                                               \
  }                                                             \
  MOZ_ASSERT(id != GetFrameId(),                                \
    "A frame failed to QueryFrame to its *own type*. "          \
    "It may be missing NS_DECL_QUERYFRAME, or a "               \
    "NS_QUERYFRAME_ENTRY() line with its own type name");       \
  return nullptr;                                               \
}

class nsQueryFrame
{
public:
  enum FrameIID {
#define FRAME_ID(classname, ...) classname##_id,
#define ABSTRACT_FRAME_ID(classname) classname##_id,
#include "nsFrameIdList.h"
#undef FRAME_ID
#undef ABSTRACT_FRAME_ID

    // This marker allows mozilla::ArenaObjectID to "extend" this enum
    // with additional sequential values for use in nsPresArena and
    // nsIPresShell::{Allocate,Free}ByObjectId
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

  virtual void* QueryFrame(FrameIID id) = 0;
};

class do_QueryFrame
{
public:
  explicit do_QueryFrame(nsQueryFrame *s) : mRawPtr(s) { }

  // The return and argument types here are arbitrarily selected so no
  // corresponding member function exists.
  typedef void (do_QueryFrame::* MatchNullptr)(double, float);
  // Implicit constructor for nullptr, trick borrowed from already_AddRefed.
  MOZ_IMPLICIT do_QueryFrame(MatchNullptr aRawPtr) : mRawPtr(nullptr) {}

  template<class Dest>
  operator Dest*() {
    static_assert(mozilla::IsSame<Dest, typename Dest::Has_NS_DECL_QUERYFRAME_TARGET>::value,
                  "Dest must declare itself as a queryframe target");
    if (!mRawPtr)
      return nullptr;

    return reinterpret_cast<Dest*>(mRawPtr->QueryFrame(Dest::kFrameIID));
  }

private:
  nsQueryFrame *mRawPtr;
};

#endif // nsQueryFrame_h
