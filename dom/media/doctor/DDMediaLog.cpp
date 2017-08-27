/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DDMediaLog.h"

namespace mozilla {

size_t
DDMediaLog::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
{
  size_t size = mMessages.ShallowSizeOfExcludingThis(aMallocSizeOf);
  for (const DDLogMessage& message : mMessages) {
    if (message.mValue.is<const nsCString>()) {
      size +=
        message.mValue.as<const nsCString>().SizeOfExcludingThisIfUnshared(
          aMallocSizeOf);
    } else if (message.mValue.is<MediaResult>()) {
      size += message.mValue.as<MediaResult>()
                .Message()
                .SizeOfExcludingThisIfUnshared(aMallocSizeOf);
    }
  }
  return size;
}

} // namespace mozilla
