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
 * The Original Code is bug 589324 test.
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
  const DUMMY_PAGE_URL = "http://mochi.test:8888/browser/browser/base/content/test/tabview/dummy_page.html";
  const DUMMY_PAGE_URL_2 = "http://mochi.test:8888/";

  let ss = Cc["@mozilla.org/browser/sessionstore;1"].getService(Ci.nsISessionStore);
  waitForExplicitFinish();

  // open a new window and setup the window state.
  let newWin = openDialog(getBrowserURL(), "_blank", "chrome,all,dialog=no");
  newWin.addEventListener("load", function(event) {
    this.removeEventListener("load", arguments.callee, false);

    let newState = {
      windows: [{
        tabs: [{
          entries: [{ url: DUMMY_PAGE_URL }],
          hidden: true,
          attributes: {},
          extData: {
            "tabview-tab":
              '{"bounds":{"left":21,"top":29,"width":204,"height":153},' +
              '"userSize":null,"url":"' + DUMMY_PAGE_URL + '","groupID":1,' + 
              '"imageData":null,"title":null}'
          }
        },{
          entries: [{ url: DUMMY_PAGE_URL_2 }],
          hidden: false,
          attributes: {},
          extData: {
            "tabview-tab": 
              '{"bounds":{"left":315,"top":29,"width":111,"height":84},' + 
              '"userSize":null,"url":"' + DUMMY_PAGE_URL_2 + '","groupID":2,' + 
              '"imageData":null,"title":null}'
          },
        }],
        selected:2,
        _closedTabs: [],
        extData: {
          "tabview-groups": '{"nextID":3,"activeGroupId":2}',
          "tabview-group": 
            '{"1":{"bounds":{"left":15,"top":5,"width":280,"height":232},' + 
            '"userSize":null,"locked":{},"title":"","id":1},' + 
            '"2":{"bounds":{"left":309,"top":5,"width":267,"height":226},' + 
            '"userSize":null,"locked":{},"title":"","id":2}}',
          "tabview-ui": '{"pageBounds":{"left":0,"top":0,"width":788,"height":548}}'
        }, sizemode:"normal"
      }]
    };
    ss.setWindowState(newWin, JSON.stringify(newState), true);

    let firstTab = newWin.gBrowser.tabs[0];
    let secondTab = newWin.gBrowser.tabs[1];

    // wait until the first tab is fully loaded
    let browser = newWin.gBrowser.getBrowserForTab(firstTab);
    let onLoad = function() {
      browser.removeEventListener("load", onLoad, true);

      is(browser.currentURI.spec, DUMMY_PAGE_URL, 
         "The url of first tab url is dummy_page.html");

      // check the hidden state of both tabs.
      ok(firstTab.hidden, "The first tab is hidden");
      ok(!secondTab.hidden, "The second tab is not hidden");
      is(secondTab, newWin.gBrowser.selectedTab, "The second tab is selected");

      // when the second tab is hidden, the iframe should be initialized and 
      // the first tab should be visible.
      let onTabHide = function() {
        newWin.gBrowser.tabContainer.addEventListener("TabHide", onTabHide, true);

        ok(newWin.TabView.getContentWindow(), "");

        ok(!firstTab.hidden, "The first tab is not hidden");
        is(firstTab, newWin.gBrowser.selectedTab, "The first tab is selected");
        ok(secondTab.hidden, "The second tab is hidden");

        // clean up and finish
        newWin.close();

        finish();
      };
      newWin.gBrowser.tabContainer.addEventListener("TabHide", onTabHide, true);

      // switch to another tab
      newWin.switchToTabHavingURI(DUMMY_PAGE_URL);
    }
    browser.addEventListener("load", onLoad, true);
  }, false);
}
