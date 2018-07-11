/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/browser-window */

function OpenBrowserWindowFromDockMenu(options) {
  let win = OpenBrowserWindow(options);
  win.addEventListener("load", function() {
    let dockSupport = Cc["@mozilla.org/widget/macdocksupport;1"]
      .getService(Ci.nsIMacDockSupport);
    dockSupport.activateApplication(true);
  }, { once: true });

  return win;
}

gBrowserInit.nonBrowserWindowStartup = function() {
  // Disable inappropriate commands / submenus
  var disabledItems = ["Browser:SavePage",
                       "Browser:SendLink", "cmd_pageSetup", "cmd_print", "cmd_find", "cmd_findAgain",
                       "viewToolbarsMenu", "viewSidebarMenuMenu", "Browser:Reload",
                       "viewFullZoomMenu", "pageStyleMenu", "charsetMenu", "View:PageSource", "View:FullScreen",
                       "viewHistorySidebar", "Browser:AddBookmarkAs", "Browser:BookmarkAllTabs",
                       "View:PageInfo", "History:UndoCloseTab"];
  var element;

  for (let disabledItem of disabledItems) {
    element = document.getElementById(disabledItem);
    if (element)
      element.setAttribute("disabled", "true");
  }

  // Show menus that are only visible in non-browser windows
  let shownItems = ["menu_openLocation"];
  for (let shownItem of shownItems) {
    element = document.getElementById(shownItem);
    if (element)
      element.removeAttribute("hidden");
  }

  // If no windows are active (i.e. we're the hidden window), disable the close, minimize
  // and zoom menu commands as well
  if (window.location.href == "chrome://browser/content/hiddenWindow.xul") {
    var hiddenWindowDisabledItems = ["cmd_close", "minimizeWindow", "zoomWindow"];
    for (let hiddenWindowDisabledItem of hiddenWindowDisabledItems) {
      element = document.getElementById(hiddenWindowDisabledItem);
      if (element)
        element.setAttribute("disabled", "true");
    }

    // also hide the window-list separator
    element = document.getElementById("sep-window-list");
    element.setAttribute("hidden", "true");

    // Setup the dock menu.
    let dockMenuElement = document.getElementById("menu_mac_dockmenu");
    if (dockMenuElement != null) {
      let nativeMenu = Cc["@mozilla.org/widget/standalonenativemenu;1"]
                       .createInstance(Ci.nsIStandaloneNativeMenu);

      try {
        nativeMenu.init(dockMenuElement);

        let dockSupport = Cc["@mozilla.org/widget/macdocksupport;1"]
                          .getService(Ci.nsIMacDockSupport);
        dockSupport.dockMenu = nativeMenu;
      } catch (e) {
      }
    }
  }

  if (PrivateBrowsingUtils.permanentPrivateBrowsing) {
    document.getElementById("macDockMenuNewWindow").hidden = true;
  }
  if (!PrivateBrowsingUtils.enabled) {
    document.getElementById("macDockMenuNewPrivateWindow").hidden = true;
  }

  this._delayedStartupTimeoutId = setTimeout(this.nonBrowserWindowDelayedStartup.bind(this), 0);
};

gBrowserInit.nonBrowserWindowDelayedStartup = function() {
  this._delayedStartupTimeoutId = null;

  // initialise the offline listener
  BrowserOffline.init();

  // initialize the private browsing UI
  gPrivateBrowsingUI.init();

};

gBrowserInit.nonBrowserWindowShutdown = function() {
  let dockSupport = Cc["@mozilla.org/widget/macdocksupport;1"]
                    .getService(Ci.nsIMacDockSupport);
  dockSupport.dockMenu = null;

  // If nonBrowserWindowDelayedStartup hasn't run yet, we have no work to do -
  // just cancel the pending timeout and return;
  if (this._delayedStartupTimeoutId) {
    clearTimeout(this._delayedStartupTimeoutId);
    return;
  }

  BrowserOffline.uninit();
};

addEventListener("load",   function() { gBrowserInit.nonBrowserWindowStartup()  }, false);
addEventListener("unload", function() { gBrowserInit.nonBrowserWindowShutdown() }, false);
