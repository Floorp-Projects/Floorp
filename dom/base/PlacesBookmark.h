/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PlacesBookmark_h
#define mozilla_dom_PlacesBookmark_h

#include "mozilla/dom/PlacesEvent.h"

namespace mozilla {
namespace dom {

class PlacesBookmark : public PlacesEvent {
 public:
  const PlacesBookmark* AsPlacesBookmark() const override { return this; }

  unsigned short ItemType() { return mItemType; }
  int64_t Id() { return mId; }
  int64_t ParentId() { return mParentId; }
  void GetUrl(nsString& aUrl) { aUrl = mUrl; }
  void GetGuid(nsCString& aGuid) { aGuid = mGuid; }
  void GetParentGuid(nsCString& aParentGuid) { aParentGuid = mParentGuid; }
  uint16_t Source() { return mSource; }
  bool IsTagging() { return mIsTagging; }

  unsigned short mItemType;
  int64_t mId;
  int64_t mParentId;
  nsString mUrl;
  nsCString mGuid;
  nsCString mParentGuid;
  uint16_t mSource;
  bool mIsTagging;

 protected:
  using PlacesEvent::PlacesEvent;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_PlacesBookmark_h
