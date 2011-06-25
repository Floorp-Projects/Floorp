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
 * Portions created by the Initial Developer are Copyright (C) 2009
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
  /** Test for Bug 493467 **/

  let tab = gBrowser.addTab();
  tab.linkedBrowser.stop();
  let tabState = JSON.parse(ss.getTabState(tab));
  is(tabState.disallow || "", "", "Everything is allowed per default");
  
  // collect all permissions that can be set on a docShell (i.e. all
  // attributes starting with "allow" such as "allowJavascript") and
  // disallow them all, as SessionStore only remembers disallowed ones
  let permissions = [];
  let docShell = tab.linkedBrowser.docShell;
  for (let attribute in docShell) {
    if (/^allow([A-Z].*)/.test(attribute)) {
      permissions.push(RegExp.$1);
      docShell[attribute] = false;
    }
  }
  
  // make sure that all available permissions have been remembered
  tabState = JSON.parse(ss.getTabState(tab));
  let disallow = tabState.disallow.split(",");
  permissions.forEach(function(aName) {
    ok(disallow.indexOf(aName) > -1, "Saved state of allow" + aName);
  });
  // IF A TEST FAILS, please add the missing permission's name (without the
  // leading "allow") to nsSessionStore.js's CAPABILITIES array. Thanks.
  
  gBrowser.removeTab(tab);
}
