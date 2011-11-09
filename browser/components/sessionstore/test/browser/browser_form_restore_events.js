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
 * Simon Bünzli <zeniko@gmail.com>.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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
  /** Originally a test for Bug 476161, but then expanded to include all input types in bug 640136 **/

  waitForExplicitFinish();

  let file = Components.classes["@mozilla.org/file/directory_service;1"]
             .getService(Components.interfaces.nsIProperties)
             .get("TmpD", Components.interfaces.nsIFile);

  let testURL = "http://mochi.test:8888/browser/" +
    "browser/components/sessionstore/test/browser/browser_form_restore_events_sample.html";
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
