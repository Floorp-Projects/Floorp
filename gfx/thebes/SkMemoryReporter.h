/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_SKMEMORYREPORTER_H
#define GFX_SKMEMORYREPORTER_H

#include "nsIMemoryReporter.h"

namespace mozilla {
namespace gfx {

class SkMemoryReporter final : public nsIMemoryReporter
{
public:
  NS_DECL_ISUPPORTS

  NS_IMETHOD CollectReports(nsIHandleReportCallback* aHandleReport,
                            nsISupports* aData,
                            bool aAnonymize) override;

private:
  ~SkMemoryReporter() {}
};

} // namespace gfx
} // namespace mozilla

#endif /* GFX_SKMEMORYREPORTER_H */
