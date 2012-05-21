/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  let loader = Cc["@mozilla.org/moz/jssubscript-loader;1"].
               getService(Ci.mozIJSSubScriptLoader);
  let rootDir = getRootDirectory(gTestPath);
  let jar = getJar(rootDir);
  if (jar) {
    let tmpdir = extractJarToTmp(jar);
    rootDir = "file://" + tmpdir.path + '/';
  }
  loader.loadSubScript(rootDir + "privacypane_tests.js", this);

  run_test_subset([
    test_locbar_suggestion_retention(-1, undefined),
    test_locbar_suggestion_retention(1, -1),
    test_locbar_suggestion_retention(2, 1),
    test_locbar_suggestion_retention(0, 2),
    test_locbar_suggestion_retention(0, 0),

    // reset all preferences to their default values once we're done
    reset_preferences
  ]);
}
