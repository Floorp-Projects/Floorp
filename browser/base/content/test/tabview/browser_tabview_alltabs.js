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
 * The Original Code is a test for bug 595395.
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

Cu.import("resource:///modules/tabview/AllTabs.jsm");

function test() {
  waitForExplicitFinish();

  let newTab = gBrowser.addTab();

  // TabPinned
  let pinned = function(tab) {
    is(tab, newTab, "The tabs are the same after the tab is pinned");
    ok(tab.pinned, "The tab gets pinned");

    gBrowser.unpinTab(tab);
  };

  // TabUnpinned
  let unpinned = function(tab) {
    AllTabs.unregister("pinned", pinned);
    AllTabs.unregister("unpinned", unpinned);

    is(tab, newTab, "The tabs are the same after the tab is unpinned");
    ok(!tab.pinned, "The tab gets unpinned");

    // clean up and finish
    gBrowser.removeTab(tab);
    finish();
  };

  AllTabs.register("pinned", pinned);
  AllTabs.register("unpinned", unpinned);

  ok(!newTab.pinned, "The tab is not pinned");
  gBrowser.pinTab(newTab);
}

