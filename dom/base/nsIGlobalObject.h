/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIGlobalObject_h__
#define nsIGlobalObject_h__

#include "nsISupports.h"
#include "js/TypeDecls.h"

#define NS_IGLOBALOBJECT_IID \
{ 0xe2538ded, 0x13ef, 0x4f4d, \
{ 0x94, 0x6b, 0x65, 0xd3, 0x33, 0xb4, 0xf0, 0x3c } }

class nsIPrincipal;

class nsIGlobalObject : public nsISupports
{
  bool mIsDying;

protected:
  nsIGlobalObject()
   : mIsDying(false)
  {}

public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IGLOBALOBJECT_IID)

  /**
   * This check is added to deal with Promise microtask queues. On the main
   * thread, we do not impose restrictions about when script stops running or
   * when runnables can no longer be dispatched to the main thread. This means
   * it is possible for a Promise chain to keep resolving an infinite chain of
   * promises, preventing the browser from shutting down. See Bug 1058695. To
   * prevent this, the nsGlobalWindow subclass sets this flag when it is
   * closed. The Promise implementation checks this and prohibits new runnables
   * from being dispatched.
   *
   * We pair this with checks during processing the promise microtask queue
   * that pops up the slow script dialog when the Promise queue is preventing
   * a window from going away.
   */
  bool
  IsDying() const
  {
    return mIsDying;
  }

  virtual JSObject* GetGlobalJSObject() = 0;

  // This method is not meant to be overridden.
  nsIPrincipal* PrincipalOrNull();

protected:
  void
  StartDying()
  {
    mIsDying = true;
  }
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIGlobalObject,
                              NS_IGLOBALOBJECT_IID)

#endif // nsIGlobalObject_h__
