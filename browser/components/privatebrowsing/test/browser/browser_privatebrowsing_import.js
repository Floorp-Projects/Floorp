/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Private Browsing Tests.
 *
 * The Initial Developer of the Original Code is
 * Ehsan Akhgari.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ehsan Akhgari <ehsan.akhgari@gmail.com> (Original Author)
 *   Steffen Wilberg <steffen.wilberg@web.de>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
