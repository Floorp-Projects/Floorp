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
  ServoMediaList(const nsAString& aMedia, dom::CallerType);
  ServoMediaList();

  already_AddRefed<dom::MediaList> Clone() final override;

  void GetText(nsAString& aMediaText) final override;
  void SetText(const nsAString& aMediaText) final override;

  uint32_t Length() final override;
  void IndexedGetter(uint32_t aIndex, bool& aFound,
                     nsAString& aReturn) final override;

  bool Matches(nsPresContext*) const final override;

#ifdef DEBUG
  bool IsServo() const final override { return true; }
#endif

  RawServoMediaList& RawList() { return *mRawList; }

protected:
  nsresult Delete(const nsAString& aOldMedium) final override;
  nsresult Append(const nsAString& aNewMedium) final override;
  void SetTextInternal(const nsAString& aMediaText, dom::CallerType);

  ~ServoMediaList() {}

private:
  RefPtr<RawServoMediaList> mRawList;
};

} // namespace mozilla

#endif // mozilla_ServoMediaList_h
