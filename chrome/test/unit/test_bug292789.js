/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

const MANIFESTS = [do_get_file("data/test_bug292789.manifest")];

registerManifests(MANIFESTS);

var gCR;

function check_accessibility(spec, desired) {
  var uri = Services.io.newURI(spec);
  var actual = gCR.allowContentToAccess(uri);
  Assert.equal(desired, actual);
}

function run_test() {
  gCR = Cc["@mozilla.org/chrome/chrome-registry;1"].getService(
    Ci.nsIXULChromeRegistry
  );
  gCR.checkForNewChrome();

  check_accessibility("chrome://test1/content/", false);
  check_accessibility("chrome://test1/content/foo.js", false);
  check_accessibility("chrome://test2/content/", true);
  check_accessibility("chrome://test2/content/foo.js", true);
  check_accessibility("chrome://test3/content/", false);
  check_accessibility("chrome://test3/content/foo.js", false);
  check_accessibility("chrome://test4/content/", true);
}
