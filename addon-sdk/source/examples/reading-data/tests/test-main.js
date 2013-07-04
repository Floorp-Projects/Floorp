/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var m = require("main");
var self = require("sdk/self");

exports.testReplace = function(test) {
  var input = "Hello World";
  var output = m.replaceMom(input);
  test.assertEqual(output, "Hello Mom");
  var callbacks = { quit: function() {} };

  // Make sure it doesn't crash...
  m.main({ staticArgs: {} }, callbacks);
};

exports.testID = function(test) {
  // The ID is randomly generated during tests, so we cannot compare it against
  // anything in particular.  Just assert that it is not empty.
  test.assert(self.id.length > 0);
  test.assertEqual(self.data.url("sample.html"),
                   "resource://reading-data-example-at-jetpack-dot-mozillalabs-dot-com/reading-data/data/sample.html");
};
