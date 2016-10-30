requestLongerTimeout(2);

let loader = Cc["@mozilla.org/moz/jssubscript-loader;1"].
             getService(Ci.mozIJSSubScriptLoader);
let rootDir = getRootDirectory(gTestPath);
let jar = getJar(rootDir);
if (jar) {
  let tmpdir = extractJarToTmp(jar);
  rootDir = "file://" + tmpdir.path + '/';
}
loader.loadSubScript(rootDir + "privacypane_tests_perwindow.js", this);
let runtime = Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULRuntime);

run_test_subset([
  test_custom_retention("acceptCookies", "remember"),
  test_custom_retention("acceptCookies", "custom"),
  test_custom_retention("acceptThirdPartyMenu", "custom", "visited"),
  test_custom_retention("acceptThirdPartyMenu", "custom", "always"),
  test_custom_retention("keepCookiesUntil", "custom", 1),
  test_custom_retention("keepCookiesUntil", "custom", 2),
  test_custom_retention("keepCookiesUntil", "custom", 0),
  test_custom_retention("alwaysClear", "custom"),
  test_custom_retention("alwaysClear", "custom"),
  test_historymode_retention("remember", "custom"),
]);
