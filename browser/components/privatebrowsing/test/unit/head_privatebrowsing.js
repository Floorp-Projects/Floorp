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
 * The Original Code is Private Browsing Tests.
 *
 * The Initial Developer of the Original Code is
 * Ehsan Akhgari.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ehsan Akhgari <ehsan.akhgari@gmail.com> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

const kPrivateBrowsingNotification = "private-browsing";
const kPrivateBrowsingCancelVoteNotification = "private-browsing-cancel-vote";
const kPrivateBrowsingTransitionCompleteNotification = "private-browsing-transition-complete";
const kEnter = "enter";
const kExit = "exit";

const NS_APP_USER_PROFILE_50_DIR = "ProfD";

function LOG(aMsg) {
  aMsg = ("*** PRIVATEBROWSING TESTS: " + aMsg);
  Cc["@mozilla.org/consoleservice;1"].getService(Ci.nsIConsoleService).
                                      logStringMessage(aMsg);
  print(aMsg);
}

function uri(spec) {
  return Cc["@mozilla.org/network/io-service;1"].
         getService(Ci.nsIIOService).
         newURI(spec, null, null);
}

var dirSvc = Cc["@mozilla.org/file/directory_service;1"].getService(Ci.nsIProperties);
var profileDir = do_get_profile();

// Do not attempt to restore any session since we don't have any windows
Cc["@mozilla.org/preferences-service;1"].
  getService(Ci.nsIPrefBranch).
  setBoolPref("browser.privatebrowsing.keep_current_session", true);

/**
 * Removes any files that could make our tests fail.
 */
function cleanUp()
{
  let files = [
    "downloads.sqlite",
    "places.sqlite",
    "cookies.sqlite",
    "signons.sqlite",
    "permissions.sqlite"
  ];

  for (let i = 0; i < files.length; i++) {
    let file = dirSvc.get("ProfD", Ci.nsIFile);
    file.append(files[i]);
    if (file.exists())
      file.remove(false);
  }
}
cleanUp();

var PRIVATEBROWSING_CONTRACT_ID;
function run_test_on_all_services() {
  var contractIDs = [
    "@mozilla.org/privatebrowsing;1",
    "@mozilla.org/privatebrowsing-wrapper;1"
  ];
  for (var i = 0; i < contractIDs.length; ++i) {
    PRIVATEBROWSING_CONTRACT_ID = contractIDs[i];
    run_test_on_service();
    cleanUp();
  }
}
