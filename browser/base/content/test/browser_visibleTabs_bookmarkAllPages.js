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
 * The Original Code is bookmark all pages test with tab view.
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

function test() {
  waitForExplicitFinish();

  let tabOne = gBrowser.addTab("about:blank");
  let tabTwo = gBrowser.addTab("http://mochi.test:8888/");
  gBrowser.selectedTab = tabTwo;

  let browser = gBrowser.getBrowserForTab(tabTwo);
  let onLoad = function() {
    browser.removeEventListener("load", onLoad, true);

    gBrowser.showOnlyTheseTabs([tabTwo]);

    is(gBrowser.visibleTabs.length, 1, "Only one tab is visible");

    let uris = PlacesCommandHook.uniqueCurrentPages;
    is(uris.length, 1, "Only one uri is returned");

    is(uris[0].spec, tabTwo.linkedBrowser.currentURI.spec, "It's the correct URI");

    gBrowser.removeTab(tabOne);
    gBrowser.removeTab(tabTwo);
    Array.forEach(gBrowser.tabs, function(tab) {
      gBrowser.showTab(tab);
    });

    finish();
  }
  browser.addEventListener("load", onLoad, true);
}
