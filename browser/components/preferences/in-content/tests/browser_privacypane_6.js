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
  try {
    loader.loadSubScript(rootDir + "privacypane_tests_perwindow.js", this);
  } catch(x) {
    loader.loadSubScript(rootDir + "privacypane_tests.js", this);
  }

  run_test_subset([
    test_privatebrowsing_toggle,
    enter_private_browsing, // once again, test with PB initially enabled
    test_privatebrowsing_toggle,

    // don't reset preferences, will pick up where we left off in browser_privacypane_7.js
  ]);
}
