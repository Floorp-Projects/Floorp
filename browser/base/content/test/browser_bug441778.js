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

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

/*
 * Test the fix for bug 441778 to ensure site-specific page zoom doesn't get
 * modified by sub-document loads of content from a different domain.
 */

let testPage = 'data:text/html,<body><iframe id="a" src=""></iframe></body>';

function test() {
  waitForExplicitFinish();

  // The zoom level before the sub-document load.  We set this in continueTest
  // and then compare it to the current zoom level in finishTest to make sure
  // it hasn't changed.
  let zoomLevel;

  // Prepare the test tab
  let testTab = gBrowser.addTab();
  gBrowser.selectedTab = testTab;
  let testBrowser = gBrowser.getBrowserForTab(testTab);

  let finishTest = function() {
    testBrowser.removeProgressListener(progressListener);
    is(ZoomManager.zoom, zoomLevel, "zoom is retained after sub-document load");
    gBrowser.removeCurrentTab();
    finish();
  };

  let progressListener = {
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIWebProgressListener,
                                           Ci.nsISupportsWeakReference]),
    onStateChange: function() {},
    onProgressChange: function() {},
    onLocationChange: function() {
      window.setTimeout(finishTest, 0);
    },
    onStatusChange: function() {},
    onSecurityChange: function() {}
  };

  let continueTest = function() {
    // Change the zoom level and then save it so we can compare it to the level
    // after loading the sub-document.
    FullZoom.enlarge();
    zoomLevel = ZoomManager.zoom;

    // Finish the test in a timeout after the sub-document location change
    // to give the zoom controller time to respond to it.
    testBrowser.addProgressListener(progressListener);

    // Start the sub-document load.
    content.document.getElementById("a").src = "http://test2.example.org/";
  };

  // Continue the test after the test page has loaded.
  // Note: in order for the sub-document load to trigger a location change
  // the way it does under real world usage scenarios, we have to continue
  // the test in a timeout for some unknown reason.
  let continueListener = function() {
    window.setTimeout(continueTest, 0);
    
    // Remove the load listener so it doesn't get called for the sub-document.
    testBrowser.removeEventListener("load", continueListener, true);
  };
  testBrowser.addEventListener("load", continueListener, true);

  // Start the test by loading the test page.
  testBrowser.contentWindow.location = testPage;
}
