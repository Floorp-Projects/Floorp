/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TabContext_h
#define mozilla_dom_TabContext_h

#include "mozIApplication.h"
#include "nsCOMPtr.h"
#include "mozilla/BasePrincipal.h"

namespace mozilla {
namespace dom {

class IPCTabContext;

/**
 * TabContext encapsulates information about an iframe that may be a mozbrowser
 * or mozapp.  You can ask whether a TabContext corresponds to a mozbrowser or
 * mozapp, get the app that contains the browser, and so on.
 *
 * TabParent and TabChild both inherit from TabContext, and you can also have
 * standalone TabContext objects.
 *
 * This class is immutable except by calling one of the protected
 * SetTabContext*() methods (and those methods can only be called once).  See
 * also MutableTabContext.
 */
class TabContext
{
public:
  TabContext();

  /* (The implicit copy-constructor and operator= are fine.) */

  /**
   * Generates IPCTabContext of type BrowserFrameIPCTabContext or
   * AppFrameIPCTabContext from this TabContext's information.
   */
  IPCTabContext AsIPCTabContext() const;

  /**
   * Does this TabContext correspond to a mozbrowser?  (<iframe mozbrowser
   * mozapp> is not a browser.)
   *
   * If IsBrowserElement() is true, HasOwnApp() and HasAppOwnerApp() are
   * guaranteed to be false.
   *
   * If IsBrowserElement() is false, HasBrowserOwnerApp() is guaranteed to be
   * false.
   */
  bool IsBrowserElement() const;

  /**
   * Does this TabContext correspond to a mozbrowser or mozapp?  This is
   * equivalent to IsBrowserElement() || HasOwnApp().
   */
  bool IsBrowserOrApp() const;

  /**
   * OwnAppId() returns the id of the app which directly corresponds to this
   * context's frame.  GetOwnApp() returns the corresponding app object, and
   * HasOwnApp() returns true iff GetOwnApp() would return a non-null value.
   *
   * If HasOwnApp() is true, IsBrowserElement() is guaranteed to be false.
   */
  uint32_t OwnAppId() const;
  already_AddRefed<mozIApplication> GetOwnApp() const;
  bool HasOwnApp() const;

  /**
   * BrowserOwnerAppId() gets the ID of the app which contains this browser
   * frame.  If this is not a browser frame (i.e., if !IsBrowserElement()), then
   * BrowserOwnerAppId() is guaranteed to return NO_APP_ID.
   *
   * Even if we are a browser frame, BrowserOwnerAppId() may still return
   * NO_APP_ID, if this browser frame is not contained inside an app.
   */
  uint32_t BrowserOwnerAppId() const;
  already_AddRefed<mozIApplication> GetBrowserOwnerApp() const;
  bool HasBrowserOwnerApp() const;

  /**
   * AppOwnerAppId() gets the ID of the app which contains this app frame.  If
   * this is not an app frame (i.e., if !HasOwnApp()), then AppOwnerAppId() is
   * guaranteed to return NO_APP_ID.
   *
   * Even if we are an app frame, AppOwnerAppId() may still return NO_APP_ID, if
   * this app frame is not contained inside an app.
   */
  uint32_t AppOwnerAppId() const;
  already_AddRefed<mozIApplication> GetAppOwnerApp() const;
  bool HasAppOwnerApp() const;

  /**
   * OwnOrContainingAppId() gets the ID of this frame, if HasOwnApp().  If this
   * frame does not have its own app, it gets the ID of the app which contains
   * this frame (i.e., the result of {Browser,App}OwnerAppId(), as applicable).
   */
  uint32_t OwnOrContainingAppId() const;
  already_AddRefed<mozIApplication> GetOwnOrContainingApp() const;
  bool HasOwnOrContainingApp() const;

  /**
   * OriginAttributesRef() returns the DocShellOriginAttributes of this frame to
   * the caller. This is used to store any attribute associated with the frame's
   * docshell, such as the AppId.
   */
  const DocShellOriginAttributes& OriginAttributesRef() const;

  /**
   * Returns the origin associated with the tab (w/o suffix) if this tab owns
   * a signed packaged content.
   */
  const nsACString& SignedPkgOriginNoSuffix() const;

protected:
  friend class MaybeInvalidTabContext;

  /**
   * These protected mutator methods let you modify a TabContext once.  Further
   * attempts to modify a given TabContext will fail (the method will return
   * false).
   *
   * These mutators will also fail if the TabContext was created with anything
   * other than the no-args constructor.
   */

  /**
   * Set this TabContext to match the given TabContext.
   */
  bool SetTabContext(const TabContext& aContext);

  /**
   * Set the TabContext for this frame. This can either be:
   *  - an app frame (with the given own app) inside the given owner app. Either
   *    apps can be null.
   *  - a browser frame inside the given owner app (which may be null).
   *  - a non-browser, non-app frame. Both own app and owner app should be null.
   */
  bool SetTabContext(mozIApplication* aOwnApp,
                     mozIApplication* aAppFrameOwnerApp,
                     const DocShellOriginAttributes& aOriginAttributes,
                     const nsACString& aSignedPkgOriginNoSuffix);

private:
  /**
   * Has this TabContext been initialized?  If so, mutator methods will fail.
   */
  bool mInitialized;

  /**
   * This TabContext's own app.  If this is non-null, then this
   * TabContext corresponds to an app, and mIsBrowser must be false.
   */
  nsCOMPtr<mozIApplication> mOwnApp;

  /**
   * This TabContext's containing app.  If mIsBrowser, this corresponds to the
   * app which contains the browser frame; otherwise, this corresponds to the
   * app which contains the app frame.
   */
  nsCOMPtr<mozIApplication> mContainingApp;

  /*
   * Cache of mContainingApp->GetLocalId().
   */
  uint32_t mContainingAppId;

  /**
   * DocShellOriginAttributes of the top level tab docShell
   */
  DocShellOriginAttributes mOriginAttributes;

  /**
   * The signed package origin without suffix. Since the signed packaged
   * web content is always loaded in a separate process, it makes sense
   * that we store this immutable value in TabContext. If the TabContext
   * doesn't own a signed package, this value would be empty.
   */
  nsCString mSignedPkgOriginNoSuffix;
};

/**
 * MutableTabContext is the same as MaybeInvalidTabContext, except the mutation
 * methods are public instead of protected.  You can still only call these
 * mutation methods once on a given object.
 */
class MutableTabContext : public TabContext
{
public:
  bool SetTabContext(const TabContext& aContext)
  {
    return TabContext::SetTabContext(aContext);
  }

  bool
  SetTabContext(mozIApplication* aOwnApp,
                mozIApplication* aAppFrameOwnerApp,
                const DocShellOriginAttributes& aOriginAttributes,
                const nsACString& aSignedPkgOriginNoSuffix = EmptyCString())
  {
    return TabContext::SetTabContext(aOwnApp,
                                     aAppFrameOwnerApp,
                                     aOriginAttributes,
                                     aSignedPkgOriginNoSuffix);
  }
};

/**
 * MaybeInvalidTabContext is a simple class that lets you transform an
 * IPCTabContext into a TabContext.
 *
 * The issue is that an IPCTabContext is not necessarily valid; for example, it
 * might specify an app-id which doesn't exist.  So to convert an IPCTabContext
 * into a TabContext, you construct a MaybeInvalidTabContext, check whether it's
 * valid, and, if so, read out your TabContext.
 *
 * Example usage:
 *
 *   void UseTabContext(const TabContext& aTabContext);
 *
 *   void CreateTab(const IPCTabContext& aContext) {
 *     MaybeInvalidTabContext tc(aContext);
 *     if (!tc.IsValid()) {
 *       NS_ERROR(nsPrintfCString("Got an invalid IPCTabContext: %s",
 *                                tc.GetInvalidReason()));
 *       return;
 *     }
 *     UseTabContext(tc.GetTabContext());
 *   }
 */
class MaybeInvalidTabContext
{
public:
  /**
   * This constructor copies the information in aContext and sets IsValid() as
   * appropriate.
   */
  explicit MaybeInvalidTabContext(const IPCTabContext& aContext);

  /**
   * Was the IPCTabContext we received in our constructor valid?
   */
  bool IsValid();

  /**
   * If IsValid(), this function returns null.  Otherwise, it returns a
   * human-readable string indicating why the IPCTabContext passed to our
   * constructor was not valid.
   */
  const char* GetInvalidReason();

  /**
   * If IsValid(), this function returns a reference to a TabContext
   * corresponding to the IPCTabContext passed to our constructor.  If
   * !IsValid(), this function crashes.
   */
  const TabContext& GetTabContext();

private:
  MaybeInvalidTabContext(const MaybeInvalidTabContext&) = delete;
  MaybeInvalidTabContext& operator=(const MaybeInvalidTabContext&) = delete;

  const char* mInvalidReason;
  MutableTabContext mTabContext;
};

} // namespace dom
} // namespace mozilla

#endif
