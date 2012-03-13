/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Wellington Fernando de Macedo.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *    Wellington Fernando de Macedo <wfernandom2004@gmail.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * This implementation has support only for http requests. It is because the
 * spec has defined event streams only for http. HTTP is required because
 * this implementation uses some http headers: "Last-Event-ID", "Cache-Control"
 * and "Accept".
 */

#ifndef nsEventSource_h__
#define nsEventSource_h__

#include "nsIEventSource.h"
#include "nsIJSNativeInitializer.h"
#include "nsDOMEventTargetHelper.h"
#include "nsIObserver.h"
#include "nsIStreamListener.h"
#include "nsIChannelEventSink.h"
#include "nsIInterfaceRequestor.h"
#include "nsITimer.h"
#include "nsIHttpChannel.h"
#include "nsWeakReference.h"
#include "nsDeque.h"
#include "nsIUnicodeDecoder.h"

#define NS_EVENTSOURCE_CID                          \
 { /* 755e2d2d-a836-4539-83f4-16b51156341f */       \
  0x755e2d2d, 0xa836, 0x4539,                       \
 {0x83, 0xf4, 0x16, 0xb5, 0x11, 0x56, 0x34, 0x1f} }

#define NS_EVENTSOURCE_CONTRACTID "@mozilla.org/eventsource;1"

class AsyncVerifyRedirectCallbackFwr;
class nsAutoClearFields;

class nsEventSource: public nsDOMEventTargetHelper,
                     public nsIEventSource,
                     public nsIJSNativeInitializer,
                     public nsIObserver,
                     public nsIStreamListener,
                     public nsIChannelEventSink,
                     public nsIInterfaceRequestor,
                     public nsSupportsWeakReference
{
friend class AsyncVerifyRedirectCallbackFwr;

public:
  nsEventSource();
  virtual ~nsEventSource();
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SKIPPABLE_SCRIPT_HOLDER_CLASS_INHERITED(nsEventSource,
                                                                   nsDOMEventTargetHelper)

  NS_DECL_NSIEVENTSOURCE

  // nsIJSNativeInitializer
  NS_IMETHOD Initialize(nsISupports* aOwner, JSContext* cx, JSObject* obj,
                        PRUint32 argc, jsval* argv);

  NS_DECL_NSIOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSICHANNELEVENTSINK
  NS_DECL_NSIINTERFACEREQUESTOR

  // Determine if preferences allow EventSource
  static bool PrefEnabled();

  virtual void DisconnectFromOwner();
protected:
  nsresult GetBaseURI(nsIURI **aBaseURI);

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
                               const PRUnichar  *aError,
                               const PRUnichar **aFormatStrings,
                               PRUint32          aFormatStringsLen);
  nsresult ConsoleError();

  static NS_METHOD StreamReaderFunc(nsIInputStream *aInputStream,
                                    void           *aClosure,
                                    const char     *aFromRawSegment,
                                    PRUint32        aToOffset,
                                    PRUint32        aCount,
                                    PRUint32       *aWriteCount);
  nsresult SetFieldAndClear();
  nsresult ClearFields();
  nsresult ResetEvent();
  nsresult DispatchCurrentMessageEvent();
  nsresult ParseCharacter(PRUnichar aChr);
  bool CheckCanRequestSrc(nsIURI* aSrc = nsnull);  // if null, it tests mSrc
  nsresult CheckHealthOfRequestCallback(nsIRequest *aRequestCallback);
  nsresult OnRedirectVerifyCallback(nsresult result);

  nsCOMPtr<nsIURI> mSrc;

  nsString mLastEventID;
  PRUint32 mReconnectionTime;  // in ms

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

  nsRefPtr<nsDOMEventListenerWrapper> mOnOpenListener;
  nsRefPtr<nsDOMEventListenerWrapper> mOnErrorListener;
  nsRefPtr<nsDOMEventListenerWrapper> mOnMessageListener;

  nsCOMPtr<nsILoadGroup> mLoadGroup;

  /**
   * The notification callbacks the channel had initially.
   * We want to forward things here as needed.
   */
  nsCOMPtr<nsIInterfaceRequestor> mNotificationCallbacks;
  nsCOMPtr<nsIChannelEventSink>   mChannelEventSink;

  nsCOMPtr<nsIHttpChannel> mHttpChannel;

  nsCOMPtr<nsITimer> mTimer;

  PRInt32 mReadyState;
  nsString mOriginalURL;

  nsCOMPtr<nsIPrincipal> mPrincipal;
  nsString mOrigin;

  PRUint32 mRedirectFlags;
  nsCOMPtr<nsIAsyncVerifyRedirectCallback> mRedirectCallback;
  nsCOMPtr<nsIChannel> mNewRedirectChannel;

  // Event Source owner information:
  // - the script file name
  // - source code line number where the Event Source object was constructed.
  // - the ID of the inner window where the script lives. Note that this may not
  //   be the same as the Event Source owner window.
  // These attributes are used for error reporting.
  nsString mScriptFile;
  PRUint32 mScriptLine;
  PRUint64 mInnerWindowID;

private:
  nsEventSource(const nsEventSource& x);   // prevent bad usage
  nsEventSource& operator=(const nsEventSource& x);
};

#endif // nsEventSource_h__
