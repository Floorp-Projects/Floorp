/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var m = require("main");
var self = require("sdk/self");

exports.testMain = function(test) {
  var callbacks = { quit: function() {
    test.pass();
    test.done();
  } };

  test.waitUntilDone();
  // Make sure it doesn't crash...
  m.main({ staticArgs: {quitWhenDone: true} }, callbacks);
};

exports.testData = function(test) {
  test.assert(self.data.load("panel.js").length > 0);
};
