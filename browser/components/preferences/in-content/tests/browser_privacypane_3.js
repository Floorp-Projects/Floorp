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
  test_custom_retention("rememberHistory", "remember"),
  test_custom_retention("rememberHistory", "custom"),
  test_custom_retention("rememberForms", "remember"),
  test_custom_retention("rememberForms", "custom"),
  test_historymode_retention("remember", "remember"),
]);
