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
