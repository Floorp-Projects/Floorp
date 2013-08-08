/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Panel } = require("sdk/panel")
const { data } = require("sdk/self")

exports["test addon global"] = function(assert, done) {
  let panel = Panel({
    contentURL: //"data:text/html,now?",
                data.url("./index.html"),
    onMessage: function(message) {
      assert.pass("got message from panel script");
      panel.destroy();
      done();
    },
    onError: function(error) {
      asser.fail(Error("failed to recieve message"));
      done();
    }
  });
};

require("sdk/test/runner").runTestsFromModule(module);
