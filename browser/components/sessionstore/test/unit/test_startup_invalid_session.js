/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function run_test() {
  let profd = do_get_profile();
  var SessionFile = Cu.import("resource:///modules/sessionstore/SessionFile.jsm", {}).SessionFile;

  let sourceSession = do_get_file("data/sessionstore_invalid.js");
  sourceSession.copyTo(profd, "sessionstore.js");

  let sourceCheckpoints = do_get_file("data/sessionCheckpoints_all.json");
  sourceCheckpoints.copyTo(profd, "sessionCheckpoints.json");

  // Compress sessionstore.js to sessionstore.jsonlz4
  // and remove sessionstore.js
  let oldExtSessionFile = SessionFile.Paths.clean.replace("jsonlz4", "js");
  writeCompressedFile(oldExtSessionFile, SessionFile.Paths.clean).then(() => {
    let startup = Cc["@mozilla.org/browser/sessionstartup;1"].
        getService(Ci.nsISessionStartup);

    afterSessionStartupInitialization(function cb() {
      Assert.equal(startup.sessionType, Ci.nsISessionStartup.NO_SESSION);
      do_test_finished();
    });
  });

  do_test_pending();
}
