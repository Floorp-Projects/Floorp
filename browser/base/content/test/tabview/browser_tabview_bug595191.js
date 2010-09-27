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
 * The Original Code is tabview search test.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Sean Dunn <seanedunn@yahoo.com>
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

function test() {
  waitForExplicitFinish();

  // show the tab view
  window.addEventListener("tabviewshown", onTabViewWindowLoaded, false);
  ok(!TabView.isVisible(), "Tab View is hidden");
  TabView.toggle();
}

function onTabViewWindowLoaded() {
  window.removeEventListener("tabviewshown", onTabViewWindowLoaded, false);

  ok(TabView.isVisible(), "Tab View is visible");

  let contentWindow = document.getElementById("tab-view").contentWindow;
  let searchButton = contentWindow.document.getElementById("searchbutton");

  ok(searchButton, "Search button exists");
  
  let onSearchEnabled = function() {
    let search = contentWindow.document.getElementById("search");
    ok(search.style.display != "none", "Search is enabled");
    contentWindow.removeEventListener(
      "tabviewsearchenabled", onSearchEnabled, false);
    escapeTest(contentWindow);
  }
  contentWindow.addEventListener("tabviewsearchenabled", onSearchEnabled, 
    false);
  // enter search mode
  EventUtils.sendMouseEvent({ type: "mousedown" }, searchButton, 
    contentWindow);
}

function escapeTest(contentWindow) {  
  let onSearchDisabled = function() {
    let search = contentWindow.document.getElementById("search");

    ok(search.style.display == "none", "Search is disabled");

    contentWindow.removeEventListener(
      "tabviewsearchdisabled", onSearchDisabled, false);
    toggleTabViewTest(contentWindow);
  }
  contentWindow.addEventListener("tabviewsearchdisabled", onSearchDisabled, 
    false);
  // the search box focus()es in a function on the timeout queue, so we just
  // want to queue behind it.
  setTimeout( function() {
    EventUtils.synthesizeKey("VK_ESCAPE", {});
  }, 0);
}

function toggleTabViewTest(contentWindow) {
  let onTabViewHidden = function() {
    contentWindow.removeEventListener("tabviewhidden", onTabViewHidden, false);

    ok(!TabView.isVisible(), "Tab View is hidden");

    finish();
  }
  contentWindow.addEventListener("tabviewhidden", onTabViewHidden, false);
  // When search is hidden, it focus()es on the background, so avert the 
  // race condition by delaying ourselves on the timeout queue
  setTimeout( function() {
    // Use keyboard shortcut to toggle back to browser view
    EventUtils.synthesizeKey("e", { accelKey: true });
  }, 0);
}
