/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsLayoutStatics_h__
#define nsLayoutStatics_h__

#include "nscore.h"
#include "MainThreadUtils.h"
#include "nsTraceRefcnt.h"
#include "nsDebug.h"

// This isn't really a class, it's a namespace for static methods.
// Documents and other objects can hold a reference to the layout static
// objects so that they last past the xpcom-shutdown notification.

class nsLayoutStatics
{
public:
  // Called by the layout module constructor. This call performs an AddRef()
  // internally.
  static nsresult Initialize();

  static void AddRef()
  {
    NS_ASSERTION(NS_IsMainThread(),
                 "nsLayoutStatics reference counting must be on main thread");

    NS_ASSERTION(sLayoutStaticRefcnt,
                 "nsLayoutStatics already dropped to zero!");

    ++sLayoutStaticRefcnt;
    NS_LOG_ADDREF(&sLayoutStaticRefcnt, sLayoutStaticRefcnt,
                  "nsLayoutStatics", 1);
  }
  static void Release()
  {
    NS_ASSERTION(NS_IsMainThread(),
                 "nsLayoutStatics reference counting must be on main thread");

    --sLayoutStaticRefcnt;
    NS_LOG_RELEASE(&sLayoutStaticRefcnt, sLayoutStaticRefcnt,
                   "nsLayoutStatics");

    if (!sLayoutStaticRefcnt)
      Shutdown();
  }

private:
  // not to be called!
  nsLayoutStatics();

  static void Shutdown();

  static nsrefcnt sLayoutStaticRefcnt;
};

class nsLayoutStaticsRef
{
public:
  nsLayoutStaticsRef()
  {
    nsLayoutStatics::AddRef();
  }
  ~nsLayoutStaticsRef()
  {
    nsLayoutStatics::Release();
  }
};

#endif // nsLayoutStatics_h__
