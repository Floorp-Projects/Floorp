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
 * The Original Code is tabview bug600645 test.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Raymond Lee <raymond@appcoast.com>
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

let newTab;

function test() {
  waitForExplicitFinish();

  newTab = gBrowser.addTab();
  gBrowser.pinTab(newTab);

  window.addEventListener("tabviewshown", onTabViewWindowLoaded, false);
  TabView.toggle();
}

function onTabViewWindowLoaded() {
  window.removeEventListener("tabviewshown", onTabViewWindowLoaded, false);

  let contentWindow = document.getElementById("tab-view").contentWindow;
  is(contentWindow.GroupItems.groupItems.length, 1, 
     "There is one group item on startup");

  let groupItem = contentWindow.GroupItems.groupItems[0];
  let icon = contentWindow.iQ(".appTabIcon", groupItem.$appTabTray)[0];
  let $icon = contentWindow.iQ(icon);

  is($icon.data("xulTab"), newTab, 
     "The app tab icon has the right tab reference")
  // check to see whether it's showing the default one or not.
  is($icon.attr("src"), contentWindow.Utils.defaultFaviconURL, 
     "The icon is showing the default fav icon for blank tab");

  let errorHandler = function(event) {
    newTab.removeEventListener("error", errorHandler, false);

    // since the browser code and test code are invoked when an error event is 
    // fired, a delay is used here to avoid the test code run before the browser 
    // code.
    executeSoon(function() {
      is($icon.attr("src"), contentWindow.Utils.defaultFaviconURL, 
         "The icon is showing th default fav icon");

      // clean up
      gBrowser.removeTab(newTab);
      let endGame = function() {
        window.removeEventListener("tabviewhidden", endGame, false);

        ok(!TabView.isVisible(), "Tab View is hidden");
        finish();
      }
      window.addEventListener("tabviewhidden", endGame, false);
      TabView.toggle();
    });
  };
  newTab.addEventListener("error", errorHandler, false);

  newTab.linkedBrowser.loadURI(
    "http://mochi.test:8888/browser/browser/base/content/test/tabview/test_bug600645.html");
}
