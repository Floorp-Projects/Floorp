/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TabContext_h
#define mozilla_dom_TabContext_h

#include "nsCOMPtr.h"
#include "mozilla/BasePrincipal.h"
#include "nsPIDOMWindow.h"
#include "nsPIWindowRoot.h"

namespace mozilla {
namespace dom {

class IPCTabContext;

/**
 * TabContext encapsulates information about an iframe that may be a mozbrowser.
 *
 * BrowserParent and BrowserChild both inherit from TabContext, and you can also
 * have standalone TabContext objects.
 *
 * This class is immutable except by calling one of the protected
 * SetTabContext*() methods (and those methods can only be called once).  See
 * also MutableTabContext.
 */
class TabContext {
 public:
  TabContext();

  /* (The implicit copy-constructor and operator= are fine.) */

  /**
   * Generates IPCTabContext of type BrowserFrameIPCTabContext from this
   * TabContext's information.
   */
  IPCTabContext AsIPCTabContext() const;

  /**
   * Does this TabContext correspond to a mozbrowser?
   *
   * <iframe mozbrowser> is a mozbrowser element, but <xul:browser> is not.
   */
  bool IsMozBrowserElement() const;

  bool IsJSPlugin() const;
  int32_t JSPluginId() const;

  uint64_t ChromeOuterWindowID() const;

  /**
   * OriginAttributesRef() returns the OriginAttributes of this frame to
   * the caller. This is used to store any attribute associated with the frame's
   * docshell.
   */
  const OriginAttributes& OriginAttributesRef() const;

  /**
   * Returns the presentation URL associated with the tab if this tab is
   * created for presented content
   */
  const nsAString& PresentationURL() const;

  UIStateChangeType ShowFocusRings() const;

  uint32_t MaxTouchPoints() const { return mMaxTouchPoints; }

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
   * Set the tab context's origin attributes to a private browsing value.
   */
  void SetPrivateBrowsingAttributes(bool aIsPrivateBrowsing);

  /**
   * Set the first party domain of the tab context's origin attributes.
   */
  void SetFirstPartyDomainAttributes(const nsAString& aFirstPartyDomain);

  bool SetTabContext(bool aIsMozBrowserElement, uint64_t aChromeOuterWindowID,
                     UIStateChangeType aShowFocusRings,
                     const OriginAttributes& aOriginAttributes,
                     const nsAString& aPresentationURL,
                     uint32_t aMaxTouchPoints);

  /**
   * Modify this TabContext to match the given TabContext.  This is a special
   * case triggered by nsFrameLoader::SwapWithOtherRemoteLoader which may have
   * caused the owner content to change.
   *
   * This special case only allows the field `mIsMozBrowserElement` to be
   * changed.  If any other fields have changed, the update is ignored and
   * returns false.
   */
  bool UpdateTabContextAfterSwap(const TabContext& aContext);

  /**
   * Set this TabContext to be for a JS plugin. aPluginID is the id of the JS
   * plugin
   * (@see nsFakePlugin::mId).
   * As with the other protected mutator methods, this lets you modify a
   * TabContext once.
   * (@see TabContext::SetTabContext above for more details).
   */
  bool SetTabContextForJSPluginFrame(int32_t aJSPluginID);

  void SetMaxTouchPoints(uint32_t aMaxTouchPoints) {
    mMaxTouchPoints = aMaxTouchPoints;
  }

 private:
  /**
   * Has this TabContext been initialized?  If so, mutator methods will fail.
   */
  bool mInitialized;

  /**
   * Whether this TabContext corresponds to a mozbrowser.
   *
   * <iframe mozbrowser> and <xul:browser> are not considered to be
   * mozbrowser elements.
   */
  bool mIsMozBrowserElement;

  /**
   * The outerWindowID of the window hosting the remote frameloader.
   */
  uint64_t mChromeOuterWindowID;

  int32_t mJSPluginID;

  /**
   * OriginAttributes of the top level tab docShell
   */
  OriginAttributes mOriginAttributes;

  /**
   * The requested presentation URL.
   */
  nsString mPresentationURL;

  /**
   * Keyboard indicator state (focus rings).
   */
  UIStateChangeType mShowFocusRings;

  /**
   * Maximum number of touch points.
   */
  uint32_t mMaxTouchPoints;
};

/**
 * MutableTabContext is the same as MaybeInvalidTabContext, except the mutation
 * methods are public instead of protected.  You can still only call these
 * mutation methods once on a given object.
 */
class MutableTabContext : public TabContext {
 public:
  bool SetTabContext(const TabContext& aContext) {
    return TabContext::SetTabContext(aContext);
  }

  bool SetTabContext(bool aIsMozBrowserElement, uint64_t aChromeOuterWindowID,
                     UIStateChangeType aShowFocusRings,
                     const OriginAttributes& aOriginAttributes,
                     const nsAString& aPresentationURL,
                     uint32_t aMaxTouchPoints) {
    return TabContext::SetTabContext(aIsMozBrowserElement, aChromeOuterWindowID,
                                     aShowFocusRings, aOriginAttributes,
                                     aPresentationURL, aMaxTouchPoints);
  }

  bool SetTabContextForJSPluginFrame(uint32_t aJSPluginID) {
    return TabContext::SetTabContextForJSPluginFrame(aJSPluginID);
  }

  void SetFirstPartyDomainAttributes(const nsAString& aFirstPartyDomain) {
    TabContext::SetFirstPartyDomainAttributes(aFirstPartyDomain);
  }
};

/**
 * MaybeInvalidTabContext is a simple class that lets you transform an
 * IPCTabContext into a TabContext.
 *
 * The issue is that an IPCTabContext is not necessarily valid.  So to convert
 * an IPCTabContext into a TabContext, you construct a MaybeInvalidTabContext,
 * check whether it's valid, and, if so, read out your TabContext.
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
class MaybeInvalidTabContext {
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

}  // namespace dom
}  // namespace mozilla

#endif
