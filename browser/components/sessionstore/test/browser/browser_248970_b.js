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
 * The Original Code is sessionstore test code.
 *
 * The Initial Developer of the Original Code is
 * Aaron Train <aaron.train@gmail.com>.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ehsan Akhgari <ehsan.akhgari@gmail.com>
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
  /** Test (B) for Bug 248970 **/

  function test(aLambda) {
    try {
      return aLambda() || true;
    } catch(ex) { }
    return false;
  }

  var file = Components.classes["@mozilla.org/file/directory_service;1"]
             .getService(Components.interfaces.nsIProperties)
             .get("TmpD", Components.interfaces.nsIFile);
  filePath = file.path;

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

  // test setup
  waitForExplicitFinish();

  // private browsing service
  let pb = Cc["@mozilla.org/privatebrowsing;1"].
           getService(Ci.nsIPrivateBrowsingService);
  gPrefService.setBoolPref("browser.privatebrowsing.keep_current_session", true);

  // sessionstore service
  let ss = test(function() Cc["@mozilla.org/browser/sessionstore;1"].
                           getService(Ci.nsISessionStore));

  //////////////////////////////////////////////////////////////////
  // Test (B) : Session data restoration between modes            //
  //////////////////////////////////////////////////////////////////

  let rootDir = getRootDirectory(gTestPath);
  const testURL = rootDir + "browser_248970_b_sample.html";
  const testURL2 = "http://mochi.test:8888/browser/" +
  "browser/components/sessionstore/test/browser/browser_248970_b_sample.html";

  // get closed tab count
  let count = ss.getClosedTabCount(window);
  let max_tabs_undo = gPrefService.getIntPref("browser.sessionstore.max_tabs_undo");
  ok(0 <= count && count <= max_tabs_undo,
    "getClosedTabCount should return zero or at most max_tabs_undo");

  // setup a state for tab (A) so we can check later that is restored
  let key = "key";
  let value = "Value " + Math.random();
  let state = { entries: [{ url: testURL }], extData: { key: value } };

  // public session, add new tab: (A)
  let tab_A = gBrowser.addTab(testURL);
  ss.setTabState(tab_A, JSON.stringify(state));
  tab_A.linkedBrowser.addEventListener("load", function(aEvent) {
    this.removeEventListener("load", arguments.callee, true);

    // make sure that the next closed tab will increase getClosedTabCount
    gPrefService.setIntPref("browser.sessionstore.max_tabs_undo", max_tabs_undo + 1)

    // populate tab_A with form data
    for (let i in fieldList)
      setFormValue(tab_A, i, fieldList[i]);

    // public session, close tab: (A)
    gBrowser.removeTab(tab_A);

    // verify that closedTabCount increased
    ok(ss.getClosedTabCount(window) > count, "getClosedTabCount has increased after closing a tab");

    // verify tab: (A), in undo list
    let tab_A_restored = test(function() ss.undoCloseTab(window, 0));
    ok(tab_A_restored, "a tab is in undo list");
    tab_A_restored.linkedBrowser.addEventListener("load", function(aEvent) {
      this.removeEventListener("load", arguments.callee, true);

      is(testURL, this.currentURI.spec, "it's the same tab that we expect");
      gBrowser.removeTab(tab_A_restored);

      // enter private browsing mode
      pb.privateBrowsingEnabled = true;
      ok(pb.privateBrowsingEnabled, "private browsing enabled");

      // setup a state for tab (B) so we can check that its duplicated properly
      let key1 = "key1";
      let value1 = "Value " + Math.random();
      let state1 = { entries: [{ url: testURL2 }], extData: { key1: value1 } };

      // private browsing session, new tab: (B)
      let tab_B = gBrowser.addTab(testURL2);
      ss.setTabState(tab_B, JSON.stringify(state1));
      tab_B.linkedBrowser.addEventListener("load", function(aEvent) {
        this.removeEventListener("load", arguments.callee, true);

        // populate tab: (B) with different form data
        for (let item in fieldList)
          setFormValue(tab_B, item, fieldList[item]);

        // duplicate tab: (B)
        let tab_C = gBrowser.duplicateTab(tab_B);
        tab_C.linkedBrowser.addEventListener("load", function(aEvent) {
          this.removeEventListener("load", arguments.callee, true);

          // verify the correctness of the duplicated tab
          is(ss.getTabValue(tab_C, key1), value1,
            "tab successfully duplicated - correct state");

          for (let item in fieldList)
            ok(compareFormValue(tab_C, item, fieldList[item]),
              "The value for \"" + item + "\" was correctly duplicated");

          // private browsing session, close tab: (C) and (B)
          gBrowser.removeTab(tab_C);
          gBrowser.removeTab(tab_B);

          // exit private browsing mode
          pb.privateBrowsingEnabled = false;
          ok(!pb.privateBrowsingEnabled, "private browsing disabled");

          // cleanup
          if (gPrefService.prefHasUserValue("browser.privatebrowsing.keep_current_session"))
            gPrefService.clearUserPref("browser.privatebrowsing.keep_current_session");
          finish();
        }, true);
      }, true);
    }, true);
  }, true);
}
