/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
