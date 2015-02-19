/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { create: makeFrame } = require("sdk/frame/utils");
const { window } = require("sdk/addon/window");
const { Loader } = require('sdk/test/loader');

exports.testMembranelessMode = function(assert, done) {
  const loader = Loader(module);
  const Worker = loader.require("sdk/content/worker").Worker;

  let url = "data:text/html;charset=utf-8," + encodeURIComponent(
    '<script>' +
    'function runTest() {' +
    '  assert(fuu.bar == 42, "Content-script objects should be accessible to content with' +
    '         the unsafe-content-script flag on.");' +
    '}' +
    '</script>'
  );

  let element = makeFrame(window.document, {
    nodeName: "iframe",
    type: "content",
    allowJavascript: true,
    allowPlugins: true,
    allowAuth: true,
    uri: url
  });

  element.addEventListener("DOMContentLoaded", onDOMReady, false);

  function onDOMReady() {
    let worker = Worker({
      window: element.contentWindow,
      contentScript:
        'new ' + function () {
          var assert = function assert(v, msg) {
            self.port.emit("assert", { assertion: v, msg: msg });
          }
          var done = function done() {
            self.port.emit("done");
          }
          window.wrappedJSObject.fuu = { bar: 42 };
          window.wrappedJSObject.assert = assert;
          window.wrappedJSObject.runTest();
          done();
        }
    });

    worker.port.on("done", () => {
      // cleanup
      element.parentNode.removeChild(element);
      worker.destroy();
      loader.unload();

      done();
    });

    worker.port.on("assert", function (data) {
      assert.ok(data.assertion, data.msg);
    });

  }
};

require("sdk/test/runner").runTestsFromModule(module);
