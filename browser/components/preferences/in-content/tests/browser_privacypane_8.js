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
  // history mode should be initialized to remember
  test_historymode_retention("remember", undefined),

  // history mode should remain remember; toggle acceptCookies checkbox
  test_custom_retention("acceptCookies", "remember"),

  // history mode should now be custom; set history mode to dontremember
  test_historymode_retention("dontremember", "custom"),

  // history mode should remain custom; set history mode to remember
  test_historymode_retention("remember", "custom"),

  // history mode should now be remember
  test_historymode_retention("remember", "remember"),
]);
