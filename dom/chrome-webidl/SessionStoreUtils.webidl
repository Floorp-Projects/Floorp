/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

interface nsIDocShell;
interface nsISupports;

/**
 * A callback passed to SessionStoreUtils.forEachNonDynamicChildFrame().
 */
callback SessionStoreUtilsFrameCallback = void (WindowProxy frame, unsigned long index);

/**
 * SessionStore utility functions implemented in C++ for performance reasons.
 */
[ChromeOnly, Exposed=Window]
namespace SessionStoreUtils {
  /**
   * Calls the given |callback| once for each non-dynamic child frame of the
   * given |window|.
   */
  [Throws]
  void forEachNonDynamicChildFrame(WindowProxy window,
                                   SessionStoreUtilsFrameCallback callback);

  /**
   * Takes the given listener, wraps it in a filter that filters out events from
   * dynamic docShells, and adds that filter as a listener for the given event
   * type on the given event target.  The listener that was added is returned
   * (as nsISupports) so that it can later be removed via
   * removeDynamicFrameFilteredListener.
   *
   * This is implemented as a native filter, rather than a JS-based one, for
   * performance reasons.
   */
  [Throws]
  nsISupports? addDynamicFrameFilteredListener(EventTarget target,
                                               DOMString type,
                                               any listener,
                                               boolean useCapture);

  /**
   * Remove the passed-in filtered listener from the given event target, if it's
   * currently a listener for the given event type there.  The 'listener'
   * argument must be something that was returned by
   * addDynamicFrameFilteredListener.
   *
   * This is needed, instead of the normal removeEventListener, because the
   * caller doesn't actually have something that WebIDL considers an
   * EventListener.
   */
  [Throws]
  void removeDynamicFrameFilteredListener(EventTarget target,
                                          DOMString type,
                                          nsISupports listener,
                                          boolean useCapture);

  /*
   * Save the docShell.allow* properties
   */
  ByteString collectDocShellCapabilities(nsIDocShell docShell);

  /*
   * Restore the docShell.allow* properties
   */
  void restoreDocShellCapabilities(nsIDocShell docShell,
                                   ByteString disallowCapabilities);

  /**
   * Collects scroll position data for any given |frame| in the frame hierarchy.
   *
   * @param document (DOMDocument)
   *
   * @return {scroll: "x,y"} e.g. {scroll: "100,200"}
   *         Returns null when there is no scroll data we want to store for the
   *         given |frame|.
   */
  SSScrollPositionDict collectScrollPosition(Document document);

  /**
   * Restores scroll position data for any given |frame| in the frame hierarchy.
   *
   * @param frame (DOMWindow)
   * @param value (object, see collectScrollPosition())
   */
  void restoreScrollPosition(Window frame, optional SSScrollPositionDict data);
};

dictionary SSScrollPositionDict {
  ByteString scroll;
};
