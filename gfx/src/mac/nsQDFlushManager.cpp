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
 * The Original Code is supposed to avoid excessive QuickDraw flushes.
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

#include "nsQDFlushManager.h"

#include "nsComponentManagerUtils.h"
#include "nsIObserverService.h"
#include "nsIServiceManager.h"
#include "nsCRT.h"

class nsQDFlushPort : public nsITimerCallback
{
  // nsQDFlushManager is responsible for maintaining the list of port objects.
  friend class nsQDFlushManager;

protected:
                        nsQDFlushPort(CGrafPtr aPort);
                        ~nsQDFlushPort();

  void                  Init(CGrafPtr aPort);
  void                  Destroy();
  void                  FlushPortBuffer(RgnHandle aRegion);
  PRInt64               TimeUntilFlush(AbsoluteTime aNow);
  NS_DECL_ISUPPORTS
  NS_DECL_NSITIMERCALLBACK

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

class nsQDFlushObserver : public nsIObserver
{
  // nsQDFlushManager is responsible for creating the observer.
  friend class nsQDFlushManager;

protected:
                        nsQDFlushObserver(nsQDFlushManager* aParent);
                        ~nsQDFlushObserver();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  nsQDFlushManager*     mParent;                // Parent object
};

// nsQDFlushManager

// protected static
nsQDFlushManager* nsQDFlushManager::sQDFlushManager = nsnull;
nsQDFlushObserver* nsQDFlushManager::sQDFlushObserver = nsnull;

// nsQDFlushManager() constructor
//
// If you create your own object, you are responsible for freeing it.  Once
// freed, it will free the attached port objects.
//
// If you work through the static object by calling through the static
// sQDFlushPortBuffer method, the static object will be created dynamically
// at first call and will be freed automatically at shutdown.
nsQDFlushManager::nsQDFlushManager()
: mPortList(nsnull)
{
}

// ~nsQDFlushManager() destructor
//
// See the note about object creation and destruction in the comment
// preceding the constructor.
nsQDFlushManager::~nsQDFlushManager()
{
  nsQDFlushPort* port = mPortList;
  while (port)
  {
    nsQDFlushPort* next = port->mNext;
    port->Destroy();
    NS_RELEASE(port);
    port = next;
  }
}

// FlushPortBuffer(aPort, aRegion)
//
// The public entry point for object-based calls.  Calls
// QDFlushPortBuffer(aPort, aRegion) if aPort hasn't been flushed too
// recently.  If it has been, calls QDAddRegionToDirtyRegion(aPort, aRegion)
// and if no flush has been scheduled, schedules a flush for the appropriate
// time.
//
// public
void
nsQDFlushManager::FlushPortBuffer(CGrafPtr aPort, RgnHandle aRegion)
{
  CreateOrGetPort(aPort)->FlushPortBuffer(aRegion);
}

// sFlushPortBuffer(aPort, aRegion)
//
// The public entry point for calls into the static flush manager.  Gets
// the static object, creating it if necessary, and then calls into
// FlushPortBuffer above.
//
// public static
void
nsQDFlushManager::sFlushPortBuffer(CGrafPtr aPort, RgnHandle aRegion)
{
  GetStaticQDFlushManager()->FlushPortBuffer(aPort, aRegion);
}

// CreateOrGetPort(aPort)
//
// Walks through the list of port objects and returns the one corresponding to
// aPort if it exists.  If it doesn't exist, but an unused existing port
// object can be adapted to aPort, it will be adapted and returned.  If no
// suitable port object exists, a new port object is created, added to the
// list, and returned.  Any port objects created here will be destroyed when
// the ~nsQDFlushManager destructor runs or when RemovePort is called.
//
// protected
nsQDFlushPort*
nsQDFlushManager::CreateOrGetPort(CGrafPtr aPort)
{
  AbsoluteTime now = ::UpTime();

  // First, go through the list and see if an object is already associated
  // with this port.
  nsQDFlushPort* port = mPortList;
  while (port)
  {
    if (aPort == port->mPort)
    {
      return port;
    }
    port = port->mNext;
  }

  // If no port object exists yet, try to find an object that's not in use.
  // If there is one, repurpose it.
  // Don't be frightened.  The pointer-pointer business isn't so confusing,
  // and it eases maintenance of the linked list.
  nsQDFlushPort** portPtr = &mPortList;
  while ((port = *portPtr))
  {
    if (!port->mFlushTimerRunning && port->TimeUntilFlush(now) < 0)
    {
      // If the flush timer is not running and it's past the time during which
      // a flush would be postponed, the object is no longer needed.  Future
      // flushes for port->mPort would occur immediately.  Since there's no
      // longer any state to track, the object can be reused for another port
      // This keeps the size of the list manageable.
      port->Init(aPort);
      return port;
    }
    portPtr = &port->mNext;
  }

  // portPtr points to mNext of the last port object in the list, or if none,
  // to mPortList.  That makes it easy to hook the new object up.
  *portPtr = port = new nsQDFlushPort(aPort);
  NS_IF_ADDREF(port);
  return port;
}

// RemovePort(aPort)
//
// Walks through the list of port objects and removes the one corresponding to
// aPort, if it exists.
//
// Presently unused.
//
// protected
void
nsQDFlushManager::RemovePort(CGrafPtr aPort)
{
  // Traversal is as in CreateOrGetPort.
  nsQDFlushPort** portPtr = &mPortList;
  while (nsQDFlushPort* port = *portPtr)
  {
    if (aPort == port->mPort)
    {
      nsQDFlushPort* next = port->mNext;
      port->Destroy();
      NS_RELEASE(port);

      // portPtr points to mNext of the previous object, or if none,
      // mPortList.  That makes it easy to snip the old object out by
      // setting it to the follower.
      *portPtr = next;
      return;
    }
    portPtr = &port->mNext;
  }
}

// GetStaticQDFlushManager()
//
// Most calls through nsQDFlushManager are expected to be made through
// a global object, through the static sQDFlushPort method.  Because
// constructors and destructors on static objects are distrusted, each
// call into sQDFlushPort obtains the static object by calling here.
// If the object does not yet exist, it is created.  To handle destruction,
// a shutdown observer is added.  The observer handles teardown of the
// static manager object and all attached port objects.
//
// protected static
nsQDFlushManager*
nsQDFlushManager::GetStaticQDFlushManager()
{
  if (!sQDFlushManager)
  {
    // The static flush manager has not yet been initialized.  Create it.
    sQDFlushManager = new nsQDFlushManager();
    NS_ASSERTION(sQDFlushManager, "failed to create flush manager");
    if (!sQDFlushManager)
    {
      // Early return, avoid creating a useless observer.
      return nsnull;
    }

    // Create a shutdown observer to handle destruction.
    sQDFlushObserver = new nsQDFlushObserver(sQDFlushManager);
    NS_ASSERTION(sQDFlushObserver, "failed to create observer");
    if (sQDFlushObserver)
    {
      // register for shutdown
      nsresult rv;
      nsCOMPtr<nsIObserverService> observerService(
        do_GetService("@mozilla.org/observer-service;1", &rv));
      if (NS_SUCCEEDED(rv))
      {
        // There's not much we can or should do if this fails.
        observerService->AddObserver(sQDFlushObserver,
          NS_XPCOM_SHUTDOWN_OBSERVER_ID, PR_FALSE);
      }
    }
  }

  return sQDFlushManager;
}

// nsQDFlushPort

nsQDFlushPort::nsQDFlushPort(CGrafPtr aPort)
: mNext(nsnull)
, mPort(aPort)
, mLastFlushTime((AbsoluteTime){0, 0})
, mFlushTimer(nsnull)
, mFlushTimerRunning(PR_FALSE)
{
}

nsQDFlushPort::~nsQDFlushPort()
{
  // Everything should have been taken care of by Destroy().
}

// Init(aPort)
//
// (Re)initialize object.
//
// protected
void
nsQDFlushPort::Init(CGrafPtr aPort)
{
  mPort = aPort;
}

// Destroy()
//
// Prepare object for destruction.
//
// protected
void
nsQDFlushPort::Destroy()
{
  if (mFlushTimer)
  {
    mFlushTimer->Cancel();
  }
  mFlushTimer = nsnull;
  mFlushTimerRunning = PR_FALSE;
  mNext = nsnull;
}

// FlushPortBuffer(aRegion)
//
// Flushes, dirties, and schedules, as appropriate.  Public access is from
// nsQDFlushManager::FlushPortBuffer(CGrafPtr aPort, RgnHandle aRegion).
//
// protected
void
nsQDFlushPort::FlushPortBuffer(RgnHandle aRegion)
{
  AbsoluteTime now = ::UpTime();
  PRInt64 timeUntilFlush = TimeUntilFlush(now);

  if (!mFlushTimerRunning && timeUntilFlush < 0)
  {
    // If past the time for the next acceptable flush, flush now.
    ::QDFlushPortBuffer(mPort, aRegion);
    mLastFlushTime = now;
  }
  else
  {
    // If it's not time for the next flush yet, or if the timer is running
    // indicating that an update is pending, just mark the dirty region.
    ::QDAddRegionToDirtyRegion(mPort, aRegion);

    if (!mFlushTimerRunning)
    {
      // No flush scheduled?  No problem.
      if (!mFlushTimer)
      {
        // No timer object?  No problem.
        nsresult err;
        mFlushTimer = do_CreateInstance("@mozilla.org/timer;1", &err);
        NS_ASSERTION(NS_SUCCEEDED(err), "Could not instantiate flush timer.");
      }
      if (mFlushTimer)
      {
        // Start the clock, with the timer firing at the already-calculated
        // time until the next flush.  Nanoseconds (1E-9) were used above,
        // but nsITimer is big on milliseconds (1E-3), so divide by 1E6.
        // Any time that was consumed between the ::UpTime call and now
        // will be lost.  That's not so bad in the usual case, it's a tiny
        // bit less not so bad if a timer object didn't exist yet and was
        // created.  It's better to update slightly less frequently than
        // the target than slightly more frequently.
        mFlushTimer->InitWithCallback(this, (PRUint32)(timeUntilFlush/1E6),
                                      nsITimer::TYPE_ONE_SHOT);
        mFlushTimerRunning = PR_TRUE;
      }
    }
  }
}

// protected
PRInt64
nsQDFlushPort::TimeUntilFlush(AbsoluteTime aNow)
{
  Nanoseconds elapsed = ::AbsoluteDeltaToNanoseconds(aNow, mLastFlushTime);

  // nano = 1E-9 and the desired refresh rate is in Hz, so 1E9/kRefreshRateHz
  // gives the interval between updates in nanoseconds.
  return S64Subtract(U64SetU(1E9/kRefreshRateHz),
                     UnsignedWideToUInt64(elapsed));
}

// nsITimer implementation

NS_IMPL_ISUPPORTS1(nsQDFlushPort, nsITimerCallback)

// Notify(aTimer)
//
// Timer callback.  Flush the dirty port buffer to the screen.
NS_IMETHODIMP
nsQDFlushPort::Notify(nsITimer* aTimer)
{
  NS_ASSERTION(aTimer == mFlushTimer, "Callback called by wrong timer");

  // Flush the dirty region.
  ::QDFlushPortBuffer(mPort, NULL);

  mLastFlushTime = ::UpTime();
  mFlushTimerRunning = PR_FALSE;

  // This shouldn't be necessary, nsITimer.idl
  // aTimer->Cancel();

  return NS_OK;
}

// nsQDFlushObserver

nsQDFlushObserver::nsQDFlushObserver(nsQDFlushManager* aParent)
: mParent(aParent)
{
}

nsQDFlushObserver::~nsQDFlushObserver()
{
}

// nsIObserver implementation

NS_IMPL_ISUPPORTS1(nsQDFlushObserver, nsIObserver)

// Observe(aSubject, aTopic, aData)
//
// Shutdown callback.  Delete static object and any attached port objects.
NS_IMETHODIMP
nsQDFlushObserver::Observe(nsISupports* aSubject, const char* aTopic,
                           const PRUnichar* aData)
{
  NS_ASSERTION(!nsCRT::strcmp(NS_XPCOM_SHUTDOWN_OBSERVER_ID,aTopic),
               "Shutdown observer called with wrong topic\n");
  delete mParent;
  return NS_OK;
}
