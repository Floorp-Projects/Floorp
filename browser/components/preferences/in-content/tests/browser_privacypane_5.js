let loader = Cc["@mozilla.org/moz/jssubscript-loader;1"].
             getService(Ci.mozIJSSubScriptLoader);
let rootDir = getRootDirectory(gTestPath);
let jar = getJar(rootDir);
if (jar) {
  let tmpdir = extractJarToTmp(jar);
  rootDir = "file://" + tmpdir.path + '/';
}
/* import-globals-from privacypane_tests_perwindow.js */
loader.loadSubScript(rootDir + "privacypane_tests_perwindow.js", this);

run_test_subset([
  test_locbar_suggestion_retention("history", true),
  test_locbar_suggestion_retention("bookmark", true),
  test_locbar_suggestion_retention("openpage", false),
  test_locbar_suggestion_retention("history", true),
  test_locbar_suggestion_retention("history", false),
]);
