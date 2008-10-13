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
  /** Test for Bug 456342 **/
  
  waitForExplicitFinish();
  
  // make sure we do save form data
  let privacy_level = gPrefService.getIntPref("browser.sessionstore.privacy_level");
  gPrefService.setIntPref("browser.sessionstore.privacy_level", 0);
  
  let testURL = "chrome://mochikit/content/browser/" +
    "browser/components/sessionstore/test/browser/browser_456342_sample.xhtml";
  let tab = gBrowser.addTab(testURL);
  tab.linkedBrowser.addEventListener("load", function(aEvent) {
    gBrowser.removeTab(tab);
    
    let ss = Cc["@mozilla.org/browser/sessionstore;1"]
               .getService(Ci.nsISessionStore);
    let undoItems = eval("(" + ss.getClosedTabData(window) + ")");
    let savedFormData = undoItems[0].state.entries[0].formdata;
    
    let countGood = 0, countBad = 0;
    for each (let value in savedFormData) {
      if (value == "save me")
        countGood++;
      else
        countBad++;
    }
    
    is(countGood, 4, "Saved text for non-standard input fields");
    is(countBad,  0, "Didn't save text for ignored field types");
    
    // clean up
    gPrefService.setIntPref("browser.sessionstore.privacy_level", privacy_level);
    finish();
  }, true);
}
