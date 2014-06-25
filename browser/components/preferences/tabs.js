# -*- indent-tabs-mode: nil; js-indent-level: 4 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

var gTabsPane = {

  /*
   * Preferences:
   *
   * browser.link.open_newwindow
   * - determines where pages which would open in a new window are opened:
   *     1 opens such links in the most recent window or tab,
   *     2 opens such links in a new window,
   *     3 opens such links in a new tab
   * browser.tabs.loadInBackground
   * - true if display should switch to a new tab which has been opened from a
   *   link, false if display shouldn't switch
   * browser.tabs.warnOnClose
   * - true if when closing a window with multiple tabs the user is warned and
   *   allowed to cancel the action, false to just close the window
   * browser.tabs.warnOnOpen
   * - true if the user should be warned if he attempts to open a lot of tabs at
   *   once (e.g. a large folder of bookmarks), false otherwise
   * browser.taskbar.previews.enable
   * - true if tabs are to be shown in the Windows 7 taskbar
   */

#ifdef XP_WIN
  /**
   * Initialize any platform-specific UI.
   */
  init: function () {
    const Cc = Components.classes;
    const Ci = Components.interfaces;
    try {
      let sysInfo = Cc["@mozilla.org/system-info;1"].
                    getService(Ci.nsIPropertyBag2);
      let ver = parseFloat(sysInfo.getProperty("version"));
      let showTabsInTaskbar = document.getElementById("showTabsInTaskbar");
      showTabsInTaskbar.hidden = ver < 6.1;
    } catch (ex) {}
  },
#endif

  /**
   * Determines where a link which opens a new window will open.
   *
   * @returns |true| if such links should be opened in new tabs
   */
  readLinkTarget: function() {
    var openNewWindow = document.getElementById("browser.link.open_newwindow");
    return openNewWindow.value != 2;
  },

  /**
   * Determines where a link which opens a new window will open.
   *
   * @returns 2 if such links should be opened in new windows,
   *          3 if such links should be opened in new tabs
   */
  writeLinkTarget: function() {
    var linkTargeting = document.getElementById("linkTargeting");
    return linkTargeting.checked ? 3 : 2;
  }
};
