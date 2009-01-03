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
  /** Test for Bug 463206 **/
  
  waitForExplicitFinish();
  
  let testURL = "http://localhost:8888/browser/" +
    "browser/components/sessionstore/test/browser/browser_463206_sample.html";
  
  var frameCount = 0;
  let tab = gBrowser.addTab(testURL);
  tab.linkedBrowser.addEventListener("load", function(aEvent) {
    // wait for all frames to load completely
    if (frameCount++ < 5)
      return;
    this.removeEventListener("load", arguments.callee, true);
    
    function typeText(aTextField, aValue) {
      aTextField.value = aValue;
      
      let event = aTextField.ownerDocument.createEvent("UIEvents");
      event.initUIEvent("input", true, true, aTextField.ownerDocument.defaultView, 0);
      aTextField.dispatchEvent(event);
    }
    
    let doc = tab.linkedBrowser.contentDocument;
    typeText(doc.getElementById("out1"), Date.now());
    typeText(doc.getElementsByName("1|#out2")[0], Math.random());
    typeText(doc.defaultView.frames[0].frames[1].document.getElementById("in1"), new Date());
    
    frameCount = 0;
    let tab2 = gBrowser.duplicateTab(tab);
    tab2.linkedBrowser.addEventListener("load", function(aEvent) {
      // wait for all frames to load completely
      if (frameCount++ < 5)
        return;
      
      let doc = tab2.linkedBrowser.contentDocument;
      let win = tab2.linkedBrowser.contentWindow;
      isnot(doc.getElementById("out1").value,
            win.frames[1].document.getElementById("out1").value,
            "text isn't reused for frames");
      isnot(doc.getElementsByName("1|#out2")[0].value, "",
            "text containing | and # is correctly restored");
      is(win.frames[1].document.getElementById("out2").value, "",
            "id prefixes can't be faked");
      isnot(win.frames[0].frames[1].document.getElementById("in1").value, "",
            "id prefixes aren't mixed up");
      is(win.frames[1].frames[0].document.getElementById("in1").value, "",
            "id prefixes aren't mixed up");
      
      // clean up
      gBrowser.removeTab(tab2);
      gBrowser.removeTab(tab);
      
      finish();
    }, true);
  }, true);
}
