/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PresentationSessionTransport_h
#define mozilla_dom_PresentationSessionTransport_h

#include "mozilla/RefPtr.h"
#include "nsCOMPtr.h"
#include "nsIAsyncInputStream.h"
#include "nsIPresentationSessionTransport.h"
#include "nsIStreamListener.h"
#include "nsISupportsImpl.h"
#include "nsITransport.h"

class nsISocketTransport;
class nsIInputStreamPump;
class nsIScriptableInputStream;
class nsIMultiplexInputStream;
class nsIAsyncStreamCopier;
class nsIInputStream;

namespace mozilla {
namespace dom {

/*
 * App-to-App transport channel for the presentation session. It's usually
 * initialized with an |InitWithSocketTransport| call if at the presenting sender
 * side; whereas it's initialized with an |InitWithChannelDescription| if at the
 * presenting receiver side. The lifetime is managed in either
 * |PresentationControllingInfo| (sender side) or |PresentationPresentingInfo|
 * (receiver side) in PresentationSessionInfo.cpp.
 *
 * TODO bug 1148307 Implement PresentationSessionTransport with DataChannel.
 * The implementation over the TCP channel is primarily used for the early stage
 * of Presentation API (without SSL) and should be migrated to DataChannel with
 * full support soon.
 */
class PresentationSessionTransport final : public nsIPresentationSessionTransport
                                         , public nsITransportEventSink
                                         , public nsIInputStreamCallback
                                         , public nsIStreamListener
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(PresentationSessionTransport,
                                           nsIPresentationSessionTransport)

  NS_DECL_NSIPRESENTATIONSESSIONTRANSPORT
  NS_DECL_NSITRANSPORTEVENTSINK
  NS_DECL_NSIINPUTSTREAMCALLBACK
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER

  PresentationSessionTransport();

  void NotifyCopyComplete(nsresult aStatus);

private:
  ~PresentationSessionTransport();

  nsresult CreateStream();

  nsresult CreateInputStreamPump();

  void EnsureCopying();

  enum ReadyState {
    CONNECTING,
    OPEN,
    CLOSING,
    CLOSED
  };

  void SetReadyState(ReadyState aReadyState);

  bool IsReadyToNotifyData()
  {
    return mDataNotificationEnabled && mReadyState == OPEN;
  }

  ReadyState mReadyState;
  bool mAsyncCopierActive;
  nsresult mCloseStatus;
  bool mDataNotificationEnabled;

  // Raw socket streams
  nsCOMPtr<nsISocketTransport> mTransport;
  nsCOMPtr<nsIInputStream> mSocketInputStream;
  nsCOMPtr<nsIOutputStream> mSocketOutputStream;

  // Input stream machinery
  nsCOMPtr<nsIInputStreamPump> mInputStreamPump;
  nsCOMPtr<nsIScriptableInputStream> mInputStreamScriptable;

  // Output stream machinery
  nsCOMPtr<nsIMultiplexInputStream> mMultiplexStream;
  nsCOMPtr<nsIAsyncStreamCopier> mMultiplexStreamCopier;

  nsCOMPtr<nsIPresentationSessionTransportCallback> mCallback;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_PresentationSessionTransport_h
