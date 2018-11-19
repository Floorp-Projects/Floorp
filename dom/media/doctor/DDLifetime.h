/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DDLifetime_h_
#define DDLifetime_h_

#include "DDLogObject.h"
#include "DDMessageIndex.h"
#include "DDTimeStamp.h"

namespace mozilla {

namespace dom {
class HTMLMediaElement;
}  // namespace dom

// This struct records the lifetime of one C++ object.
// Note that multiple objects may have the same address and type (at different
// times), so the recorded construction/destruction times should be used to
// distinguish them.
struct DDLifetime {
  const DDLogObject mObject;
  const DDMessageIndex mConstructionIndex;
  const DDTimeStamp mConstructionTimeStamp;
  // Only valid when mDestructionTimeStamp is not null.
  DDMessageIndex mDestructionIndex;
  DDTimeStamp mDestructionTimeStamp;
  // Associated HTMLMediaElement, initially nullptr until this object can be
  // linked to its HTMLMediaElement.
  const dom::HTMLMediaElement* mMediaElement;
  // If not null, derived object for which this DDLifetime is a base class.
  // This is used to link messages from the same object, even when they
  // originate from a method on a base class.
  // Note: We assume a single-inheritance hierarchy.
  DDLogObject mDerivedObject;
  DDMessageIndex mDerivedObjectLinkingIndex;
  // Unique tag used to identify objects in a log, easier to read than object
  // pointers.
  // Negative and unique for unassociated objects.
  // Positive for associated objects, and unique for that HTMLMediaElement
  // group.
  int32_t mTag;

  DDLifetime(DDLogObject aObject, DDMessageIndex aConstructionIndex,
             DDTimeStamp aConstructionTimeStamp, int32_t aTag)
      : mObject(aObject),
        mConstructionIndex(aConstructionIndex),
        mConstructionTimeStamp(aConstructionTimeStamp),
        mDestructionIndex(0),
        mMediaElement(nullptr),
        mDerivedObjectLinkingIndex(0),
        mTag(aTag) {}

  // Is this lifetime alive at the given index?
  // I.e.: Constructed before, and destroyed later or not yet.
  bool IsAliveAt(DDMessageIndex aIndex) const {
    return aIndex >= mConstructionIndex &&
           (!mDestructionTimeStamp || aIndex <= mDestructionIndex);
  }

  // Print the object's pointer, tag and class name (and derived class). E.g.:
  // "dom::HTMLVideoElement[134073800]#1 (as dom::HTMLMediaElement)"
  void AppendPrintf(nsCString& aString) const;
  nsCString Printf() const;
};

}  // namespace mozilla

#endif  // DDLifetime_h_
