/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

function test_uri(obj) {
  var uri = null;
  var failed = false;
  var message = "";
  try {
    uri = Services.io.newURI(obj.uri);
    if (!obj.result) {
      failed = true;
      message = obj.uri + " should not be accepted as a valid URI";
    }
  } catch (ex) {
    if (obj.result) {
      failed = true;
      message = obj.uri + " should be accepted as a valid URI";
    }
  }
  if (failed)
    do_throw(message);
  if (obj.result) {
    do_check_true(uri != null);
    do_check_eq(uri.spec, obj.uri);
  }
}

function run_test() {
  var tests = [
    {uri: "chrome://blah/content/blah.xul", result: true},
    {uri: "chrome://blah/content/:/blah/blah.xul", result: false},
    {uri: "chrome://blah/content/%252e./blah/blah.xul", result: false},
    {uri: "chrome://blah/content/%252e%252e/blah/blah.xul", result: false},
    {uri: "chrome://blah/content/blah.xul?param=%252e./blah/", result: true},
    {uri: "chrome://blah/content/blah.xul?param=:/blah/", result: true},
    {uri: "chrome://blah/content/blah.xul?param=%252e%252e/blah/", result: true},
  ];
  for (var i = 0; i < tests.length; ++i)
    test_uri(tests[i]);
}
