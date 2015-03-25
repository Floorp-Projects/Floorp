/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TCPSocket_h
#define mozilla_dom_TCPSocket_h

#include "mozilla/dom/TCPSocketBinding.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "nsITransport.h"
#include "nsIStreamListener.h"
#include "nsIAsyncInputStream.h"
#include "nsISupportsImpl.h"
#include "nsIObserver.h"
#include "nsWeakReference.h"
#include "js/RootingAPI.h"

class nsISocketTransport;
class nsIInputStreamPump;
class nsIScriptableInputStream;
class nsIBinaryInputStream;
class nsIMultiplexInputStream;
class nsIAsyncStreamCopier;
class nsIInputStream;
class nsINetworkInfo;

namespace mozilla {
class ErrorResult;
namespace dom {

class DOMError;
class USVStringOrArrayBuffer;

// This interface is only used for legacy navigator.mozTCPSocket API compatibility.
class LegacyMozTCPSocket : public nsISupports
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(LegacyMozTCPSocket)

  explicit LegacyMozTCPSocket(nsPIDOMWindow* aWindow);

  already_AddRefed<TCPSocket>
  Open(const nsAString& aHost,
       uint16_t aPort,
       const SocketOptions& aOptions,
       ErrorResult& aRv);

  bool WrapObject(JSContext* aCx,
                  JS::Handle<JSObject*> aGivenProto,
                  JS::MutableHandle<JSObject*> aReflector);

private:
  virtual ~LegacyMozTCPSocket();

  nsCOMPtr<nsIGlobalObject> mGlobal;
};

class TCPSocket final : public DOMEventTargetHelper
                      , public nsIStreamListener
                      , public nsITransportEventSink
                      , public nsIInputStreamCallback
                      , public nsIObserver
                      , public nsSupportsWeakReference
{
public:
  TCPSocket(nsIGlobalObject* aGlobal, const nsAString& aHost, uint16_t aPort,
            bool aSsl, bool aUseArrayBuffers);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(TCPSocket, DOMEventTargetHelper)
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSITRANSPORTEVENTSINK
  NS_DECL_NSIINPUTSTREAMCALLBACK
  NS_DECL_NSIOBSERVER

  nsPIDOMWindow* GetParentObject() const
  {
    return GetOwner();
  }

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  void GetHost(nsAString& aHost);
  uint32_t Port();
  bool Ssl();
  uint64_t BufferedAmount();
  void Suspend();
  void Resume(ErrorResult& aRv);
  void Close();
  bool Send(const nsCString& aData, ErrorResult& aRv);
  bool Send(const ArrayBuffer& aData,
            uint32_t aByteOffset,
            const Optional<uint32_t>& aByteLength,
            ErrorResult& aRv);
  TCPReadyState ReadyState();
  TCPSocketBinaryType BinaryType();
  void UpgradeToSecure(ErrorResult& aRv);

  static already_AddRefed<TCPSocket>
  Constructor(const GlobalObject& aGlobal,
              const nsAString& aHost,
              uint16_t aPort,
              const SocketOptions& aOptions,
              ErrorResult& aRv);

  static bool SocketEnabled();

  IMPL_EVENT_HANDLER(open);
  IMPL_EVENT_HANDLER(drain);
  IMPL_EVENT_HANDLER(data);
  IMPL_EVENT_HANDLER(error);
  IMPL_EVENT_HANDLER(close);

  nsresult Init();

  // Inform this socket that a buffered send() has completed sending.
  void NotifyCopyComplete(nsresult aStatus);

private:
  ~TCPSocket();

  // Send the contents of the provided input stream, which is assumed to be the given length
  // for reporting and buffering purposes.
  bool Send(nsIInputStream* aStream, uint32_t aByteLength);
  // Begin an asynchronous copy operation if one is not already in progress.
  nsresult EnsureCopying();
  // Enable TLS on this socket.
  void ActivateTLS();
  // Dispatch an "error" event at this object.
  void FireErrorEvent(const nsAString& aName, const nsAString& aMessage);
  // Dispatch an event of the given type at this object.
  void FireEvent(const nsAString& aType);
  // Dispatch a "data" event at this object.
  void FireDataEvent(JSContext* aCx, const nsAString& aType, JS::Handle<JS::Value> aData);
  // Dispatch an error event if necessary, then dispatch a "close" event.
  nsresult MaybeReportErrorAndCloseIfOpen(nsresult status);
#ifdef MOZ_WIDGET_GONK
  // Store and reset any saved network stats for this socket.
  void SaveNetworkStats(bool aEnforce);
#endif

  TCPReadyState mReadyState;
  // Whether to use strings or array buffers for the "data" event.
  bool mUseArrayBuffers;
  nsString mHost;
  uint16_t mPort;
  // Whether this socket is using a secure transport.
  bool mSsl;

  // Raw socket streams
  nsCOMPtr<nsISocketTransport> mTransport;
  nsCOMPtr<nsIInputStream> mSocketInputStream;
  nsCOMPtr<nsIOutputStream> mSocketOutputStream;

  // Input stream machinery
  nsCOMPtr<nsIInputStreamPump> mInputStreamPump;
  nsCOMPtr<nsIScriptableInputStream> mInputStreamScriptable;
  nsCOMPtr<nsIBinaryInputStream> mInputStreamBinary;

  // Output stream machinery
  nsCOMPtr<nsIMultiplexInputStream> mMultiplexStream;
  nsCOMPtr<nsIAsyncStreamCopier> mMultiplexStreamCopier;

  // Is there an async copy operation in progress?
  bool mAsyncCopierActive;
  // True if the buffer is full and a "drain" event is expected by the client.
  bool mWaitingForDrain;

  // The id of the window that created this socket.
  uint64_t mInnerWindowID;

  // The number of times this socket has had `Suspend` called without a corresponding `Resume`.
  uint32_t mSuspendCount;

  // True if this socket has been upgraded to secure after the initial connection,
  // but the actual upgrade is waiting for an in-progress copy operation to complete.
  bool mWaitingForStartTLS;
  // The buffered data awaiting the TLS upgrade to finish.
  nsTArray<nsCOMPtr<nsIInputStream>> mPendingDataAfterStartTLS;

#ifdef MOZ_WIDGET_GONK
  // Number of bytes sent.
  uint32_t mTxBytes;
  // Number of bytes received.
  uint32_t mRxBytes;
  // The app that owns this socket.
  uint32_t mAppId;
  // Was this socket created inside of a mozbrowser frame?
  bool mInBrowser;
  // The name of the active network used by this socket.
  nsCOMPtr<nsINetworkInfo> mActiveNetworkInfo;
#endif
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_TCPSocket_h
