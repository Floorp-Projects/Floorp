/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test makes sure that Sidebars do not migrate across windows with
// different privacy states

// See Bug 885054: https://bugzilla.mozilla.org/show_bug.cgi?id=885054

function test() {
  waitForExplicitFinish();

  let { utils: Cu } = Components;

  let { Promise: { defer } } = Cu.import("resource://gre/modules/Promise.jsm", {});

  // opens a sidebar
  function openSidebar(win) {
    let { promise, resolve } = defer();
    let doc = win.document;

    let sidebarID = 'viewBookmarksSidebar';

    let sidebar = doc.getElementById('sidebar');

    let sidebarurl = doc.getElementById(sidebarID).getAttribute('sidebarurl');

    sidebar.addEventListener('load', function onSidebarLoad() {
      if (sidebar.contentWindow.location.href != sidebarurl)
        return;
      sidebar.removeEventListener('load', onSidebarLoad, true);

      resolve(win);
    }, true);

    win.toggleSidebar(sidebarID, true);

    return promise;
  }

  let windowCache = [];
  function cacheWindow(w) {
    windowCache.push(w);
    return w;
  }
  function closeCachedWindows () {
    windowCache.forEach(function(w) w.close());
  }

  // Part 1: NON PRIVATE WINDOW -> PRIVATE WINDOW
  openWindow(window, {}, 1).
    then(cacheWindow).
    then(openSidebar).
    then(function(win) openWindow(win, { private: true })).
    then(cacheWindow).
    then(function({ document }) {
      let sidebarBox = document.getElementById("sidebar-box");
      is(sidebarBox.hidden, true, 'Opening a private window from reg window does not open the sidebar');
    }).
    // Part 2: NON PRIVATE WINDOW -> NON PRIVATE WINDOW
    then(function() openWindow(window)).
    then(cacheWindow).
    then(openSidebar).
    then(function(win) openWindow(win)).
    then(cacheWindow).
    then(function({ document }) {
      let sidebarBox = document.getElementById("sidebar-box");
      is(sidebarBox.hidden, false, 'Opening a reg window from reg window does open the sidebar');
    }).
    // Part 3: PRIVATE WINDOW -> NON PRIVATE WINDOW
    then(function() openWindow(window, { private: true })).
    then(cacheWindow).
    then(openSidebar).
    then(function(win) openWindow(win)).
    then(cacheWindow).
    then(function({ document }) {
      let sidebarBox = document.getElementById("sidebar-box");
      is(sidebarBox.hidden, true, 'Opening a reg window from a private window does not open the sidebar');
    }).
    // Part 4: PRIVATE WINDOW -> PRIVATE WINDOW
    then(function() openWindow(window, { private: true })).
    then(cacheWindow).
    then(openSidebar).
    then(function(win) openWindow(win, { private: true })).
    then(cacheWindow).
    then(function({ document }) {
      let sidebarBox = document.getElementById("sidebar-box");
      is(sidebarBox.hidden, false, 'Opening a private window from private window does open the sidebar');
    }).
    then(closeCachedWindows).
    then(finish);
}
