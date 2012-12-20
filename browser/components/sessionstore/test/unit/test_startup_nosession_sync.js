/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */


// Test nsISessionStartup.sessionType in the following scenario:
// - no sessionstore.js;
// - the session store has not been loaded yet, so we have to trigger
//    synchronous fallback

function run_test() {
  do_get_profile();
  let startup = Cc["@mozilla.org/browser/sessionstartup;1"].
    getService(Ci.nsISessionStartup);
  do_check_eq(startup.sessionType, Ci.nsISessionStartup.NO_SESSION);
}