/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Page } = require("sdk/page-worker");
const { data } = require("sdk/self");

exports["test page load"] = function(assert, done) {
  let page = Page({
    contentURL: data.url("page.html")
  });

  page.port.on("load", function(check) {
    assert.equal(check, "ok", "saw the load message");
    page.destroy();
    done();
  });
};

exports["test postMessage"] = function(assert, done) {
  let page = Page({
    contentURL: data.url("page.html"),
    onMessage: function(msg) {
      if (msg == "pong") {
        assert.ok(true, "saw the second message");
        page.destroy();
        done();
        return;
      }

      assert.equal(msg, "first message", "saw the first message");
      this.postMessage("ping");
    }
  });
};

exports["test port"] = function(assert, done) {
  let page = Page({
    contentURL: data.url("page.html")
  });

  page.port.on("pong", function() {
    assert.ok(true, "saw the response");
    page.destroy();
    done();
  });

  page.port.emit("ping");
};

require("sdk/test/runner").runTestsFromModule(module);
