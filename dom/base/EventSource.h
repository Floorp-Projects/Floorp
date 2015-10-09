/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This implementation has support only for http requests. It is because the
 * spec has defined event streams only for http. HTTP is required because
 * this implementation uses some http headers: "Last-Event-ID", "Cache-Control"
 * and "Accept".
 */

#ifndef mozilla_dom_EventSource_h
#define mozilla_dom_EventSource_h

#include "mozilla/Attributes.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "nsIObserver.h"
#include "nsIStreamListener.h"
#include "nsIChannelEventSink.h"
#include "nsIInterfaceRequestor.h"
#include "nsITimer.h"
#include "nsIHttpChannel.h"
#include "nsWeakReference.h"
#include "nsDeque.h"
#include "nsIUnicodeDecoder.h"

class nsPIDOMWindow;

namespace mozilla {

class ErrorResult;

namespace dom {

struct EventSourceInit;

class EventSource final : public DOMEventTargetHelper
                        , public nsIObserver
                        , public nsIStreamListener
                        , public nsIChannelEventSink
                        , public nsIInterfaceRequestor
                        , public nsSupportsWeakReference
{
public:
  explicit EventSource(nsPIDOMWindow* aOwnerWindow);
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SKIPPABLE_SCRIPT_HOLDER_CLASS_INHERITED(
    EventSource, DOMEventTargetHelper)

  NS_DECL_NSIOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSICHANNELEVENTSINK
  NS_DECL_NSIINTERFACEREQUESTOR

  // nsWrapperCache
  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL
  nsPIDOMWindow*
  GetParentObject() const
  {
    return GetOwner();
  }
  static already_AddRefed<EventSource>
  Constructor(const GlobalObject& aGlobal, const nsAString& aURL,
              const EventSourceInit& aEventSourceInitDict,
              ErrorResult& aRv);

  void GetUrl(nsAString& aURL) const
  {
    aURL = mOriginalURL;
  }
  bool WithCredentials() const
  {
    return mWithCredentials;
  }

  enum {
    CONNECTING = 0U,
    OPEN = 1U,
    CLOSED = 2U
  };
  uint16_t ReadyState() const
  {
    return mReadyState;
  }

  IMPL_EVENT_HANDLER(open)
  IMPL_EVENT_HANDLER(message)
  IMPL_EVENT_HANDLER(error)
  void Close();

  // Determine if preferences allow EventSource
  static bool PrefEnabled(JSContext* aCx = nullptr, JSObject* aGlobal = nullptr);

  virtual void DisconnectFromOwner() override;

protected:
  virtual ~EventSource();

  nsresult Init(nsISupports* aOwner,
                const nsAString& aURL,
                bool aWithCredentials);

  nsresult GetBaseURI(nsIURI **aBaseURI);

  net::ReferrerPolicy GetReferrerPolicy();

  nsresult SetupHttpChannel();
  nsresult InitChannelAndRequestEventSource();
  nsresult ResetConnection();
  nsresult DispatchFailConnection();
  nsresult SetReconnectionTimeout();

  void AnnounceConnection();
  void DispatchAllMessageEvents();
  void ReestablishConnection();
  void FailConnection();

  nsresult Thaw();
  nsresult Freeze();

  static void TimerCallback(nsITimer *aTimer, void *aClosure);

  nsresult PrintErrorOnConsole(const char       *aBundleURI,
                               const char16_t  *aError,
                               const char16_t **aFormatStrings,
                               uint32_t          aFormatStringsLen);
  nsresult ConsoleError();

  static NS_METHOD StreamReaderFunc(nsIInputStream *aInputStream,
                                    void           *aClosure,
                                    const char     *aFromRawSegment,
                                    uint32_t        aToOffset,
                                    uint32_t        aCount,
                                    uint32_t       *aWriteCount);
  nsresult SetFieldAndClear();
  nsresult ClearFields();
  nsresult ResetEvent();
  nsresult DispatchCurrentMessageEvent();
  nsresult ParseCharacter(char16_t aChr);
  nsresult CheckHealthOfRequestCallback(nsIRequest *aRequestCallback);
  nsresult OnRedirectVerifyCallback(nsresult result);

  nsCOMPtr<nsIURI> mSrc;

  nsString mLastEventID;
  uint32_t mReconnectionTime;  // in ms

  struct Message {
    nsString mEventName;
    nsString mLastEventID;
    nsString mData;
  };
  nsDeque mMessagesToDispatch;
  Message mCurrentMessage;

  /**
   * A simple state machine used to manage the event-source's line buffer
   *
   * PARSE_STATE_OFF              -> PARSE_STATE_BEGIN_OF_STREAM
   *
   * PARSE_STATE_BEGIN_OF_STREAM  -> PARSE_STATE_BOM_WAS_READ |
   *                                 PARSE_STATE_CR_CHAR |
   *                                 PARSE_STATE_BEGIN_OF_LINE |
   *                                 PARSE_STATE_COMMENT |
   *                                 PARSE_STATE_FIELD_NAME
   *
   * PARSE_STATE_BOM_WAS_READ     -> PARSE_STATE_CR_CHAR |
   *                                 PARSE_STATE_BEGIN_OF_LINE |
   *                                 PARSE_STATE_COMMENT |
   *                                 PARSE_STATE_FIELD_NAME
   *
   * PARSE_STATE_CR_CHAR -> PARSE_STATE_CR_CHAR |
   *                        PARSE_STATE_COMMENT |
   *                        PARSE_STATE_FIELD_NAME |
   *                        PARSE_STATE_BEGIN_OF_LINE
   *
   * PARSE_STATE_COMMENT -> PARSE_STATE_CR_CHAR |
   *                        PARSE_STATE_BEGIN_OF_LINE
   *
   * PARSE_STATE_FIELD_NAME   -> PARSE_STATE_CR_CHAR |
   *                             PARSE_STATE_BEGIN_OF_LINE |
   *                             PARSE_STATE_FIRST_CHAR_OF_FIELD_VALUE
   *
   * PARSE_STATE_FIRST_CHAR_OF_FIELD_VALUE  -> PARSE_STATE_FIELD_VALUE |
   *                                           PARSE_STATE_CR_CHAR |
   *                                           PARSE_STATE_BEGIN_OF_LINE
   *
   * PARSE_STATE_FIELD_VALUE      -> PARSE_STATE_CR_CHAR |
   *                                 PARSE_STATE_BEGIN_OF_LINE
   *
   * PARSE_STATE_BEGIN_OF_LINE   -> PARSE_STATE_CR_CHAR |
   *                                PARSE_STATE_COMMENT |
   *                                PARSE_STATE_FIELD_NAME |
   *                                PARSE_STATE_BEGIN_OF_LINE
   *
   * Whenever the parser find an empty line or the end-of-file
   * it dispatches the stacked event.
   *
   */
  enum ParserStatus {
    PARSE_STATE_OFF,
    PARSE_STATE_BEGIN_OF_STREAM,
    PARSE_STATE_BOM_WAS_READ,
    PARSE_STATE_CR_CHAR,
    PARSE_STATE_COMMENT,
    PARSE_STATE_FIELD_NAME,
    PARSE_STATE_FIRST_CHAR_OF_FIELD_VALUE,
    PARSE_STATE_FIELD_VALUE,
    PARSE_STATE_BEGIN_OF_LINE
  };
  ParserStatus mStatus;

  bool mFrozen;
  bool mErrorLoadOnRedirect;
  bool mGoingToDispatchAllMessages;
  bool mWithCredentials;
  bool mWaitingForOnStopRequest;

  // used while reading the input streams
  nsCOMPtr<nsIUnicodeDecoder> mUnicodeDecoder;
  nsresult mLastConvertionResult;
  nsString mLastFieldName;
  nsString mLastFieldValue;

  nsCOMPtr<nsILoadGroup> mLoadGroup;

  nsCOMPtr<nsIHttpChannel> mHttpChannel;

  nsCOMPtr<nsITimer> mTimer;

  uint16_t mReadyState;
  nsString mOriginalURL;

  nsCOMPtr<nsIPrincipal> mPrincipal;
  nsString mOrigin;

  // Event Source owner information:
  // - the script file name
  // - source code line number and column number where the Event Source object
  //   was constructed.
  // - the ID of the inner window where the script lives. Note that this may not
  //   be the same as the Event Source owner window.
  // These attributes are used for error reporting.
  nsString mScriptFile;
  uint32_t mScriptLine;
  uint32_t mScriptColumn;
  uint64_t mInnerWindowID;

private:
  EventSource(const EventSource& x);   // prevent bad usage
  EventSource& operator=(const EventSource& x);
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_EventSource_h
