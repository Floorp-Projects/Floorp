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
 * Portions created by the Initial Developer are Copyright (C) 2008
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
  /** Test for Bug 466937 **/
  
  waitForExplicitFinish();

  var file = Components.classes["@mozilla.org/file/directory_service;1"]
             .getService(Components.interfaces.nsIProperties)
             .get("TmpD", Components.interfaces.nsILocalFile);
  file.append("466937_test.file");
  file.createUnique(Components.interfaces.nsIFile.NORMAL_FILE_TYPE, 0666);
  let testPath = file.path;
  
  let testURL = "http://mochi.test:8888/browser/" +
    "browser/components/sessionstore/test/browser/browser_466937_sample.html";
  
  let tab = gBrowser.addTab(testURL);
  tab.linkedBrowser.addEventListener("load", function(aEvent) {
    tab.linkedBrowser.removeEventListener("load", arguments.callee, true);
    let doc = tab.linkedBrowser.contentDocument;
    doc.getElementById("reverse_thief").value = "/home/user/secret2";
    doc.getElementById("bystander").value = testPath;
    
    let tab2 = gBrowser.duplicateTab(tab);
    tab2.linkedBrowser.addEventListener("load", function(aEvent) {
      tab2.linkedBrowser.removeEventListener("load", arguments.callee, true);
      doc = tab2.linkedBrowser.contentDocument;
      is(doc.getElementById("thief").value, "",
         "file path wasn't set to text field value");
      is(doc.getElementById("reverse_thief").value, "",
         "text field value wasn't set to full file path");
      is(doc.getElementById("bystander").value, testPath,
         "normal case: file path was correctly preserved");
      
      // clean up
      gBrowser.removeTab(tab2);
      gBrowser.removeTab(tab);
      
      finish();
    }, true);
  }, true);
}
