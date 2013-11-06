/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { data } = require("sdk/self");
const { Symbiont } = require("sdk/content/symbiont");

exports["test:direct communication with trusted document"] = function(assert, done) {
  let worker = Symbiont({
    contentURL: data.url("test-trusted-document.html")
  });

  worker.port.on('document-to-addon', function (arg) {
    assert.equal(arg, "ok", "Received an event from the document");
    worker.destroy();
    done();
  });
  worker.port.emit('addon-to-document', 'ok');
};

exports["test:`addon` is not available when a content script is set"] = function(assert, done) {
  let worker = Symbiont({
    contentURL: data.url("test-trusted-document.html"),
    contentScript: "new " + function ContentScriptScope() {
      self.port.emit("cs-to-addon", "addon" in unsafeWindow);
    }
  });

  worker.port.on('cs-to-addon', function (hasAddon) {
    assert.equal(hasAddon, false,
      "`addon` is not available");
    worker.destroy();
    done();
  });
};

require("sdk/test/runner").runTestsFromModule(module);
