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
 * The Original Code is Tab View first run (Bug 593157) test.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Michael Yoshitaka Erlewine <mitcho@mitcho.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

var prefsBranch = Cc["@mozilla.org/preferences-service;1"].
                  getService(Ci.nsIPrefService).
                  getBranch("browser.panorama.");

function test() {
  waitForExplicitFinish();

  ok(!TabView.isVisible(), "Main window TabView is hidden");

  ok(experienced(), "should start as experienced");

  prefsBranch.setBoolPref("experienced_first_run", false);
  ok(!experienced(), "set to not experienced");

  newWindowWithTabView(checkFirstRun, part2);
}

function experienced() {
  return prefsBranch.prefHasUserValue("experienced_first_run") &&
    prefsBranch.getBoolPref("experienced_first_run");
}

function checkFirstRun(win) {
  let contentWindow = win.document.getElementById("tab-view").contentWindow;
  
  is(win.gBrowser.tabs.length, 2, "There should be two tabs");
  
  let groupItems = contentWindow.GroupItems.groupItems;
  is(groupItems.length, 1, "There should be one group");
  is(groupItems[0].getChildren().length, 1, "...with one child");

  let orphanTabCount = contentWindow.GroupItems.getOrphanedTabs().length;
  is(orphanTabCount, 1, "There should also be an orphaned tab");
  
  ok(experienced(), "we're now experienced");
}

function part2() {
  newWindowWithTabView(checkNotFirstRun, endGame);
}

function checkNotFirstRun(win) {
  let contentWindow = win.document.getElementById("tab-view").contentWindow;
  
  is(win.gBrowser.tabs.length, 1, "There should be one tab");
  
  let groupItems = contentWindow.GroupItems.groupItems;
  is(groupItems.length, 1, "There should be one group");
  is(groupItems[0].getChildren().length, 1, "...with one child");

  let orphanTabCount = contentWindow.GroupItems.getOrphanedTabs().length;
  is(orphanTabCount, 0, "There should also be no orphaned tabs");
}

function endGame() {
  ok(!TabView.isVisible(), "Main window TabView is still hidden");
  ok(experienced(), "should finish as experienced");
  finish();
}

function newWindowWithTabView(callback, completeCallback) {
  let charsetArg = "charset=" + window.content.document.characterSet;
  let win = window.openDialog(getBrowserURL(), "_blank", "chrome,all,dialog=no,height=800,width=800",
                              "about:blank", charsetArg, null, null, true);
  let onLoad = function() {
    win.removeEventListener("load", onLoad, false);
    let onShown = function() {
      win.removeEventListener("tabviewshown", onShown, false);
      callback(win);
      win.close();
      if (typeof completeCallback == "function")
        completeCallback();
    };
    win.addEventListener("tabviewshown", onShown, false);
    win.TabView.toggle();
  }
  win.addEventListener("load", onLoad, false);
}
