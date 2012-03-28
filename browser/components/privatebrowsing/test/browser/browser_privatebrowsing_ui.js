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

// This test makes sure that the gPrivateBrowsingUI object, the Private Browsing
// menu item and its XUL <command> element work correctly.

function test() {
  // initialization
  gPrefService.setBoolPref("browser.privatebrowsing.keep_current_session", true);
  let pb = Cc["@mozilla.org/privatebrowsing;1"].
           getService(Ci.nsIPrivateBrowsingService);
  let observerData;
  function observer(aSubject, aTopic, aData) {
    if (aTopic == "private-browsing")
      observerData = aData;
  }
  Services.obs.addObserver(observer, "private-browsing", false);
  let pbMenuItem = document.getElementById("privateBrowsingItem");
  // add a new blank tab to ensure the title can be meaningfully compared later
  gBrowser.selectedTab = gBrowser.addTab();
  let originalTitle = document.title;

  // test the gPrivateBrowsingUI object
  ok(gPrivateBrowsingUI, "The gPrivateBrowsingUI object exists");
  is(pb.privateBrowsingEnabled, false, "The private browsing mode should not be started initially");
  is(gPrivateBrowsingUI.privateBrowsingEnabled, false, "gPrivateBrowsingUI should expose the correct private browsing status");
  is(gPrivateBrowsingUI.privateWindow, false, "gPrivateBrowsingUI should expose the correct per-window private browsing status");
  ok(pbMenuItem, "The Private Browsing menu item exists");
  is(pbMenuItem.getAttribute("label"), pbMenuItem.getAttribute("startlabel"), "The Private Browsing menu item should read \"Start Private Browsing\"");
  gPrivateBrowsingUI.toggleMode();
  is(pb.privateBrowsingEnabled, true, "The private browsing mode should be started");
  is(gPrivateBrowsingUI.privateBrowsingEnabled, true, "gPrivateBrowsingUI should expose the correct private browsing status");
  is(gPrivateBrowsingUI.privateWindow, true, "gPrivateBrowsingUI should expose the correct per-window private browsing status");
  // check to see if the Private Browsing mode was activated successfully
  is(observerData, "enter", "Private Browsing mode was activated using the gPrivateBrowsingUI object");
  is(pbMenuItem.getAttribute("label"), pbMenuItem.getAttribute("stoplabel"), "The Private Browsing menu item should read \"Stop Private Browsing\"");
  gPrivateBrowsingUI.toggleMode()
  is(pb.privateBrowsingEnabled, false, "The private browsing mode should not be started");
  is(gPrivateBrowsingUI.privateBrowsingEnabled, false, "gPrivateBrowsingUI should expose the correct private browsing status");
  is(gPrivateBrowsingUI.privateWindow, false, "gPrivateBrowsingUI should expose the correct per-window private browsing status");
  // check to see if the Private Browsing mode was deactivated successfully
  is(observerData, "exit", "Private Browsing mode was deactivated using the gPrivateBrowsingUI object");
  is(pbMenuItem.getAttribute("label"), pbMenuItem.getAttribute("startlabel"), "The Private Browsing menu item should read \"Start Private Browsing\"");

  // These are tests for the privateWindow setter.  Note that the setter should
  // not be used anywhere else for now!
  gPrivateBrowsingUI.privateWindow = true;
  is(gPrivateBrowsingUI.privateWindow, true, "gPrivateBrowsingUI should accept the correct per-window private browsing status");
  gPrivateBrowsingUI.privateWindow = false;
  is(gPrivateBrowsingUI.privateWindow, false, "gPrivateBrowsingUI should accept the correct per-window private browsing status");

  // now, test using the <command> object
  let cmd = document.getElementById("Tools:PrivateBrowsing");
  isnot(cmd, null, "XUL command object for the private browsing service exists");
  var func = new Function("", cmd.getAttribute("oncommand"));
  func.call(cmd);
  // check to see if the Private Browsing mode was activated successfully
  is(observerData, "enter", "Private Browsing mode was activated using the command object");
  // check to see that the window title has been changed correctly
  isnot(document.title, originalTitle, "Private browsing mode has correctly changed the title");
  func.call(cmd);
  // check to see if the Private Browsing mode was deactivated successfully
  is(observerData, "exit", "Private Browsing mode was deactivated using the command object");
  // check to see that the window title has been restored correctly
  is(document.title, originalTitle, "Private browsing mode has correctly restored the title");

  // cleanup
  gBrowser.removeCurrentTab();
  Services.obs.removeObserver(observer, "private-browsing");
  gPrefService.clearUserPref("browser.privatebrowsing.keep_current_session");
}
