/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { PageMod } = require("sdk/page-mod");
const tabs = require("sdk/tabs");
const { startServerAsync } = require("./httpd");

const serverPort = 8099;
const TEST_TAB_URL = "about:mozilla";

exports.testCrossDomainIframe = function(assert, done) {
  let server = startServerAsync(serverPort);
  server.registerPathHandler("/iframe", function handle(request, response) {
    response.write("<html><body>foo</body></html>");
  });

  let pageMod = PageMod({
    include: TEST_TAB_URL,
    contentScript: "new " + function ContentScriptScope() {
      self.on("message", function (url) {
        let iframe = document.createElement("iframe");
        iframe.addEventListener("load", function onload() {
          iframe.removeEventListener("load", onload, false);
          self.postMessage(iframe.contentWindow.document.body.innerHTML);
        }, false);
        iframe.setAttribute("src", url);
        document.documentElement.appendChild(iframe);
      });
    },
    onAttach: function(w) {
      w.on("message", function (body) {
        assert.equal(body, "foo", "received iframe html content");
        pageMod.destroy();
        w.tab.close(function() {
          server.stop(done);
        });
      });

      w.postMessage("http://localhost:" + serverPort + "/iframe");
    }
  });

  tabs.open({
    url: TEST_TAB_URL,
    inBackground: true
  });
};

exports.testCrossDomainXHR = function(assert, done) {
  let server = startServerAsync(serverPort);
  server.registerPathHandler("/xhr", function handle(request, response) {
    response.write("foo");
  });

  let pageMod = PageMod({
    include: TEST_TAB_URL,
    contentScript: "new " + function ContentScriptScope() {
      self.on("message", function (url) {
        let request = new XMLHttpRequest();
        request.overrideMimeType("text/plain");
        request.open("GET", url, true);
        request.onload = function () {
          self.postMessage(request.responseText);
        };
        request.send(null);
      });
    },
    onAttach: function(w) {
      w.on("message", function (body) {
        assert.equal(body, "foo", "received XHR content");
        pageMod.destroy();
        w.tab.close(function() {
          server.stop(done);
        });
      });

      w.postMessage("http://localhost:" + serverPort + "/xhr");
    }
  });

  tabs.open({
    url: TEST_TAB_URL,
    inBackground: true
  });
};

require("sdk/test/runner").runTestsFromModule(module);
