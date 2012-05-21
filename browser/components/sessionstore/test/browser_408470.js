/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  /** Test for Bug 408470 **/
  
  waitForExplicitFinish();
  
  let pendingCount = 1;
  let rootDir = getRootDirectory(gTestPath);
  let testUrl = rootDir + "browser_408470_sample.html";
  let tab = gBrowser.addTab(testUrl);
  
  tab.linkedBrowser.addEventListener("load", function(aEvent) {
    tab.linkedBrowser.removeEventListener("load", arguments.callee, true);
    // enable all stylesheets and verify that they're correctly persisted
    Array.forEach(tab.linkedBrowser.contentDocument.styleSheets, function(aSS, aIx) {
      pendingCount++;
      let ssTitle = aSS.title;
      gPageStyleMenu.switchStyleSheet(ssTitle, tab.linkedBrowser.contentWindow);

      let newTab = gBrowser.duplicateTab(tab);
      newTab.linkedBrowser.addEventListener("load", function(aEvent) {
        newTab.linkedBrowser.removeEventListener("load", arguments.callee, true);
        let states = Array.map(newTab.linkedBrowser.contentDocument.styleSheets,
                               function(aSS) !aSS.disabled);
        let correct = states.indexOf(true) == aIx && states.indexOf(true, aIx + 1) == -1;
        
        if (/^fail_/.test(ssTitle))
          ok(!correct, "didn't restore stylesheet " + ssTitle);
        else
          ok(correct, "restored stylesheet " + ssTitle);
        
        gBrowser.removeTab(newTab);
        if (--pendingCount == 0)
          finish();
      }, true);
    });
    
    // disable all styles and verify that this is correctly persisted
    tab.linkedBrowser.markupDocumentViewer.authorStyleDisabled = true;
    let newTab = gBrowser.duplicateTab(tab);
    newTab.linkedBrowser.addEventListener("load", function(aEvent) {
      newTab.linkedBrowser.removeEventListener("load", arguments.callee, true);
      is(newTab.linkedBrowser.markupDocumentViewer.authorStyleDisabled, true,
         "disabled all stylesheets");
      
      gBrowser.removeTab(newTab);
      if (--pendingCount == 0)
        finish();
    }, true);
    
    gBrowser.removeTab(tab);
  }, true);
}
