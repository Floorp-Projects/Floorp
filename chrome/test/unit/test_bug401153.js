/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

var MANIFESTS = [
  do_get_file("data/test_bug401153.manifest")
];

registerManifests(MANIFESTS);

Components.utils.import("resource://testing-common/AppInfo.jsm", this);
updateAppInfo({
  name: "XPCShell",
  ID: "{39885e5f-f6b4-4e2a-87e5-6259ecf79011}",
  version: "5",
  platformVersion: "1.9",
});

var gIOS = Cc["@mozilla.org/network/io-service;1"]
            .getService(Ci.nsIIOService);
var chromeReg = Cc["@mozilla.org/chrome/chrome-registry;1"]
                 .getService(Ci.nsIChromeRegistry);
chromeReg.checkForNewChrome();

var rph = gIOS.getProtocolHandler("resource")
              .QueryInterface(Ci.nsIResProtocolHandler);

function test_succeeded_mapping(namespace, target)
{
  try {
    do_check_true(rph.hasSubstitution(namespace));
    var uri = gIOS.newURI("resource://" + namespace, null, null);
    dump("### checking for " + target + ", getting " + rph.resolveURI(uri) + "\n");
    do_check_eq(rph.resolveURI(uri), target);
  }
  catch (ex) {
    dump(ex + "\n");
    do_throw(namespace);
  }
}

function test_failed_mapping(namespace)
{
  do_check_false(rph.hasSubstitution(namespace));
}

function run_test()
{
  var data = gIOS.newFileURI(do_get_file("data")).spec;
  test_succeeded_mapping("test1", data + "test1/");
  test_succeeded_mapping("test3", "jar:" + data + "test3.jar!/resources/");
  test_failed_mapping("test4");
  test_succeeded_mapping("test5", data + "test5/");
}
