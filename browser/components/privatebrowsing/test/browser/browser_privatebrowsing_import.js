/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test makes sure that the "Import and Backup->Import From Another Browser"
// menu item in the Places Organizer is disabled inside private browsing mode.

// TEST_PATH=browser/components/privatebrowsing/test/browser/browser_privatebrowsing_import.js make -C $(OBJDIR) mochitest-browser-chrome

let pb = Cc["@mozilla.org/privatebrowsing;1"].
         getService(Ci.nsIPrivateBrowsingService);

function test() {
  waitForExplicitFinish();
  gPrefService.setBoolPref("browser.privatebrowsing.keep_current_session", true);

  // first test: open the library with PB disabled
  pb.privateBrowsingEnabled = false;
  openLibrary(testPBoff);
}

function openLibrary(callback) {
  var library = window.openDialog("chrome://browser/content/places/places.xul",
                                  "", "chrome,toolbar=yes,dialog=no,resizable");
  waitForFocus(function () {
    callback(library);
  }, library);
}

function testPBoff(win) {
  // XXX want to test the #browserImport menuitem instead
  let importMenuItem = win.document.getElementById("OrganizerCommand_browserImport");

  // make sure the menu item is enabled outside PB mode when opening the Library
  ok(!importMenuItem.hasAttribute("disabled"),
    "Import From Another Browser menu item should be enabled outside PB mode when opening the Library");

  // enter private browsing mode
  pb.privateBrowsingEnabled = true;
  ok(importMenuItem.hasAttribute("disabled"),
    "Import From Another Browser menu item should be disabled after starting PB mode");

  // leave private browsing mode
  pb.privateBrowsingEnabled = false;
  ok(!importMenuItem.hasAttribute("disabled"),
    "Import From Another Browser menu item should not be disabled after leaving the PB mode");

  win.close();

  // launch the second test: open the Library with PB enabled
  pb.privateBrowsingEnabled = true;
  openLibrary(testPBon);
}

function testPBon(win) {
  let importMenuItem = win.document.getElementById("OrganizerCommand_browserImport");

  // make sure the menu item is disabled in PB mode when opening the Library
  ok(importMenuItem.hasAttribute("disabled"),
    "Import From Another Browser menu item should be disabled in PB mode when opening the Libary");

  // leave private browsing mode
  pb.privateBrowsingEnabled = false;
  ok(!importMenuItem.hasAttribute("disabled"),
    "Import From Another Browser menu item should not be disabled after leaving PB mode");

  win.close();

  // cleanup
  gPrefService.clearUserPref("browser.privatebrowsing.keep_current_session");
  finish();
}
