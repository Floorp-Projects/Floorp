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
  /** Test for Bug 454908 **/
  
  waitForExplicitFinish();
  
  let fieldValues = {
    username: "User " + Math.random(),
    passwd:   "pwd" + Date.now()
  };
  
  // make sure we do save form data
  gPrefService.setIntPref("browser.sessionstore.privacy_level", 0);
  
  let rootDir = getRootDirectory(gTestPath);
  let testURL = rootDir + "browser_454908_sample.html";
  let tab = gBrowser.addTab(testURL);
  tab.linkedBrowser.addEventListener("load", function(aEvent) {
    tab.linkedBrowser.removeEventListener("load", arguments.callee, true);
    let doc = tab.linkedBrowser.contentDocument;
    for (let id in fieldValues)
      doc.getElementById(id).value = fieldValues[id];
    
    gBrowser.removeTab(tab);
    
    tab = undoCloseTab();
    tab.linkedBrowser.addEventListener("load", function(aEvent) {
      tab.linkedBrowser.removeEventListener("load", arguments.callee, true);
      let doc = tab.linkedBrowser.contentDocument;
      for (let id in fieldValues) {
        let node = doc.getElementById(id);
        if (node.type == "password")
          is(node.value, "", "password wasn't saved/restored");
        else
          is(node.value, fieldValues[id], "username was saved/restored");
      }
      
      // clean up
      if (gPrefService.prefHasUserValue("browser.sessionstore.privacy_level"))
        gPrefService.clearUserPref("browser.sessionstore.privacy_level");
      // undoCloseTab can reuse a single blank tab, so we have to
      // make sure not to close the window when closing our last tab
      if (gBrowser.tabs.length == 1)
        gBrowser.addTab();
      gBrowser.removeTab(tab);
      finish();
    }, true);
  }, true);
}
