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
  /** Test for Bug 464620 (injection on input) **/
  
  waitForExplicitFinish();
  
  let testURL = "http://mochi.test:8888/browser/" +
    "browser/components/sessionstore/test/browser_464620_a.html";
  
  var frameCount = 0;
  let tab = gBrowser.addTab(testURL);
  tab.linkedBrowser.addEventListener("load", function(aEvent) {
    // wait for all frames to load completely
    if (frameCount++ < 4)
      return;
    this.removeEventListener("load", arguments.callee, true);
    
    executeSoon(function() {
      frameCount = 0;
      let tab2 = gBrowser.duplicateTab(tab);
      tab2.linkedBrowser.addEventListener("464620_a", function(aEvent) {
        tab2.linkedBrowser.removeEventListener("464620_a", arguments.callee, true);
        is(aEvent.data, "done", "XSS injection was attempted");
        
        // let form restoration complete and take into account the
        // setTimeout(..., 0) in sss_restoreDocument_proxy
        executeSoon(function() {
          setTimeout(function() {
            let win = tab2.linkedBrowser.contentWindow;
            isnot(win.frames[0].document.location, testURL,
                  "cross domain document was loaded");
            ok(!/XXX/.test(win.frames[0].document.body.innerHTML),
               "no content was injected");
            
            // clean up
            gBrowser.removeTab(tab2);
            gBrowser.removeTab(tab);
            
            finish();
          }, 0);
        });
      }, true, true);
    });
  }, true);
}
