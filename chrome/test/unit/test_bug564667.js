/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 *      the Mozilla Foundation.
 *
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *      Hernan Rodriguez Colmeiro <colmeiro@gmail.com>.
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK *****
 */

const UNPACKAGED_ADDON = do_get_file("data/test_bug564667");
const PACKAGED_ADDON = do_get_file("data/test_bug564667.xpi");

var gIOS = Cc["@mozilla.org/network/io-service;1"].
           getService(Ci.nsIIOService);

var gCR = Cc["@mozilla.org/chrome/chrome-registry;1"].
          getService(Ci.nsIChromeRegistry).
          QueryInterface(Ci.nsIXULOverlayProvider);

/*
 * Checks that a mapping was added
 */
function test_mapping(chromeURL, target) {
  var uri = gIOS.newURI(chromeURL, null, null);

  try {
    var result = gCR.convertChromeURL(uri);
    do_check_eq(result.spec, target);
  }
  catch (ex) {
    do_throw(chromeURL + " not Registered");
  }
}

/*
 * Checks that a mapping was removed
 */
function test_removed_mapping(chromeURL, target) {
  var uri = gIOS.newURI(chromeURL, null, null);
  try {
    var result = gCR.convertChromeURL(uri);
    do_throw(chromeURL + " not removed");
  }
  catch (ex) {
    // This should throw
  }
}

/*
 * Checks if any overlay was added after loading
 * the manifest files
 *
 * @param type The type of overlay: overlay|style
 */
function test_no_overlays(chromeURL, target, type) {
  var type = type || "overlay";
  var uri = gIOS.newURI(chromeURL, null, null);
  var present = false, elem;

  var overlays = (type == "overlay") ?
      gCR.getXULOverlays(uri) : gCR.getStyleOverlays(uri);

  // We shouldn't be allowed to register overlays nor styles
  if (overlays.hasMoreElements()) {
    if (type == "styles")
      do_throw("Style Registered: " + chromeURL);
    else
      do_throw("Overlay Registered: " + chromeURL);
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
  test_mapping("chrome://testOverride/content", 'file:///test1/override')

  // Test Not-Adding Overlays
  test_no_overlays("chrome://test1/content/overlay.xul",
                   "chrome://test1/content/test1.xul");

  // Test Not-Adding Styles
  test_no_overlays("chrome://test1/content/style.xul",
                   "chrome://test1/content/test1.css", "styles");


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
  testManifest(UNPACKAGED_ADDON, gIOS.newFileURI(UNPACKAGED_ADDON).spec);

  // Test a packaged addon
  testManifest(PACKAGED_ADDON, "jar:" + gIOS.newFileURI(PACKAGED_ADDON).spec + "!/");
}
