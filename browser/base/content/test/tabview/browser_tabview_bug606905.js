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
 * The Original Code is a test for bug 606901.
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

  let newTabs = []
  // add enough tabs so the close buttons are hidden and then check the closebuttons attribute
  do {
    let newTab = gBrowser.addTab("about:blank", {skipAnimation: true});
    newTabs.push(newTab);
  } while (gBrowser.visibleTabs[0].getBoundingClientRect().width > gBrowser.tabContainer.mTabClipWidth)

  // a setTimeout() in addTab is used to trigger adjustTabstrip() so we need a delay here as well.
  executeSoon(function() {
    is(gBrowser.tabContainer.getAttribute("closebuttons"), "activetab", "Only show button on selected tab.");

    // move a tab to another group and check the closebuttons attribute
    TabView._initFrame(function() {
      TabView.moveTabTo(newTabs[newTabs.length - 1], null);
      ok(gBrowser.visibleTabs[0].getBoundingClientRect().width > gBrowser.tabContainer.mTabClipWidth, 
         "Tab width is bigger than tab clip width");
      is(gBrowser.tabContainer.getAttribute("closebuttons"), "alltabs", "Show button on all tabs.")

      // clean up and finish
      newTabs.forEach(function(tab) {
        gBrowser.removeTab(tab);
      });
      finish();
    });
  });
}
