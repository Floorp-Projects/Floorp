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
  /** Test for Bug 345898 **/
  
  function test(aLambda) {
    try {
      aLambda();
      return false;
    }
    catch (ex) {
      return ex.name == "NS_ERROR_ILLEGAL_VALUE";
    }
  }
  
  let ss = Cc["@mozilla.org/browser/sessionstore;1"].getService(Ci.nsISessionStore);
  
  // all of the following calls with illegal arguments should throw NS_ERROR_ILLEGAL_VALUE
  ok(test(function() ss.getWindowState({})),
     "Invalid window for getWindowState throws");
  ok(test(function() ss.setWindowState({}, "", false)),
     "Invalid window for setWindowState throws");
  ok(test(function() ss.getTabState({})),
     "Invalid tab for getTabState throws");
  ok(test(function() ss.setTabState({}, "{}")),
     "Invalid tab state for setTabState throws");
  ok(test(function() ss.setTabState({}, "{ entries: [] }")),
     "Invalid tab for setTabState throws");
  ok(test(function() ss.duplicateTab({}, {})),
     "Invalid tab for duplicateTab throws");
  ok(test(function() ss.duplicateTab({}, getBrowser().selectedTab)),
     "Invalid window for duplicateTab throws");
  ok(test(function() ss.getClosedTabData({})),
     "Invalid window for getClosedTabData throws");
  ok(test(function() ss.undoCloseTab({}, 0)),
     "Invalid window for undoCloseTab throws");
  ok(test(function() ss.undoCloseTab(window, -1)),
     "Invalid index for undoCloseTab throws");
  ok(test(function() ss.getWindowValue({}, "")),
     "Invalid window for getWindowValue throws");
  ok(test(function() ss.getWindowValue({}, "")),
     "Invalid window for getWindowValue throws");
  ok(test(function() ss.getWindowValue({}, "", "")),
     "Invalid window for setWindowValue throws");
  ok(test(function() ss.deleteWindowValue({}, "")),
     "Invalid window for deleteWindowValue throws");
  ok(test(function() ss.deleteWindowValue(window, Date.now().toString())),
     "Nonexistent value for deleteWindowValue throws");
  ok(test(function() ss.deleteTabValue(getBrowser().selectedTab, Date.now().toString())),
     "Nonexistent value for deleteTabValue throws");
}
