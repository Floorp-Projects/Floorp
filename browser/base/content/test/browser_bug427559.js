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
 * Test bug 427559 to make sure focused elements that are no longer on the page
 * will have focus transferred to the window when changing tabs back to that
 * tab with the now-gone element.
 */

// Default focus on a button and have it kill itself on blur
let testPage = 'data:text/html,<body><button onblur="this.parentNode.removeChild(this);"><script>document.body.firstChild.focus();</script></body>';

function test() {
  waitForExplicitFinish();

  // Prepare the test tab
  let testTab = gBrowser.addTab();
  gBrowser.selectedTab = testTab;
  let testBrowser = gBrowser.getBrowserForTab(testTab);

  // Do stuff just after the page loads, so the page script can do its stuff
  testBrowser.addEventListener("load", function() setTimeout(function() {
    // The test page loaded, so open an empty tab, select it, then restore
    // the test tab. This causes the test page's focused element to be removed
    // from its document.
    let emptyTab = gBrowser.addTab();
    gBrowser.selectedTab = emptyTab;
    gBrowser.removeCurrentTab();
    gBrowser.selectedTab = testTab;

    // Make sure focus is given to the window because the element is now gone
    is(document.commandDispatcher.focusedWindow, window.content,
       "content window is focused");
    gBrowser.removeCurrentTab();
    finish();
  }, 0), true);

  // Start the test by loading the test page
  testBrowser.contentWindow.location = testPage;
}
