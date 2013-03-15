/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["RecentWindow"];

const Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/PrivateBrowsingUtils.jsm");

#ifndef XP_WIN
#define BROKEN_WM_Z_ORDER
#endif

this.RecentWindow = {
  /*
   * Get the most recent browser window.
   *
   * @param aOptions an object accepting the arguments for the search.
   *        Set the private property to true in order to restrict the
   *        search to private windows only, or to false in order to
   *        restrict the search to non-private windows only.  To search
   *        in both groups, don't specify the private property.
   */
  getMostRecentBrowserWindow: function RW_getMostRecentBrowserWindow(aOptions) {
    let checkPrivacy = typeof aOptions == "object" &&
                       "private" in aOptions;

    function isSuitableBrowserWindow(win) {
      return (!win.closed &&
              win.toolbar.visible &&
              (!checkPrivacy ||
               PrivateBrowsingUtils.permanentPrivateBrowsing ||
               PrivateBrowsingUtils.isWindowPrivate(win) == aOptions.private));
    }

#ifdef BROKEN_WM_Z_ORDER
    let win = Services.wm.getMostRecentWindow("navigator:browser");

    // if we're lucky, this isn't a popup, and we can just return this
    if (win && !isSuitableBrowserWindow(win)) {
      win = null;
      let windowList = Services.wm.getEnumerator("navigator:browser");
      // this is oldest to newest, so this gets a bit ugly
      while (windowList.hasMoreElements()) {
        let nextWin = windowList.getNext();
        if (isSuitableBrowserWindow(nextWin))
          win = nextWin;
      }
    }
    return win;
#else
    let windowList = Services.wm.getZOrderDOMWindowEnumerator("navigator:browser", true);
    while (windowList.hasMoreElements()) {
      let win = windowList.getNext();
      if (isSuitableBrowserWindow(win))
        return win;
    }
    return null;
#endif
  }
};

