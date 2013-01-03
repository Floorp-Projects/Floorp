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
  try {
    loader.loadSubScript(rootDir + "privacypane_tests_perwindow.js", this);
  } catch(x) {
    loader.loadSubScript(rootDir + "privacypane_tests.js", this);
  }

  run_test_subset([
    test_pane_visibility,
    test_dependent_elements,
    test_dependent_cookie_elements,
    test_dependent_clearonclose_elements,
    test_dependent_prefs,

    // reset all preferences to their default values once we're done
    reset_preferences
  ]);
}
