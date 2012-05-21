/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  /** Test for Bug 485482 **/
  
  waitForExplicitFinish();
  
  let uniqueValue = Math.random();
  
  let rootDir = getRootDirectory(gTestPath);
  let testURL = rootDir + "browser_485482_sample.html";
  let tab = gBrowser.addTab(testURL);
  tab.linkedBrowser.addEventListener("load", function(aEvent) {
    tab.linkedBrowser.removeEventListener("load", arguments.callee, true);
    let doc = tab.linkedBrowser.contentDocument;
    doc.querySelector("input[type=text]").value = uniqueValue;
    doc.querySelector("input[type=checkbox]").checked = true;
    
    let tab2 = gBrowser.duplicateTab(tab);
    tab2.linkedBrowser.addEventListener("load", function(aEvent) {
      tab2.linkedBrowser.removeEventListener("load", arguments.callee, true);
      doc = tab2.linkedBrowser.contentDocument;
      is(doc.querySelector("input[type=text]").value, uniqueValue,
         "generated XPath expression was valid");
      ok(doc.querySelector("input[type=checkbox]").checked,
         "generated XPath expression was valid");
      
      // clean up
      gBrowser.removeTab(tab2);
      gBrowser.removeTab(tab);
      finish();
    }, true);
  }, true);
}
