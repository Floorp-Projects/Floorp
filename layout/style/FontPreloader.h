/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef FontPreloader_h_
#define FontPreloader_h_

#include "mozilla/FetchPreloader.h"

namespace mozilla {

class FontPreloader final : public FetchPreloader {
 public:
  FontPreloader();

 protected:
  nsresult CreateChannel(nsIChannel** aChannel, nsIURI* aURI,
                         const CORSMode aCORSMode,
                         const dom::ReferrerPolicy& aReferrerPolicy,
                         dom::Document* aDocument, nsILoadGroup* aLoadGroup,
                         nsIInterfaceRequestor* aCallbacks,
                         uint64_t aEarlyHintPreloaderId,
                         int32_t aSupportsPriorityValue) override;
};

}  // namespace mozilla

#endif
