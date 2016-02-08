/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test makes sure that private browsing turns off doesn't cause zoom
// settings to be reset on tab switch (bug 464962)

add_task(function* test() {
  let win = (yield BrowserTestUtils.openNewBrowserWindow({ private: true }));
  let tabAbout = (yield BrowserTestUtils.openNewForegroundTab(win.gBrowser, "about:"));
  let tabMozilla = (yield BrowserTestUtils.openNewForegroundTab(win.gBrowser, "about:"));

  let mozillaZoom = win.ZoomManager.zoom;

  // change the zoom on the mozilla page
  win.FullZoom.enlarge();
  // make sure the zoom level has been changed
  isnot(win.ZoomManager.zoom, mozillaZoom, "Zoom level can be changed");
  mozillaZoom = win.ZoomManager.zoom;

  // switch to about: tab
  yield BrowserTestUtils.switchTab(win.gBrowser, tabAbout);

  // switch back to mozilla tab
  yield BrowserTestUtils.switchTab(win.gBrowser, tabMozilla);

  // make sure the zoom level has not changed
  is(win.ZoomManager.zoom, mozillaZoom,
     "Entering private browsing should not reset the zoom on a tab");

  // cleanup
  win.FullZoom.reset();
  yield Promise.all([ BrowserTestUtils.removeTab(tabMozilla),
                      BrowserTestUtils.removeTab(tabAbout) ]);

  yield BrowserTestUtils.closeWindow(win);
});
