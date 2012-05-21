/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  /** Originally a test for Bug 476161, but then expanded to include all input types in bug 640136 **/

  waitForExplicitFinish();

  let file = Components.classes["@mozilla.org/file/directory_service;1"]
             .getService(Components.interfaces.nsIProperties)
             .get("TmpD", Components.interfaces.nsIFile);

  let testURL = "http://mochi.test:8888/browser/" +
    "browser/components/sessionstore/test/browser_form_restore_events_sample.html";
  let tab = gBrowser.addTab(testURL);
  tab.linkedBrowser.addEventListener("load", function(aEvent) {
    tab.linkedBrowser.removeEventListener("load", arguments.callee, true);
    let doc = tab.linkedBrowser.contentDocument;

    // text fields
    doc.getElementById("modify01").value += Math.random();
    doc.getElementById("modify02").value += " " + Date.now();

    // textareas
    doc.getElementById("modify03").value += Math.random();
    doc.getElementById("modify04").value += " " + Date.now();

    // file
    doc.getElementById("modify05").value = file.path;

    // select
    doc.getElementById("modify06").selectedIndex = 1;
    var multipleChange = doc.getElementById("modify07");
    Array.forEach(multipleChange.options, function(option) option.selected = true);

    // checkbox
    doc.getElementById("modify08").checked = true;
    doc.getElementById("modify09").checked = false;

    // radio
    // select one then another in the same group - only last one should get event on restore
    doc.getElementById("modify10").checked = true;
    doc.getElementById("modify11").checked = true;


    let tab2 = gBrowser.duplicateTab(tab);
    tab2.linkedBrowser.addEventListener("load", function(aEvent) {
      tab2.linkedBrowser.removeEventListener("load", arguments.callee, true);
      let doc = tab2.linkedBrowser.contentDocument;
      let inputFired = doc.getElementById("inputFired").textContent.trim().split();
      let changeFired = doc.getElementById("changeFired").textContent.trim().split();

      is(inputFired.sort().join(" "), "modify01 modify02 modify03 modify04 modify05",
         "input events were only dispatched for modified input, textarea fields");

      is(changeFired.sort().join(" "), "modify06 modify07 modify08 modify09 modify11",
         "change events were only dispatched for modified select, checkbox, radio fields");

      // clean up
      gBrowser.removeTab(tab2);
      gBrowser.removeTab(tab);

      finish();
    }, true);
  }, true);
}
