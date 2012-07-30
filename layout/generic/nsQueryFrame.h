/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsQueryFrame_h
#define nsQueryFrame_h

#include "nscore.h"

#define NS_DECL_QUERYFRAME_TARGET(classname)                    \
  static const nsQueryFrame::FrameIID kFrameIID = nsQueryFrame::classname##_id;

#define NS_DECL_QUERYFRAME                                      \
  virtual void* QueryFrame(FrameIID id);

#define NS_QUERYFRAME_HEAD(class)                               \
  void* class::QueryFrame(FrameIID id) { switch (id) {

#define NS_QUERYFRAME_ENTRY(class)                              \
  case class::kFrameIID: return static_cast<class*>(this);

#define NS_QUERYFRAME_ENTRY_CONDITIONAL(class, condition)       \
  case class::kFrameIID:                                        \
  if (condition) return static_cast<class*>(this);              \
  break;

#define NS_QUERYFRAME_TAIL_INHERITING(class)                    \
  default: break;                                               \
  }                                                             \
  return class::QueryFrame(id);                                 \
}

#define NS_QUERYFRAME_TAIL_INHERITANCE_ROOT                     \
  default: break;                                               \
  }                                                             \
  return nullptr;                                                \
}

class nsQueryFrame
{
public:
  enum FrameIID {
#define FRAME_ID(classname) classname##_id,
#include "nsFrameIdList.h"
#undef FRAME_ID

    // The PresArena implementation uses this bit to distinguish objects
    // allocated by size from objects allocated by type ID (that is, frames
    // using AllocateByFrameID, and other objects using AllocateByObjectID).
    // It should not collide with any frame ID (above) or Object ID (in
    // nsPresArena.h).  It is not 0x80000000 to avoid the question of
    // whether enumeration constants are signed.
    NON_FRAME_MARKER = 0x20000000
  };

  virtual void* QueryFrame(FrameIID id) = 0;
};

class do_QueryFrame
{
public:
  do_QueryFrame(nsQueryFrame *s) : mRawPtr(s) { }

  template<class Dest>
  operator Dest*() {
    if (!mRawPtr)
      return nullptr;

    return reinterpret_cast<Dest*>(mRawPtr->QueryFrame(Dest::kFrameIID));
  }

private:
  nsQueryFrame *mRawPtr;
};

#endif // nsQueryFrame_h
