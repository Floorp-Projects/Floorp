/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_page_load_event_utils_h__
#define mozilla_dom_page_load_event_utils_h__

#include "ipc/IPCMessageUtils.h"
#include "mozilla/glean/GleanMetrics.h"

namespace IPC {

template <>
struct ParamTraits<mozilla::glean::perf::PageLoadExtra> {
  typedef mozilla::glean::perf::PageLoadExtra paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.fcpTime);
    WriteParam(aWriter, aParam.lcpTime);
    WriteParam(aWriter, aParam.jsExecTime);
    WriteParam(aWriter, aParam.loadTime);
    WriteParam(aWriter, aParam.loadType);
    WriteParam(aWriter, aParam.responseTime);
    WriteParam(aWriter, aParam.httpVer);
    WriteParam(aWriter, aParam.redirectCount);
    WriteParam(aWriter, aParam.redirectTime);
    WriteParam(aWriter, aParam.sameOriginNav);
    WriteParam(aWriter, aParam.trrDomain);
    WriteParam(aWriter, aParam.dnsLookupTime);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ReadParam(aReader, &aResult->fcpTime) &&
           ReadParam(aReader, &aResult->lcpTime) &&
           ReadParam(aReader, &aResult->jsExecTime) &&
           ReadParam(aReader, &aResult->loadTime) &&
           ReadParam(aReader, &aResult->loadType) &&
           ReadParam(aReader, &aResult->responseTime) &&
           ReadParam(aReader, &aResult->httpVer) &&
           ReadParam(aReader, &aResult->redirectCount) &&
           ReadParam(aReader, &aResult->redirectTime) &&
           ReadParam(aReader, &aResult->sameOriginNav) &&
           ReadParam(aReader, &aResult->trrDomain) &&
           ReadParam(aReader, &aResult->dnsLookupTime);
  }
};

}  // namespace IPC

#endif  // mozilla_dom_page_load_event_utils_h__
