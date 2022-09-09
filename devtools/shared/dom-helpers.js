/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

exports.DOMHelpers = {
  /**
   * A simple way to be notified (once) when a window becomes
   * interactive (DOMContentLoaded).
   *
   * It is based on the chromeEventHandler. This is useful when
   * chrome iframes are loaded in content docshells (in Firefox
   * tabs for example).
   *
   * @param nsIDOMWindow win
   *        The content window, owning the document to traverse.
   * @param Function callback
   *        The method to call when the frame is loaded.
   * @param String targetURL
   *        (optional) Check that the frame URL corresponds to the provided URL
   *        before calling the callback.
   */
  onceDOMReady(win, callback, targetURL) {
    if (!win) {
      throw new Error("window can't be null or undefined");
    }
    const docShell = win.docShell;
    const onReady = function(event) {
      if (event.target == win.document) {
        docShell.chromeEventHandler.removeEventListener(
          "DOMContentLoaded",
          onReady
        );
        // If in `callback` the URL of the window is changed and a listener to DOMContentLoaded
        // is attached, the event we just received will be also be caught by the new listener.
        // We want to avoid that so we execute the callback in the next queue.
        Services.tm.dispatchToMainThread(callback);
      }
    };
    if (
      (win.document.readyState == "complete" ||
        win.document.readyState == "interactive") &&
      win.location.href == targetURL
    ) {
      Services.tm.dispatchToMainThread(callback);
    } else {
      docShell.chromeEventHandler.addEventListener("DOMContentLoaded", onReady);
    }
  },
};
