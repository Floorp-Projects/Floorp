/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* representation of media lists for servo backend */

#include "mozilla/ServoMediaList.h"

#include "mozilla/ServoBindings.h"

namespace mozilla {

already_AddRefed<dom::MediaList>
ServoMediaList::Clone()
{
  // Currently MediaList::Clone() is only called from CSSStyleSheet's
  // constructor, so we don't need to support it at the moment.
  MOZ_ASSERT_UNREACHABLE("stylo: ServoMediaList doesn't support clone");
  return nullptr;
}

void
ServoMediaList::GetText(nsAString& aMediaText)
{
  Servo_MediaList_GetText(mRawList, &aMediaText);
}

void
ServoMediaList::SetText(const nsAString& aMediaText)
{
  NS_ConvertUTF16toUTF8 mediaText(aMediaText);
  Servo_MediaList_SetText(mRawList, &mediaText);
}

uint32_t
ServoMediaList::Length()
{
  return Servo_MediaList_GetLength(mRawList);
}

void
ServoMediaList::IndexedGetter(uint32_t aIndex, bool& aFound,
                              nsAString& aReturn)
{
  aFound = Servo_MediaList_GetMediumAt(mRawList, aIndex, &aReturn);
}

nsresult
ServoMediaList::Append(const nsAString& aNewMedium)
{
  if (aNewMedium.IsEmpty()) {
    return NS_ERROR_DOM_NOT_FOUND_ERR;
  }
  NS_ConvertUTF16toUTF8 newMedium(aNewMedium);
  Servo_MediaList_AppendMedium(mRawList, &newMedium);
  return NS_OK;
}

nsresult
ServoMediaList::Delete(const nsAString& aOldMedium)
{
  NS_ConvertUTF16toUTF8 oldMedium(aOldMedium);
  if (Servo_MediaList_DeleteMedium(mRawList, &oldMedium)) {
    return NS_OK;
  }
  return NS_ERROR_DOM_NOT_FOUND_ERR;
}

} // namespace mozilla
