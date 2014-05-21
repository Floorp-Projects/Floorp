/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Request } = require("sdk/request");

exports.testBootstrapExists = function (assert, done) {
  Request({
    url: "resource://gre/modules/sdk/bootstrap.js",
    onComplete: function (response) {
      if (response.text)
        assert.pass("the bootstrap file was found");
      done();
    }
  }).get();
};

require("sdk/test").run(exports);
