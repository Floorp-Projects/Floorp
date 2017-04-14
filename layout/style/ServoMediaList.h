/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* representation of media lists for servo backend */

#ifndef mozilla_ServoMediaList_h
#define mozilla_ServoMediaList_h

#include "mozilla/dom/MediaList.h"
#include "mozilla/ServoBindingTypes.h"

namespace mozilla {

class ServoMediaList final : public dom::MediaList
{
public:
  explicit ServoMediaList(already_AddRefed<RawServoMediaList> aRawList)
    : mRawList(aRawList) {}
  explicit ServoMediaList(const nsAString& aMedia);
  ServoMediaList();

  already_AddRefed<dom::MediaList> Clone() final;

  void GetText(nsAString& aMediaText) final;
  void SetText(const nsAString& aMediaText) final;

  uint32_t Length() final;
  void IndexedGetter(uint32_t aIndex, bool& aFound,
                     nsAString& aReturn) final;

  bool Matches(nsPresContext*) const final;

#ifdef DEBUG
  bool IsServo() const final { return true; }
#endif

  RawServoMediaList& RawList() { return *mRawList; }

protected:
  nsresult Delete(const nsAString& aOldMedium) final;
  nsresult Append(const nsAString& aNewMedium) final;

  ~ServoMediaList() {}

private:
  RefPtr<RawServoMediaList> mRawList;
};

} // namespace mozilla

#endif // mozilla_ServoMediaList_h
