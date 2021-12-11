let rootDir = getRootDirectory(gTestPath);
let jar = getJar(rootDir);
if (jar) {
  let tmpdir = extractJarToTmp(jar);
  rootDir = "file://" + tmpdir.path + "/";
}
/* import-globals-from privacypane_tests_perwindow.js */
Services.scriptloader.loadSubScript(
  rootDir + "privacypane_tests_perwindow.js",
  this
);

run_test_subset([
  test_pane_visibility,
  test_dependent_elements,
  test_dependent_cookie_elements,
  test_dependent_clearonclose_elements,
  test_dependent_prefs,
]);
