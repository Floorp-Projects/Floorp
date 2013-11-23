/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  /** Test for Bug 346337 **/

  var file = Components.classes["@mozilla.org/file/directory_service;1"]
               .getService(Components.interfaces.nsIProperties)
               .get("TmpD", Components.interfaces.nsILocalFile);
  file.append("346337_test1.file");
  file.createUnique(Components.interfaces.nsIFile.NORMAL_FILE_TYPE, 0666);
  var filePath1 = file.path;
  file = Components.classes["@mozilla.org/file/directory_service;1"]
             .getService(Components.interfaces.nsIProperties)
             .get("TmpD", Components.interfaces.nsILocalFile);
  file.append("346337_test2.file");
  file.createUnique(Components.interfaces.nsIFile.NORMAL_FILE_TYPE, 0666);
  var filePath2 = file.path;

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
    "//input[@type='file'][1]":   [filePath1],
    "//input[@type='file'][2]":   [filePath1, filePath2]
  };

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
    else if (node instanceof Ci.nsIDOMHTMLInputElement && node.type == "file")
      node.mozSetFileNameArray(aValue, aValue.length);
    else
      Array.forEach(node.options, function(aOpt, aIx)
                                    (aOpt.selected = aValue.indexOf(aIx) > -1));
  }

  function compareFormValue(aTab, aQuery, aValue) {
    let node = getElementByXPath(aTab, aQuery);
    if (!node)
      return false;
    if (node instanceof Ci.nsIDOMHTMLInputElement) {
      if (node.type == "file") {
        let fileNames = node.mozGetFileNameArray();
        return fileNames.length == aValue.length &&
               Array.every(fileNames, function(aFile) aValue.indexOf(aFile) >= 0);
      }
      return aValue == (node.type == "checkbox" || node.type == "radio" ?
                        node.checked : node.value);
    }
    if (node instanceof Ci.nsIDOMHTMLTextAreaElement)
      return aValue == node.value;
    if (!node.multiple)
      return aValue == node.selectedIndex;
    return Array.every(node.options, function(aOpt, aIx)
                                       (aValue.indexOf(aIx) > -1) == aOpt.selected);
  }

  // test setup
  let tabbrowser = gBrowser;
  waitForExplicitFinish();

  // make sure we don't save form data at all (except for tab duplication)
  gPrefService.setIntPref("browser.sessionstore.privacy_level", 2);

  let rootDir = getRootDirectory(gTestPath);
  let testURL = rootDir + "browser_346337_sample.html";
  let tab = tabbrowser.addTab(testURL);
  whenBrowserLoaded(tab.linkedBrowser, function() {
    for (let xpath in fieldList)
      setFormValue(tab, xpath, fieldList[xpath]);

    let tab2 = tabbrowser.duplicateTab(tab);
    whenTabRestored(tab2, function() {
      for (let xpath in fieldList)
        ok(compareFormValue(tab2, xpath, fieldList[xpath]),
           "The value for \"" + xpath + "\" was correctly restored");

      // clean up
      tabbrowser.removeTab(tab2);
      tabbrowser.removeTab(tab);

      tab = undoCloseTab();
      whenTabRestored(tab, function() {
        for (let xpath in fieldList)
          if (fieldList[xpath])
            ok(!compareFormValue(tab, xpath, fieldList[xpath]),
               "The value for \"" + xpath + "\" was correctly discarded");

        if (gPrefService.prefHasUserValue("browser.sessionstore.privacy_level"))
          gPrefService.clearUserPref("browser.sessionstore.privacy_level");
        // undoCloseTab can reuse a single blank tab, so we have to
        // make sure not to close the window when closing our last tab
        if (tabbrowser.tabs.length == 1)
          tabbrowser.addTab();
        tabbrowser.removeTab(tab);
        finish();
      });
    });
  });
}
