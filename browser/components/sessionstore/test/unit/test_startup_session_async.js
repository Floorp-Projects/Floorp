/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */


// Test nsISessionStartup.sessionType in the following scenario:
// - valid sessionstore.js;
// - the session store has been loaded, so no need to go
//    through the synchronous fallback

function run_test() {
  let profd = do_get_profile();
  let source = do_get_file("data/sessionstore_valid.js");
  source.copyTo(profd, "sessionstore.js");

  do_test_pending();
  let startup = Cc["@mozilla.org/browser/sessionstartup;1"].
    getService(Ci.nsISessionStartup);

  afterSessionStartupInitialization(function cb() {
    do_check_eq(startup.sessionType, Ci.nsISessionStartup.DEFER_SESSION);
    do_test_finished();
  });
}