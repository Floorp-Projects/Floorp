/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

const UNPACKAGED_ADDON = do_get_file("data/test_bug564667");
const PACKAGED_ADDON = do_get_file("data/test_bug564667.xpi");

var gCR = Cc["@mozilla.org/chrome/chrome-registry;1"].
          getService(Ci.nsIChromeRegistry);

/*
 * Checks that a mapping was added
 */
function test_mapping(chromeURL, target) {
  var uri = Services.io.newURI(chromeURL);

  try {
    var result = gCR.convertChromeURL(uri);
    Assert.equal(result.spec, target);
  } catch (ex) {
    do_throw(chromeURL + " not Registered");
  }
}

/*
 * Checks that a mapping was removed
 */
function test_removed_mapping(chromeURL, target) {
  var uri = Services.io.newURI(chromeURL);
  try {
    gCR.convertChromeURL(uri);
    do_throw(chromeURL + " not removed");
  } catch (ex) {
    // This should throw
  }
}

function testManifest(manifestPath, baseURI) {

  // ------------------  Add manifest file ------------------------
  Components.manager.addBootstrappedManifestLocation(manifestPath);

  // Test Adding Content URL
  test_mapping("chrome://test1/content", baseURI + "test/test1.xul");

  // Test Adding Locale URL
  test_mapping("chrome://test1/locale", baseURI + "test/test1.dtd");

  // Test Adding Skin URL
  test_mapping("chrome://test1/skin", baseURI + "test/test1.css");

  // Test Adding Manifest URL
  test_mapping("chrome://test2/content", baseURI + "test/test2.xul");
  test_mapping("chrome://test2/locale", baseURI + "test/test2.dtd");

  // Test Adding Override
  test_mapping("chrome://testOverride/content", "file:///test1/override");

  // ------------------  Remove manifest file ------------------------
  Components.manager.removeBootstrappedManifestLocation(manifestPath);

  // Test Removing Content URL
  test_removed_mapping("chrome://test1/content", baseURI + "test/test1.xul");

  // Test Removing Content URL
  test_removed_mapping("chrome://test1/locale", baseURI + "test/test1.dtd");

  // Test Removing Skin URL
  test_removed_mapping("chrome://test1/skin", baseURI + "test/test1.css");

  // Test Removing Manifest URL
  test_removed_mapping("chrome://test2/content", baseURI + "test/test2.xul");
  test_removed_mapping("chrome://test2/locale", baseURI + "test/test2.dtd");
}

function run_test() {
  // Test an unpackaged addon
  testManifest(UNPACKAGED_ADDON, Services.io.newFileURI(UNPACKAGED_ADDON).spec);

  // Test a packaged addon
  testManifest(PACKAGED_ADDON, "jar:" + Services.io.newFileURI(PACKAGED_ADDON).spec + "!/");
}
