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
 * The Original Code is browser test code.
 *
 * The Initial Developer of the Original Code is
 * Simon Bünzli <zeniko@gmail.com>.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   John Morkel <jmorkel@gmail.com>
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
  // Add several new tabs in sequence, interrupted by selecting a
  // different tab, moving a tab around and closing a tab,
  // returning a list of opened tabs for verifying the expected order.
  // The new tab behaviour is documented in bug 465673
  function tabOpenDance() {
    let tabs = [];
    function addTab(aURL,aReferrer)
      tabs.push(gBrowser.addTab(aURL, aReferrer));

    addTab("http://localhost:8888/#0");
    gBrowser.selectedTab = tabs[0];
    addTab("http://localhost:8888/#1");
    addTab("http://localhost:8888/#2",gBrowser.currentURI);
    addTab("http://localhost:8888/#3",gBrowser.currentURI);
    gBrowser.selectedTab = tabs[tabs.length - 1];
    gBrowser.selectedTab = tabs[0];
    addTab("http://localhost:8888/#4",gBrowser.currentURI);
    gBrowser.selectedTab = tabs[3];
    addTab("http://localhost:8888/#5",gBrowser.currentURI);
    gBrowser.removeTab(tabs.pop());
    addTab("about:blank",gBrowser.currentURI);
    gBrowser.moveTabTo(gBrowser.selectedTab, 1);
    addTab("http://localhost:8888/#6",gBrowser.currentURI);
    addTab();
    addTab("http://localhost:8888/#7");

    return tabs;
  }

  function cleanUp(aTabs)
    aTabs.forEach(gBrowser.removeTab, gBrowser);

  let tabs = tabOpenDance();

  is(tabs[0], gBrowser.mTabs[3], "tab without referrer was opened to the far right");
  is(tabs[1], gBrowser.mTabs[7], "tab without referrer was opened to the far right");
  is(tabs[2], gBrowser.mTabs[5], "tab with referrer opened immediately to the right");
  is(tabs[3], gBrowser.mTabs[1], "next tab with referrer opened further to the right");
  is(tabs[4], gBrowser.mTabs[4], "tab selection changed, tab opens immediately to the right");
  is(tabs[5], gBrowser.mTabs[6], "blank tab with referrer opens to the right of 3rd original tab where removed tab was");
  is(tabs[6], gBrowser.mTabs[2], "tab has moved, new tab opens immediately to the right"); 
  is(tabs[7], gBrowser.mTabs[8], "blank tab without referrer opens at the end"); 
  is(tabs[8], gBrowser.mTabs[9], "tab without referrer opens at the end"); 

  cleanUp(tabs);
}
