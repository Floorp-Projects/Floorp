/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DecoderDoctorDiagnostics.h"

#include "mozilla/Logging.h"

static mozilla::LazyLogModule sDecoderDoctorLog("DecoderDoctor");
#define DD_LOG(level, arg, ...) MOZ_LOG(sDecoderDoctorLog, level, (arg, ##__VA_ARGS__))
#define DD_DEBUG(arg, ...) DD_LOG(mozilla::LogLevel::Debug, arg, ##__VA_ARGS__)
#define DD_WARN(arg, ...) DD_LOG(mozilla::LogLevel::Warning, arg, ##__VA_ARGS__)

namespace mozilla
{

void
DecoderDoctorDiagnostics::StoreDiagnostics(nsIDocument* aDocument,
                                           const nsAString& aFormat,
                                           const char* aCallSite)
{
  if (NS_WARN_IF(!aDocument)) {
    DD_WARN("DecoderDoctorDiagnostics[%p]::StoreDiagnostics(nsIDocument* aDocument=nullptr, format='%s', call site '%s')",
            this, NS_ConvertUTF16toUTF8(aFormat).get(), aCallSite);
    return;
  }

  // TODO: Actually analyze data.
  DD_DEBUG("DecoderDoctorDiagnostics[%p]::StoreDiagnostics(nsIDocument* aDocument=%p, format='%s', call site '%s')",
           this, aDocument, NS_ConvertUTF16toUTF8(aFormat).get(), aCallSite);
}

} // namespace mozilla
