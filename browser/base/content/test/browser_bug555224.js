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
 * The Original Code is Browser Test Code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ryan Flint <rflint@mozilla.com>
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
const TEST_PAGE = "/browser/browser/base/content/test/dummy_page.html";
var gTestTab, gBgTab, gTestZoom;

function afterZoomAndLoad(aCallback, aTab) {
  let didLoad = false;
  let didZoom = false;
  aTab.linkedBrowser.addEventListener("load", function() {
    aTab.linkedBrowser.removeEventListener("load", arguments.callee, true);
    didLoad = true;
    if (didZoom)
      executeSoon(aCallback);
  }, true);
  let oldAPTS = FullZoom._applyPrefToSetting;
  FullZoom._applyPrefToSetting = function(value, browser) {
    if (!value)
      value = undefined;
    oldAPTS.call(FullZoom, value, browser);
    // Don't reset _applyPrefToSetting until we've seen the about:blank load(s)
    if (browser && (browser.currentURI.spec.indexOf("http:") != -1)) {
      FullZoom._applyPrefToSetting = oldAPTS;
      didZoom = true;
    }
    if (didLoad && didZoom)
      executeSoon(aCallback);
  };
}

function testBackgroundLoad() {
  is(ZoomManager.zoom, gTestZoom, "opening a background tab should not change foreground zoom");

  gBrowser.removeTab(gBgTab);

  FullZoom.reset();
  gBrowser.removeTab(gTestTab);

  finish();
}

function testInitialZoom() {
  is(ZoomManager.zoom, 1, "initial zoom level should be 1");
  FullZoom.enlarge();

  gTestZoom = ZoomManager.zoom;
  isnot(gTestZoom, 1, "zoom level should have changed");

  afterZoomAndLoad(testBackgroundLoad,
                   gBgTab = gBrowser.loadOneTab("http://mochi.test:8888" + TEST_PAGE,
                                                {inBackground: true}));
}

function test() {
  waitForExplicitFinish();

  gTestTab = gBrowser.addTab();
  gBrowser.selectedTab = gTestTab;

  afterZoomAndLoad(testInitialZoom, gTestTab);
  content.location = "http://example.org" + TEST_PAGE;
}
