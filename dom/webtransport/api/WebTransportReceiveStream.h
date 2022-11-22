/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_WEBTRANSPORT_API_WEBTRANSPORTSENDSTREAM__H_
#define DOM_WEBTRANSPORT_API_WEBTRANSPORTSENDSTREAM__H_

#include "mozilla/dom/ReadableStream.h"

#if WEBTRANSPORT_STREAM_IMPLEMENTED
namespace mozilla::dom {
class WebTransportReceiveStream final : public ReadableStream {
 protected:
  WebTransportReceiveStream();

 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(WebTransportReceiveStream,
                                           ReadableStream)

  already_AddRefed<Promise> GetStats();
};
#else
namespace mozilla::dom {
class WebTransportReceiveStream final : public nsISupports {
 protected:
  WebTransportReceiveStream();

 public:
  NS_DECL_ISUPPORTS

  already_AddRefed<Promise> GetStats();
};
#endif
}

#endif
