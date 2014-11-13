/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  let loader = Cc["@mozilla.org/moz/jssubscript-loader;1"].
               getService(Ci.mozIJSSubScriptLoader);
  let rootDir = getRootDirectory(gTestPath);
  let jar = getJar(rootDir);
  if (jar) {
    let tmpdir = extractJarToTmp(jar);
    rootDir = "file://" + tmpdir.path + '/';
  }
  loader.loadSubScript(rootDir + "privacypane_tests_perwindow.js", this);

  run_test_subset([
    test_locbar_suggestion_retention("history", true),
    test_locbar_suggestion_retention("bookmark", true),
    test_locbar_suggestion_retention("openpage", false),
    test_locbar_suggestion_retention("history", true),
    test_locbar_suggestion_retention("history", false),

    // reset all preferences to their default values once we're done
    reset_preferences
  ]);
}