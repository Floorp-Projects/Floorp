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
  loader.loadSubScript(rootDir + "privacypane_tests_perwindow.js", this);

  run_test_subset([
    test_custom_retention("acceptCookies", "remember"),
    test_custom_retention("acceptCookies", "custom"),
#ifdef RELEASE_BUILD
    test_custom_retention("acceptThirdPartyMenu", "remember", "visited"),
    test_custom_retention("acceptThirdPartyMenu", "custom", "always"),
#else
    test_custom_retention("acceptThirdPartyMenu", "remember", "always"),
    test_custom_retention("acceptThirdPartyMenu", "custom", "visited"),
#endif
    test_custom_retention("keepCookiesUntil", "remember", 1),
    test_custom_retention("keepCookiesUntil", "custom", 2),
    test_custom_retention("keepCookiesUntil", "custom", 0),
    test_custom_retention("alwaysClear", "remember"),
    test_custom_retention("alwaysClear", "custom"),
    test_historymode_retention("remember", "remember"),

    // reset all preferences to their default values once we're done
    reset_preferences
  ]);
}
