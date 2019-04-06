/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_SCROLLABLELAYERGUID_H
#define GFX_SCROLLABLELAYERGUID_H

#include <stdint.h>                      // for uint8_t, uint32_t, uint64_t
#include "mozilla/HashFunctions.h"       // for HashGeneric
#include "mozilla/gfx/Logging.h"         // for Log
#include "mozilla/layers/LayersTypes.h"  // for LayersId
#include "nsPrintfCString.h"             // for nsPrintfCString

namespace mozilla {
namespace layers {

/**
 * This class allows us to uniquely identify a scrollable layer. The
 * mLayersId identifies the layer tree (corresponding to a child process
 * and/or tab) that the scrollable layer belongs to. The mPresShellId
 * is a temporal identifier (corresponding to the document loaded that
 * contains the scrollable layer, which may change over time). The
 * mScrollId corresponds to the actual frame that is scrollable.
 */
struct ScrollableLayerGuid {
  // We use IDs to identify frames across processes.
  typedef uint64_t ViewID;
  static const ViewID NULL_SCROLL_ID;  // This container layer does not scroll.
  static const ViewID START_SCROLL_ID = 2;  // This is the ID that scrolling
                                            // subframes will begin at.

  LayersId mLayersId;
  uint32_t mPresShellId;
  ViewID mScrollId;

  ScrollableLayerGuid() : mLayersId{0}, mPresShellId(0), mScrollId(0) {}

  ScrollableLayerGuid(LayersId aLayersId, uint32_t aPresShellId,
                      ViewID aScrollId)
      : mLayersId(aLayersId),
        mPresShellId(aPresShellId),
        mScrollId(aScrollId) {}

  ScrollableLayerGuid(const ScrollableLayerGuid& other)
      : mLayersId(other.mLayersId),
        mPresShellId(other.mPresShellId),
        mScrollId(other.mScrollId) {}

  ~ScrollableLayerGuid() {}

  bool operator==(const ScrollableLayerGuid& other) const {
    return mLayersId == other.mLayersId && mPresShellId == other.mPresShellId &&
           mScrollId == other.mScrollId;
  }

  bool operator!=(const ScrollableLayerGuid& other) const {
    return !(*this == other);
  }

  bool operator<(const ScrollableLayerGuid& other) const {
    if (mLayersId < other.mLayersId) {
      return true;
    }
    if (mLayersId == other.mLayersId) {
      if (mPresShellId < other.mPresShellId) {
        return true;
      }
      if (mPresShellId == other.mPresShellId) {
        return mScrollId < other.mScrollId;
      }
    }
    return false;
  }

  // Helper structs to use as hash/equality functions in std::unordered_map.
  // e.g. std::unordered_map<ScrollableLayerGuid,
  //                    ValueType,
  //                    ScrollableLayerGuid::HashFn> myMap;
  // std::unordered_map<ScrollableLayerGuid,
  //                    ValueType,
  //                    ScrollableLayerGuid::HashIgnoringPresShellFn,
  //                    ScrollableLayerGuid::EqualIgnoringPresShellFn> myMap;

  struct HashFn {
    std::size_t operator()(const ScrollableLayerGuid& aGuid) const {
      return HashGeneric(uint64_t(aGuid.mLayersId), aGuid.mPresShellId,
                         aGuid.mScrollId);
    }
  };

  struct HashIgnoringPresShellFn {
    std::size_t operator()(const ScrollableLayerGuid& aGuid) const {
      return HashGeneric(uint64_t(aGuid.mLayersId), aGuid.mScrollId);
    }
  };

  struct EqualIgnoringPresShellFn {
    bool operator()(const ScrollableLayerGuid& lhs,
                    const ScrollableLayerGuid& rhs) const {
      return lhs.mLayersId == rhs.mLayersId && lhs.mScrollId == rhs.mScrollId;
    }
  };
};

template <int LogLevel>
gfx::Log<LogLevel>& operator<<(gfx::Log<LogLevel>& log,
                               const ScrollableLayerGuid& aGuid) {
  return log << nsPrintfCString("(0x%" PRIx64 ", %u, %" PRIu64 ")",
                                uint64_t(aGuid.mLayersId), aGuid.mPresShellId,
                                aGuid.mScrollId)
                    .get();
}

}  // namespace layers
}  // namespace mozilla

#endif /* GFX_SCROLLABLELAYERGUID_H */
