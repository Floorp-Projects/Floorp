/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 8 -*- */
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
 * The Original Code is Mozilla Plugin App.
 *
 * The Initial Developer of the Original Code is
 *   Benjamin Smedberg <benjamin@smedbergs.us>
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef mozilla_plugins_BrowserStreamChild_h
#define mozilla_plugins_BrowserStreamChild_h 1

#include "mozilla/plugins/PBrowserStreamChild.h"
#include "mozilla/plugins/AStream.h"

namespace mozilla {
namespace plugins {

class PluginInstanceChild;
class StreamNotifyChild;

class BrowserStreamChild : public PBrowserStreamChild, public AStream
{
public:
  BrowserStreamChild(PluginInstanceChild* instance,
                     const nsCString& url,
                     const uint32_t& length,
                     const uint32_t& lastmodified,
                     StreamNotifyChild* notifyData,
                     const nsCString& headers,
                     const nsCString& mimeType,
                     const bool& seekable,
                     NPError* rv,
                     uint16_t* stype);
  virtual ~BrowserStreamChild();

  NS_OVERRIDE virtual bool IsBrowserStream() { return true; }

  NPError StreamConstructed(
            const nsCString& mimeType,
            const bool& seekable,
            uint16_t* stype);

  virtual bool RecvWrite(const int32_t& offset,
                         const Buffer& data,
                         const uint32_t& newsize);
  virtual bool AnswerNPP_StreamAsFile(const nsCString& fname);
  virtual bool RecvNPP_DestroyStream(const NPReason& reason);
  virtual bool Recv__delete__();

  void EnsureCorrectInstance(PluginInstanceChild* i)
  {
    if (i != mInstance)
      NS_RUNTIMEABORT("Incorrect stream instance");
  }
  void EnsureCorrectStream(NPStream* s)
  {
    if (s != &mStream)
      NS_RUNTIMEABORT("Incorrect stream data");
  }

  NPError NPN_RequestRead(NPByteRange* aRangeList);
  void NPN_DestroyStream(NPReason reason);

  void NotifyPending() {
    NS_ASSERTION(!mNotifyPending, "Pending twice?");
    mNotifyPending = true;
    EnsureDeliveryPending();
  }

  /**
   * During instance destruction, artificially cancel all outstanding streams.
   *
   * @return false if we are already in the DELETING state.
   */
  bool InstanceDying() {
    if (DELETING == mState)
      return false;

    mInstanceDying = true;
    return true;
  }

  void FinishDelivery() {
    NS_ASSERTION(mInstanceDying, "Should only be called after InstanceDying");
    NS_ASSERTION(DELETING != mState, "InstanceDying didn't work?");
    mStreamStatus = NPRES_USER_BREAK;
    Deliver();
    NS_ASSERTION(!mStreamNotify, "Didn't deliver NPN_URLNotify?");
  }

private:
  friend class StreamNotifyChild;
  using PBrowserStreamChild::SendNPN_DestroyStream;

  /**
   * Post an event to ensure delivery of pending data/destroy/urlnotify events
   * outside of the current RPC stack.
   */
  void EnsureDeliveryPending();

  /**
   * Deliver data, destruction, notify scheduling
   * or cancelling the suspended timer as needed.
   */
  void Deliver();

  /**
   * Deliver one chunk of pending data.
   * @return true if the plugin indicated a pause was necessary
   */
  bool DeliverPendingData();

  void SetSuspendedTimer();
  void ClearSuspendedTimer();

  PluginInstanceChild* mInstance;
  NPStream mStream;

  static const NPReason kStreamOpen = -1;

  /**
   * The plugin's notion of whether a stream has been "closed" (no more
   * data delivery) differs from the plugin host due to asynchronous delivery
   * of data and NPN_DestroyStream. While the plugin-visible stream is open,
   * mStreamStatus should be kStreamOpen (-1). mStreamStatus will be a
   * failure code if either the parent or child indicates stream failure.
   */
  NPReason mStreamStatus;

  /**
   * Delivery of NPP_DestroyStream and NPP_URLNotify must be postponed until
   * all data has been delivered.
   */
  enum {
    NOT_DESTROYED, // NPP_DestroyStream not yet received
    DESTROY_PENDING, // NPP_DestroyStream received, not yet delivered
    DESTROYED // NPP_DestroyStream delivered, NPP_URLNotify may still be pending
  } mDestroyPending;
  bool mNotifyPending;

  // When NPP_Destroy is called for our instance (manager), this flag is set
  // cancels the stream and avoids sending StreamDestroyed.
  bool mInstanceDying;

  enum {
    CONSTRUCTING,
    ALIVE,
    DYING,
    DELETING
  } mState;
  nsCString mURL;
  nsCString mHeaders;
  StreamNotifyChild* mStreamNotify;

  struct PendingData
  {
    int32_t offset;
    Buffer data;
    int32_t curpos;
  };
  nsTArray<PendingData> mPendingData;

  /**
   * Asynchronous RecvWrite messages are never delivered to the plugin
   * immediately, because that may be in the midst of an unexpected RPC
   * stack frame. It instead posts a runnable using this tracker to cancel
   * in case we are destroyed.
   */
  ScopedRunnableMethodFactory<BrowserStreamChild> mDeliveryTracker;
  base::RepeatingTimer<BrowserStreamChild> mSuspendedTimer;
};

} // namespace plugins
} // namespace mozilla

#endif /* mozilla_plugins_BrowserStreamChild_h */
