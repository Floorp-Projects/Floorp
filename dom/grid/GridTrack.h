/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_GridTrack_h
#define mozilla_dom_GridTrack_h

#include "mozilla/dom/GridBinding.h"
#include "nsWrapperCache.h"

namespace mozilla {
namespace dom {

class GridTracks;

class GridTrack : public nsISupports
                , public nsWrapperCache
{
public:
  GridTrack(GridTracks *parent);

protected:
  virtual ~GridTrack();

public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(GridTrack)

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;
  GridTracks* GetParentObject()
  {
    return mParent;
  }

  double Start() const;
  double Breadth() const;
  GridTrackType Type() const;
  GridTrackState State() const;

  void SetTrackValues(double aStart, double aBreadth, GridTrackType aType, GridTrackState aState);

protected:
  RefPtr<GridTracks> mParent;
  double mStart;
  double mBreadth;
  GridTrackType mType;
  GridTrackState mState;
};

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_GridTrack_h */
