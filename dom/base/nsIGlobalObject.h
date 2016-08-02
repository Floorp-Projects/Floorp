/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIGlobalObject_h__
#define nsIGlobalObject_h__

#include "nsISupports.h"
#include "nsTArray.h"
#include "js/TypeDecls.h"

#define NS_IGLOBALOBJECT_IID \
{ 0x11afa8be, 0xd997, 0x4e07, \
{ 0xa6, 0xa3, 0x6f, 0x87, 0x2e, 0xc3, 0xee, 0x7f } }

class nsACString;
class nsCString;
class nsCycleCollectionTraversalCallback;
class nsIPrincipal;

class nsIGlobalObject : public nsISupports
{
  nsTArray<nsCString> mHostObjectURIs;
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

  // GetGlobalJSObject may return a gray object.  If this ever changes so that
  // it stops doing that, please simplify the code in FindAssociatedGlobal in
  // BindingUtils.h that does JS::ExposeObjectToActiveJS on the return value of
  // GetGlobalJSObject.  Also, in that case the JS::ExposeObjectToActiveJS in
  // AutoJSAPI::InitInternal can probably be removed.  And also the similar
  // calls in XrayWrapper and nsGlobalWindow.
  virtual JSObject* GetGlobalJSObject() = 0;

  // This method is not meant to be overridden.
  nsIPrincipal* PrincipalOrNull();

  void RegisterHostObjectURI(const nsACString& aURI);

  void UnregisterHostObjectURI(const nsACString& aURI);

  // Any CC class inheriting nsIGlobalObject should call these 2 methods if it
  // exposes the URL API.
  void UnlinkHostObjectURIs();
  void TraverseHostObjectURIs(nsCycleCollectionTraversalCallback &aCb);

protected:
  virtual ~nsIGlobalObject();

  void
  StartDying()
  {
    mIsDying = true;
  }
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIGlobalObject,
                              NS_IGLOBALOBJECT_IID)

#endif // nsIGlobalObject_h__
