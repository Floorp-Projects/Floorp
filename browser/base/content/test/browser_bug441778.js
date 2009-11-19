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
 * The Original Code is Firefox Browser Test Code.
 *
 * The Initial Developer of the Original Code is
 * Edward Lee <edward.lee@engineering.uiuc.edu>.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Myk Melez <myk@mozilla.org>
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

/*
 * Test the fix for bug 441778 to ensure site-specific page zoom doesn't get
 * modified by sub-document loads of content from a different domain.
 */

function test() {
  waitForExplicitFinish();

  const TEST_PAGE_URL = 'data:text/html,<body><iframe src=""></iframe></body>';
  const TEST_IFRAME_URL = "http://test2.example.org/";

  // Prepare the test tab
  gBrowser.selectedTab = gBrowser.addTab();
  let testBrowser = gBrowser.selectedBrowser;

  testBrowser.addEventListener("load", function () {
    testBrowser.removeEventListener("load", arguments.callee, true);

    // Change the zoom level and then save it so we can compare it to the level
    // after loading the sub-document.
    FullZoom.enlarge();
    var zoomLevel = ZoomManager.zoom;

    // Start the sub-document load.
    executeSoon(function () {
      testBrowser.addEventListener("load", function (e) {
        testBrowser.removeEventListener("load", arguments.callee, true);

        is(e.target.defaultView.location, TEST_IFRAME_URL, "got the load event for the iframe");
        is(ZoomManager.zoom, zoomLevel, "zoom is retained after sub-document load");

        gBrowser.removeCurrentTab();
        finish();
      }, true);
      content.document.querySelector("iframe").src = TEST_IFRAME_URL;
    });
  }, true);

  content.location = TEST_PAGE_URL;
}
