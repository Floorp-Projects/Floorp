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
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *    Ehsan Akhgari <ehsan@mozilla.com>
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
  /** Test for Bug 526613 **/
  
  // test setup
  let ss = Cc["@mozilla.org/browser/sessionstore;1"].
           getService(Ci.nsISessionStore);
  let os = Cc["@mozilla.org/observer-service;1"].
           getService(Ci.nsIObserverService);
  let wm = Cc["@mozilla.org/appshell/window-mediator;1"].
           getService(Ci.nsIWindowMediator);
  waitForExplicitFinish();

  function browserWindowsCount() {
    let count = 0;
    let e = wm.getEnumerator("navigator:browser");
    while (e.hasMoreElements()) {
      if (!e.getNext().closed)
        ++count;
    }

    return count;
  }

  is(browserWindowsCount(), 1, "Only one browser window should be open initially");

  // backup old state
  let oldState = ss.getBrowserState();
  // create a new state for testing
  let testState = {
    windows: [
      { tabs: [{ entries: [{ url: "http://example.com/" }] }], selected: 1 },
      { tabs: [{ entries: [{ url: "about:robots"        }] }], selected: 1 },
    ],
    // make sure the first window is focused, otherwise when restoring the
    // old state, the first window is closed and the test harness gets unloaded
    selectedWindow: 1
  };

  let observer = {
    pass: 1,
    observe: function(aSubject, aTopic, aData) {
      is(aTopic, "sessionstore-browser-state-restored",
         "The sessionstore-browser-state-restored notification was observed");

      if (this.pass++ == 1) {  
        is(browserWindowsCount(), 2, "Two windows should exist at this point");

        // let the first window be focused (see above)
        var fm = Cc["@mozilla.org/focus-manager;1"].getService(Ci.nsIFocusManager);
        if (window == fm.activeWindow) {
          executeSoon(function () ss.setBrowserState(oldState));
        } else {
          window.addEventListener("activate", function () {
            window.removeEventListener("activate", arguments.callee, false);
            ss.setBrowserState(oldState);
          }, false);
        }
      }
      else {
        is(browserWindowsCount(), 1, "Only one window should exist after cleanup");
        os.removeObserver(this, "sessionstore-browser-state-restored");
        finish();
      }
    }
  };
  os.addObserver(observer, "sessionstore-browser-state-restored", false);

  // set browser to test state
  ss.setBrowserState(JSON.stringify(testState));
}
