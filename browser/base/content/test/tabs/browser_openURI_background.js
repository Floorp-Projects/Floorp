/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_task(async function () {
  const tabCount = gBrowser.tabs.length;
  const currentTab = gBrowser.selectedTab;

  const tests = [
    ["OPEN_NEWTAB", false],
    ["OPEN_NEWTAB_BACKGROUND", true],
  ];

  for (const [flag, isBackground] of tests) {
    window.browserDOMWindow.openURI(
      makeURI("about:blank"),
      null,
      Ci.nsIBrowserDOMWindow[flag],
      Ci.nsIBrowserDOMWindow.OPEN_NEW,
      Services.scriptSecurityManager.getSystemPrincipal()
    );

    is(gBrowser.tabs.length, tabCount + 1, `${flag} opens a new tab`);

    const openedTab = gBrowser.tabs[tabCount];

    if (isBackground) {
      is(
        gBrowser.selectedTab,
        currentTab,
        `${flag} opens a new background tab`
      );
    } else {
      is(gBrowser.selectedTab, openedTab, `${flag} opens a new foreground tab`);
    }

    gBrowser.removeTab(openedTab);
  }
});
