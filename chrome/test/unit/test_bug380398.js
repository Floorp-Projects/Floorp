/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

var MANIFESTS = [do_get_file("data/test_bug380398.manifest")];

registerManifests(MANIFESTS);

const { updateAppInfo } = ChromeUtils.import(
  "resource://testing-common/AppInfo.jsm"
);
updateAppInfo({
  name: "XPCShell",
  id: "{39885e5f-f6b4-4e2a-87e5-6259ecf79011}",
  version: "5",
  platformVersion: "1.9",
});

var chromeReg = Cc["@mozilla.org/chrome/chrome-registry;1"].getService(
  Ci.nsIChromeRegistry
);
chromeReg.checkForNewChrome();

var target = Services.io.newFileURI(do_get_file("data"));
target = target.spec + "test/test.xul";

function test_succeeded_mapping(namespace) {
  var uri = Services.io.newURI("chrome://" + namespace + "/content/test.xul");
  try {
    var result = chromeReg.convertChromeURL(uri);
    Assert.equal(result.spec, target);
  } catch (ex) {
    do_throw(namespace);
  }
}

function test_failed_mapping(namespace) {
  var uri = Services.io.newURI("chrome://" + namespace + "/content/test.xul");
  try {
    chromeReg.convertChromeURL(uri);
    do_throw(namespace);
  } catch (ex) {}
}

function run_test() {
  test_succeeded_mapping("test1");
  test_succeeded_mapping("test2");
  test_succeeded_mapping("test3");
  test_succeeded_mapping("test4");
  test_succeeded_mapping("test5");
  test_failed_mapping("test6");
  test_failed_mapping("test7");
  test_failed_mapping("test8");
  test_failed_mapping("test9");
  test_failed_mapping("test10");
  test_failed_mapping("test11");
}
