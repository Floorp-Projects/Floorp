/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  /** Test for Bug 454908 **/

  waitForExplicitFinish();

  let fieldValues = {
    username: "User " + Math.random(),
    passwd:   "pwd" + Date.now()
  };

  // make sure we do save form data
  gPrefService.setIntPref("browser.sessionstore.privacy_level", 0);

  let rootDir = getRootDirectory(gTestPath);
  let testURL = rootDir + "browser_454908_sample.html";
  let tab = gBrowser.addTab(testURL);
  whenBrowserLoaded(tab.linkedBrowser, function() {
    let doc = tab.linkedBrowser.contentDocument;
    for (let id in fieldValues)
      doc.getElementById(id).value = fieldValues[id];

    gBrowser.removeTab(tab);

    tab = undoCloseTab();
    whenTabRestored(tab, function() {
      let doc = tab.linkedBrowser.contentDocument;
      for (let id in fieldValues) {
        let node = doc.getElementById(id);
        if (node.type == "password")
          is(node.value, "", "password wasn't saved/restored");
        else
          is(node.value, fieldValues[id], "username was saved/restored");
      }

      // clean up
      if (gPrefService.prefHasUserValue("browser.sessionstore.privacy_level"))
        gPrefService.clearUserPref("browser.sessionstore.privacy_level");
      // undoCloseTab can reuse a single blank tab, so we have to
      // make sure not to close the window when closing our last tab
      if (gBrowser.tabs.length == 1)
        gBrowser.addTab();
      gBrowser.removeTab(tab);
      finish();
    });
  });
}
