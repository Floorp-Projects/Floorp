/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  function tabAdded(event) {
    let tab = event.target;
    tabs.push(tab);
  }

  let tabs = [];

  let container = gBrowser.tabContainer;
  container.addEventListener("TabOpen", tabAdded, false);

  gBrowser.addTab("about:blank");
  BrowserSearch.loadSearch("mozilla", true);
  BrowserSearch.loadSearch("firefox", true);
  
  is(tabs[0], gBrowser.tabs[3], "blank tab has been pushed to the end");
  is(tabs[1], gBrowser.tabs[1], "first search tab opens next to the current tab");
  is(tabs[2], gBrowser.tabs[2], "second search tab opens next to the first search tab");

  container.removeEventListener("TabOpen", tabAdded, false);
  tabs.forEach(gBrowser.removeTab, gBrowser);
}
