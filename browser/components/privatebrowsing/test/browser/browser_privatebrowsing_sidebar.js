/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test makes sure that Sidebars do not migrate across windows with
// different privacy states

// See Bug 885054: https://bugzilla.mozilla.org/show_bug.cgi?id=885054

function test() {
  waitForExplicitFinish();

  // opens a sidebar
  function openSidebar(win) {
    return win.SidebarUI.show("viewBookmarksSidebar").then(() => win);
  }

  let windowCache = [];
  function cacheWindow(w) {
    windowCache.push(w);
    return w;
  }
  function closeCachedWindows() {
    windowCache.forEach(w => w.close());
  }

  // Part 1: NON PRIVATE WINDOW -> PRIVATE WINDOW
  openWindow(window, {}, 1)
    .then(cacheWindow)
    .then(openSidebar)
    .then(win => openWindow(win, { private: true }))
    .then(cacheWindow)
    .then(function({ document }) {
      let sidebarBox = document.getElementById("sidebar-box");
      is(
        sidebarBox.hidden,
        true,
        "Opening a private window from reg window does not open the sidebar"
      );
    })
    .then(closeCachedWindows)
    // Part 2: NON PRIVATE WINDOW -> NON PRIVATE WINDOW
    .then(() => openWindow(window))
    .then(cacheWindow)
    .then(openSidebar)
    .then(win => openWindow(win))
    .then(cacheWindow)
    .then(function({ document }) {
      let sidebarBox = document.getElementById("sidebar-box");
      is(
        sidebarBox.hidden,
        false,
        "Opening a reg window from reg window does open the sidebar"
      );
    })
    .then(closeCachedWindows)
    // Part 3: PRIVATE WINDOW -> NON PRIVATE WINDOW
    .then(() => openWindow(window, { private: true }))
    .then(cacheWindow)
    .then(openSidebar)
    .then(win => openWindow(win))
    .then(cacheWindow)
    .then(function({ document }) {
      let sidebarBox = document.getElementById("sidebar-box");
      is(
        sidebarBox.hidden,
        true,
        "Opening a reg window from a private window does not open the sidebar"
      );
    })
    .then(closeCachedWindows)
    // Part 4: PRIVATE WINDOW -> PRIVATE WINDOW
    .then(() => openWindow(window, { private: true }))
    .then(cacheWindow)
    .then(openSidebar)
    .then(win => openWindow(win, { private: true }))
    .then(cacheWindow)
    .then(function({ document }) {
      let sidebarBox = document.getElementById("sidebar-box");
      is(
        sidebarBox.hidden,
        false,
        "Opening a private window from private window does open the sidebar"
      );
    })
    .then(closeCachedWindows)
    .then(finish);
}
