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
 * The Original Code is supposed to avoid exccessive QuickDraw flushes.
 *
 * The Initial Developer of the Original Code is
 * Mark Mentovai <mark@moxienet.com>.
 * Portions created by the Initial Developer are Copyright (C) 2005
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

#ifndef nsQDFlushManager_h___
#define nsQDFlushManager_h___

#include "nscore.h"

#include "nsCOMPtr.h"
#include "nsIQDFlushManager.h"
#include "nsITimer.h"

#include <Carbon/Carbon.h>

// The expected use is to replace these calls:
//   ::QDFlushPortBuffer(port, region)
// with these:
//   nsCOMPtr<nsIQDFlushManager> qdFlushManager =
//    do_GetService("@mozilla.org/gfx/qdflushmanager;1");
//   qdFlushManager->FlushPortBuffer(port, region);
// and at port destruction time:
//   qdFlushManager->RemovePort(port)

class nsQDFlushPort : public nsITimerCallback
{
  // nsQDFlushManager is responsible for maintaining the list of port objects.
  friend class nsQDFlushManager;

  NS_DECL_ISUPPORTS
  NS_DECL_NSITIMERCALLBACK

protected:
                        nsQDFlushPort(CGrafPtr aPort);
                        ~nsQDFlushPort();

  void                  Init(CGrafPtr aPort);
  void                  Destroy();
  void                  FlushPortBuffer(RgnHandle aRegion);
  PRInt64               TimeUntilFlush(AbsoluteTime aNow);

  // To use the display's refresh rate as the update Hz, see
  // http://developer.apple.com/documentation/Performance/Conceptual/Drawing/Articles/FlushingContent.html .
  // Here, use a hard-coded 30 Hz instead.
  static const PRUint32 kRefreshRateHz = 30;    // Maximum number of updates
                                                // per second

  nsQDFlushPort*        mNext;                  // Next object in list
  CGrafPtr              mPort;                  // Associated port
  AbsoluteTime          mLastFlushTime;         // Last QDFlushPortBuffer call
  nsCOMPtr<nsITimer>    mFlushTimer;            // Timer for scheduled flush
  PRPackedBool          mFlushTimerRunning;     // Is it?
};

class NS_EXPORT nsQDFlushManager : public nsIQDFlushManager
{
  NS_DECL_ISUPPORTS
  NS_DECL_NSIQDFLUSHMANAGER

public:
                              nsQDFlushManager();
                              ~nsQDFlushManager();

protected:
  nsQDFlushPort*              CreateOrGetPort(CGrafPtr aPort);

  nsQDFlushPort*              mPortList;        // Head of list
};

// 6F91262A-CF9E-4DDD-AA01-7A1FCCF14281
#define NS_QDFLUSHMANAGER_CLASSNAME "QuickDraw Buffer Flusher"
#define NS_QDFLUSHMANAGER_CID \
 {0x6F91262A, 0xCF9E, 0x4DDD, {0xAA, 0x01, 0x7A, 0x1F, 0xCC, 0xF1, 0x42, 0x81}}
#define NS_QDFLUSHMANAGER_CONTRACTID "@mozilla.org/gfx/qdflushmanager;1"

#endif /* nsQDFlushManager_h___ */
