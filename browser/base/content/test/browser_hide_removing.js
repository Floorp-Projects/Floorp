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
 * The Original Code is tabbrowser hide removing tab test.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Edward Lee <edilee@mozilla.com>
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

// Bug 587922: tabs don't get removed if they're hidden

function test() {
  waitForExplicitFinish();

  // Add a tab that will get removed and hidden
  let testTab = gBrowser.addTab("about:blank", {skipAnimation: true});
  is(gBrowser.visibleTabs.length, 2, "just added a tab, so 2 tabs");
  gBrowser.selectedTab = testTab;

  let numVisBeforeHide, numVisAfterHide;
  gBrowser.tabContainer.addEventListener("TabSelect", function() {
    gBrowser.tabContainer.removeEventListener("TabSelect", arguments.callee, false);

    // While the next tab is being selected, hide the removing tab
    numVisBeforeHide = gBrowser.visibleTabs.length;
    gBrowser.hideTab(testTab);
    numVisAfterHide = gBrowser.visibleTabs.length;
  }, false);
  gBrowser.removeTab(testTab, {animate: true});

  // Make sure the tab gets removed at the end of the animation by polling
  (function checkRemoved() setTimeout(function() {
    if (gBrowser.tabs.length != 1)
      return checkRemoved();

    is(numVisBeforeHide, 1, "animated remove has in 1 tab left");
    is(numVisAfterHide, 1, "hiding a removing tab is also has 1 tab");
    finish();
  }, 50))();
}
