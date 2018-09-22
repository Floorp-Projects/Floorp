/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ContentBlockingLog_h
#define mozilla_dom_ContentBlockingLog_h

#include "mozilla/Pair.h"
#include "mozilla/UniquePtr.h"
#include "nsClassHashtable.h"
#include "nsHashKeys.h"
#include "nsTArray.h"

namespace mozilla {
namespace dom {

class ContentBlockingLog final
{
  // Each element is a pair of (type, blocked). The type values come from the
  // blocking types defined in nsIWebProgressListener.
  typedef nsTArray<mozilla::Pair<uint32_t, bool>> OriginLog;

public:
  ContentBlockingLog() = default;
  ~ContentBlockingLog() = default;

  void RecordLog(const nsAString& aOrigin, uint32_t aType, bool aBlocked)
  {
    if (aOrigin.IsVoid()) {
      return;
    }
    auto entry = mLog.LookupForAdd(aOrigin);
    if (entry) {
      entry.Data()->AppendElement(mozilla::MakePair(aType, aBlocked));
    } else {
      entry.OrInsert([=] {
        auto log(MakeUnique<OriginLog>());
        log->AppendElement(mozilla::MakePair(aType, aBlocked));
        return log.release();
      });
    }
  }

private:
  nsClassHashtable<nsStringHashKey, OriginLog> mLog;
};

} // namespace dom
} // namespace mozilla

#endif
