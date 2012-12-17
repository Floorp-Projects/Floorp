/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  /** Test (B) for Bug 248970 **/
  waitForExplicitFinish();

  let windowsToClose = [];
  let file = Services.dirsvc.get("TmpD", Ci.nsIFile);
  let filePath = file.path;
  let fieldList = {
    "//input[@name='input']":     Date.now().toString(),
    "//input[@name='spaced 1']":  Math.random().toString(),
    "//input[3]":                 "three",
    "//input[@type='checkbox']":  true,
    "//input[@name='uncheck']":   false,
    "//input[@type='radio'][1]":  false,
    "//input[@type='radio'][2]":  true,
    "//input[@type='radio'][3]":  false,
    "//select":                   2,
    "//select[@multiple]":        [1, 3],
    "//textarea[1]":              "",
    "//textarea[2]":              "Some text... " + Math.random(),
    "//textarea[3]":              "Some more text\n" + new Date(),
    "//input[@type='file']":      filePath
  };

  registerCleanupFunction(function() {
    windowsToClose.forEach(function(win) {
      win.close();
    });
  });

  function test(aLambda) {
    try {
      return aLambda() || true;
    } catch(ex) { }
    return false;
  }

  function getElementByXPath(aTab, aQuery) {
    let doc = aTab.linkedBrowser.contentDocument;
    let xptype = Ci.nsIDOMXPathResult.FIRST_ORDERED_NODE_TYPE;
    return doc.evaluate(aQuery, doc, null, xptype, null).singleNodeValue;
  }

  function setFormValue(aTab, aQuery, aValue) {
    let node = getElementByXPath(aTab, aQuery);
    if (typeof aValue == "string")
      node.value = aValue;
    else if (typeof aValue == "boolean")
      node.checked = aValue;
    else if (typeof aValue == "number")
      node.selectedIndex = aValue;
    else
      Array.forEach(node.options, function(aOpt, aIx)
        (aOpt.selected = aValue.indexOf(aIx) > -1));
  }

  function compareFormValue(aTab, aQuery, aValue) {
    let node = getElementByXPath(aTab, aQuery);
    if (!node)
      return false;
    if (node instanceof Ci.nsIDOMHTMLInputElement)
      return aValue == (node.type == "checkbox" || node.type == "radio" ?
                       node.checked : node.value);
    if (node instanceof Ci.nsIDOMHTMLTextAreaElement)
      return aValue == node.value;
    if (!node.multiple)
      return aValue == node.selectedIndex;
    return Array.every(node.options, function(aOpt, aIx)
            (aValue.indexOf(aIx) > -1) == aOpt.selected);
  }

  //////////////////////////////////////////////////////////////////
  // Test (B) : Session data restoration between windows          //
  //////////////////////////////////////////////////////////////////

  let rootDir = getRootDirectory(gTestPath);
  const testURL = rootDir + "browser_248970_b_sample.html";
  const testURL2 = "http://mochi.test:8888/browser/" +
    "browser/components/sessionstore/test/browser_248970_b_sample.html";

  whenNewWindowLoaded(false, function(aWin) {
    windowsToClose.push(aWin);

    // get closed tab count
    let count = ss.getClosedTabCount(aWin);
    let max_tabs_undo =
      Services.prefs.getIntPref("browser.sessionstore.max_tabs_undo");
    ok(0 <= count && count <= max_tabs_undo,
      "getClosedTabCount should return zero or at most max_tabs_undo");

    // setup a state for tab (A) so we can check later that is restored
    let key = "key";
    let value = "Value " + Math.random();
    let state = { entries: [{ url: testURL }], extData: { key: value } };

    // public session, add new tab: (A)
    let tab_A = aWin.gBrowser.addTab(testURL);
    ss.setTabState(tab_A, JSON.stringify(state));
    whenBrowserLoaded(tab_A.linkedBrowser, function() {
      // make sure that the next closed tab will increase getClosedTabCount
      Services.prefs.setIntPref(
        "browser.sessionstore.max_tabs_undo", max_tabs_undo + 1)

      // populate tab_A with form data
      for (let i in fieldList)
        setFormValue(tab_A, i, fieldList[i]);

      // public session, close tab: (A)
      aWin.gBrowser.removeTab(tab_A);

      // verify that closedTabCount increased
      ok(ss.getClosedTabCount(aWin) > count,
         "getClosedTabCount has increased after closing a tab");

      // verify tab: (A), in undo list
      let tab_A_restored = test(function() ss.undoCloseTab(aWin, 0));
      ok(tab_A_restored, "a tab is in undo list");
      whenBrowserLoaded(tab_A_restored.linkedBrowser, function() {
        is(testURL, tab_A_restored.linkedBrowser.currentURI.spec,
           "it's the same tab that we expect");
        aWin.gBrowser.removeTab(tab_A_restored);

        whenNewWindowLoaded(true, function(aWin) {
          windowsToClose.push(aWin);

          // setup a state for tab (B) so we can check that its duplicated
          // properly
          let key1 = "key1";
          let value1 = "Value " + Math.random();
          let state1 = {
            entries: [{ url: testURL2 }], extData: { key1: value1 }
          };

          let tab_B = aWin.gBrowser.addTab(testURL2);
          ss.setTabState(tab_B, JSON.stringify(state1));
          whenBrowserLoaded(tab_B.linkedBrowser, function() {
            // populate tab: (B) with different form data
            for (let item in fieldList)
              setFormValue(tab_B, item, fieldList[item]);

            // duplicate tab: (B)
            let tab_C = aWin.gBrowser.duplicateTab(tab_B);
            whenBrowserLoaded(tab_C.linkedBrowser, function() {
              // verify the correctness of the duplicated tab
              is(ss.getTabValue(tab_C, key1), value1,
                "tab successfully duplicated - correct state");

              for (let item in fieldList)
                ok(compareFormValue(tab_C, item, fieldList[item]),
                  "The value for \"" + item + "\" was correctly duplicated");

              // private browsing session, close tab: (C) and (B)
              aWin.gBrowser.removeTab(tab_C);
              aWin.gBrowser.removeTab(tab_B);

              finish();
            });
          });
        });
      });
    });
  });
}
