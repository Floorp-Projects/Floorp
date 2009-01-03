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
  /** Test for Bug 448741 **/
  
  // test setup
  let ss = Cc["@mozilla.org/browser/sessionstore;1"].getService(Ci.nsISessionStore);
  let os = Cc["@mozilla.org/observer-service;1"].getService(Ci.nsIObserverService);
  let tabbrowser = getBrowser();
  waitForExplicitFinish();
  
  let uniqueName = "bug 448741";
  let uniqueValue = "as good as unique: " + Date.now();
  let interval = gPrefService.getIntPref("browser.sessionstore.interval");
  
  // set a unique value on a new, blank tab
  var tab = tabbrowser.addTab();
  ss.setTabValue(tab, uniqueName, uniqueValue);
  let valueWasCleaned = false;
  
  // prevent our value from being written to disk
  let cleaningObserver = {
    observe: function(aSubject, aTopic, aData) {
      ok(aTopic == "sessionstore-state-write", "observed correct topic?");
      ok(aSubject instanceof Ci.nsISupportsString, "subject is a string?");
      ok(aSubject.data.indexOf(uniqueValue) > -1, "data contains our value?");
      
      // find the data for the newly added tab and delete it
      let state = eval(aSubject.data);
      for each (let winData in state.windows) {
        for each (let tabData in winData.tabs) {
          if (tabData.extData && uniqueName in tabData.extData &&
              tabData.extData[uniqueName] == uniqueValue) {
            delete tabData.extData[uniqueName];
            valueWasCleaned = true;
          }
        }
      }
      
      ok(valueWasCleaned, "found and removed the specific tab value");
      aSubject.data = uneval(state);
      os.removeObserver(this, aTopic, false);
    }
  };
  
  // make sure that all later observers don't see that value any longer
  let checkingObserver = {
    observe: function(aSubject, aTopic, aData) {
      ok(valueWasCleaned && aSubject instanceof Ci.nsISupportsString,
         "ready to check the cleaned state?");
      ok(aSubject.data.indexOf(uniqueValue) == -1, "data no longer contains our value?");
      
      // clean up
      tabbrowser.removeTab(tab);
      os.removeObserver(this, aTopic, false);
      gPrefService.setIntPref("browser.sessionstore.interval", interval);
      finish();
    }
  };
  
  // last added observers are invoked first
  os.addObserver(checkingObserver, "sessionstore-state-write", false);
  os.addObserver(cleaningObserver, "sessionstore-state-write", false);
  
  // trigger an immediate save operation
  gPrefService.setIntPref("browser.sessionstore.interval", 0);
}
