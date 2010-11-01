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
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Paul O’Shannessy <paul@oshannessy.com>
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

Cu.import("resource://gre/modules/Services.jsm");
let ss = Cc["@mozilla.org/browser/sessionstore;1"].
         getService(Ci.nsISessionStore);

let stateBackup = ss.getBrowserState();


function test() {
  /** Test for bug 581593 **/
  waitForExplicitFinish();

  let oldState = { windows: [{ tabs: [{ entries: [{ url: "example.com" }] }] }]};
  let pageData = {
    url: "about:sessionrestore",
    formdata: { "#sessionData": "(" + JSON.stringify(oldState) + ")" }
  };
  let state = { windows: [{ tabs: [{ entries: [pageData] }] }] };

  // The form data will be restored before SSTabRestored, so we want to listen
  // for that on the currently selected tab (it will be reused)
  gBrowser.selectedTab.addEventListener("SSTabRestored", onSSTabRestored, true);

  ss.setBrowserState(JSON.stringify(state));
}

function onSSTabRestored(aEvent) {
  info("SSTabRestored event");
  gBrowser.selectedTab.removeEventListener("SSTabRestored", onSSTabRestored, true);
  gBrowser.selectedBrowser.addEventListener("input", onInput, true);
}

function onInput(aEvent) {
  info("input event");
  gBrowser.selectedBrowser.removeEventListener("input", onInput, true);

  // This is an ok way to check this because we will make sure that the text
  // field is parsable.
  let val = gBrowser.selectedBrowser.contentDocument.getElementById("sessionData").value;
  try {
    JSON.parse(val);
    ok(true, "JSON.parse succeeded");
  }
  catch (e) {
    ok(false, "JSON.parse failed");
  }
  cleanup();
}

function cleanup() {
  ss.setBrowserState(stateBackup);
  executeSoon(finish);
}

