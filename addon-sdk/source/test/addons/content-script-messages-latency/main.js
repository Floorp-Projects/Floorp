/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { PageMod } = require("sdk/page-mod");
const tabs = require("sdk/tabs");
const { startServerAsync } = require("./httpd");
const { setTimeout } = require("sdk/timers");

const serverPort = 8099;

exports.testContentScriptLatencyRegression = function*(assert) {
  let server = startServerAsync(serverPort);
  server.registerPathHandler("/", function handle(request, response) {
    response.write(`<html>
      <head>
        <link rel="stylesheet" href="/slow.css">
      </head>
      <body>
        slow loading page...
      </body>
    </html>`);
  });

  server.registerPathHandler("/slow.css", function handle(request, response) {
    response.processAsync();
    response.setHeader('Content-Type', 'text/css', false);
    setTimeout(_ => {
      response.write("body { background: red; }");
      response.finish();
    }, 2000);
  });


  let pageMod;

  let worker = yield new Promise((resolve) => {
    pageMod = PageMod({
      include: "http://localhost:8099/",
      attachTo: "top",
      contentScriptWhen: "start",
      contentScript: "new " + function ContentScriptScope() {
        self.port.on("a-port-message", function () {
          self.port.emit("document-ready-state", document.readyState);
        });
      },
      onAttach: function(w) {
        resolve(w);
      }
    });

    tabs.open({
      url: "http://localhost:8099/",
      inBackground: true
    });
  });

  worker.port.emit("a-port-message");

  let waitForPortMessage = new Promise((resolve) => {
    worker.port.once("document-ready-state", (msg) => {
      resolve(msg);
    });
  });

  let documentReadyState = yield waitForPortMessage;

  assert.notEqual(
    "complete", documentReadyState,
    "content script received the port message when the page was still loading"
  );

  assert.notEqual(
    "uninitialized", documentReadyState,
    "content script should be frozen if document.readyState is still uninitialized"
  );

  assert.ok(
    ["loading", "interactive"].includes(documentReadyState),
    "content script message received with document.readyState was interactive or loading"
  );

  // Cleanup.
  pageMod.destroy();
  yield new Promise((resolve) => worker.tab.close(resolve));
  yield new Promise((resolve) => server.stop(resolve));
};

require("sdk/test/runner").runTestsFromModule(module);
