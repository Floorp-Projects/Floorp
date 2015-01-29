/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  /** Test for Bug 339445 **/

  waitForExplicitFinish();

  let testURL = "http://mochi.test:8888/browser/" +
    "browser/components/sessionstore/test/browser_339445_sample.html";

  let tab = gBrowser.addTab(testURL);
  promiseBrowserLoaded(tab.linkedBrowser).then(() => {
    let doc = tab.linkedBrowser.contentDocument;
    is(doc.getElementById("storageTestItem").textContent, "PENDING",
       "sessionStorage value has been set");

    let tab2 = gBrowser.duplicateTab(tab);
    promiseTabRestored(tab2).then(() => {
      let doc2 = tab2.linkedBrowser.contentDocument;
      is(doc2.getElementById("storageTestItem").textContent, "SUCCESS",
         "sessionStorage value has been duplicated");

      // clean up
      gBrowser.removeTab(tab2);
      gBrowser.removeTab(tab);

      finish();
    });
  });
}
