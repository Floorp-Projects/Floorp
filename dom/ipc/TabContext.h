/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TabContext_h
#define mozilla_dom_TabContext_h

#include "mozilla/Assertions.h"
#include "mozilla/dom/PContent.h"
#include "mozilla/dom/PBrowser.h"
#include "mozilla/layout/RenderFrameUtils.h"
#include "nsIScriptSecurityManager.h"
#include "mozIApplication.h"

namespace mozilla {
namespace dom {

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
protected:
  typedef mozilla::layout::ScrollingBehavior ScrollingBehavior;

public:
  /**
   * This constructor sets is-browser to false, and sets all relevant apps to
   * NO_APP_ID.  If you inherit from TabContext, you can mutate this object
   * exactly once by calling one of the protected SetTabContext*() methods.
   */
  TabContext();

  /**
   * This constructor copies the information in aContext.  The TabContext is
   * immutable after calling this method; you won't be able call any of the
   * protected SetTabContext*() methods on an object constructed using this
   * constructor.
   *
   * If aContext is a PopupIPCTabContext with isBrowserElement false and whose
   * openerParent is a browser element, this constructor will crash (even in
   * release builds).  So please check that case before calling this method.
   */
  TabContext(const IPCTabContext& aContext);

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
   * Return the requested scrolling behavior for this frame.
   */
  ScrollingBehavior GetScrollingBehavior() const { return mScrollingBehavior; }

protected:
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
   * Set this TabContext to be an app frame (with the given own app) inside the
   * given app.  Either or both apps may be null.
   */
  bool SetTabContextForAppFrame(mozIApplication* aOwnApp,
                                mozIApplication* aAppFrameOwnerApp,
                                ScrollingBehavior aRequestedBehavior);

  /**
   * Set this TabContext to be a browser frame inside the given app (which may
   * be null).
   */
  bool SetTabContextForBrowserFrame(mozIApplication* aBrowserFrameOwnerApp,
                                    ScrollingBehavior aRequestedBehavior);

private:
  /**
   * Translate an appId into a mozIApplication.
   */
  already_AddRefed<mozIApplication> GetAppForId(uint32_t aAppId) const;

  /**
   * Has this TabContext been initialized?  If so, mutator methods will fail.
   */
  bool mInitialized;

  /**
   * This TabContext's own app id.  If this is something other than NO_APP_ID,
   * then this TabContext corresponds to an app, and mIsBrowser must be false.
   */
  uint32_t mOwnAppId;

  /**
   * The id of the app which contains this TabContext's frame.  If mIsBrowser,
   * this corresponds to the ID of the app which contains the browser frame;
   * otherwise, this correspodns to the ID of the app which contains the app
   * frame.
   */
  uint32_t mContainingAppId;

  /**
   * The requested scrolling behavior for this frame.
   */
  ScrollingBehavior mScrollingBehavior;

  /**
   * Does this TabContext correspond to a browser element?
   *
   * If this is true, mOwnAppId must be NO_APP_ID.
   */
  bool mIsBrowser;
};

/**
 * MutableTabContext is the same as TabContext, except the mutation methods are
 * public instead of protected.  You can still only call these mutation methods
 * once on a given object.
 */
class MutableTabContext : public TabContext
{
public:
  bool SetTabContext(const TabContext& aContext)
  {
    return TabContext::SetTabContext(aContext);
  }

  bool SetTabContextForAppFrame(mozIApplication* aOwnApp, mozIApplication* aAppFrameOwnerApp,
                                ScrollingBehavior aRequestedBehavior)
  {
    return TabContext::SetTabContextForAppFrame(aOwnApp, aAppFrameOwnerApp,
                                                aRequestedBehavior);
  }

  bool SetTabContextForBrowserFrame(mozIApplication* aBrowserFrameOwnerApp,
                                    ScrollingBehavior aRequestedBehavior)
  {
    return TabContext::SetTabContextForBrowserFrame(aBrowserFrameOwnerApp,
                                                    aRequestedBehavior);
  }
};

} // namespace dom
} // namespace mozilla

#endif
