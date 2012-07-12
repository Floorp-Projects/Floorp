/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_BrowserElementHelpers_h
#define mozilla_BrowserElementHelpers_h

#include "nsAString.h"

class nsIDOMWindow;
class nsIURI;

namespace mozilla {

namespace dom {
class TabParent;
}

/**
 * BrowserElementParent implements a portion of the parent-process side of
 * <iframe mozbrowser>.
 *
 * Most of the parent-process side of <iframe mozbrowser> is implemented in
 * BrowserElementParent.js.  This file implements the few parts of this
 * functionality which must be written in C++.
 *
 * We don't communicate with the JS code that lives in BrowserElementParent.js;
 * the JS and C++ parts are completely separate.
 */
class BrowserElementParent
{
public:
  /**
   * Handle a window.open call from an out-of-process <iframe mozbrowser>.
   *
   * window.open inside <iframe mozbrowser> doesn't actually open a new
   * top-level window.  Instead, the "embedder" (the document which contains
   * the <iframe mozbrowser> whose content called window.open) gets the
   * opportunity to place a new <iframe mozbrowser> in the DOM somewhere.  This
   * new "popup" iframe acts as the opened window.
   *
   * This method proceeds in three steps.
   *
   * 1) We fire a mozbrowseropenwindow CustomEvent on the opener
   *    iframe element.  This event's detail is an instance of
   *    nsIOpenWindowEventDetail.
   *
   * 2) The embedder (the document which contains the opener iframe) can accept
   *    the window.open request by inserting event.detail.frameElement (an iframe
   *    element) into the DOM somewhere.
   *
   * 3) If the embedder accepted the window.open request, we return true and
   *    set aPopupTabParent's frame element to event.detail.frameElement.
   *    Otherwise, we return false.
   *
   * @param aOpenerTabParent the TabParent whose TabChild called window.open.
   * @param aPopupTabParent the TabParent inside which the opened window will
   *                        live.
   * @return true on success, false otherwise.  Failure is not (necessarily)
   *         an error; it may indicate that the embedder simply rejected the
   *         window.open request.
   */
  static bool
  OpenWindowOOP(mozilla::dom::TabParent* aOpenerTabParent,
                mozilla::dom::TabParent* aPopupTabParent,
                const nsAString& aURL,
                const nsAString& aName,
                const nsAString& aFeatures);

  /**
   * Handle a window.open call from an in-process <iframe mozbrowser>.
   *
   * As with OpenWindowOOP, we return true if the window.open request
   * succeeded, and return false if the embedder denied the request.
   *
   * (These parameter types are silly, but they match what our caller has in
   * hand.  Feel free to add an override, if they are inconvenient to you.)
   */
  static bool
  OpenWindowInProcess(nsIDOMWindow* aOpenerWindow,
                      nsIURI* aURI,
                      const nsAString& aName,
                      const nsACString& aFeatures,
                      nsIDOMWindow** aReturnWindow);
};

} // namespace mozilla

#endif
