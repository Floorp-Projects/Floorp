/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Loader } = require("sdk/test/loader");
const hiddenFrames = require("sdk/frame/hidden-frame");
const { HiddenFrame } = hiddenFrames;

exports["test Frame"] = function(assert, done) {
  let url = "data:text/html;charset=utf-8,<!DOCTYPE%20html>";

  let hiddenFrame = hiddenFrames.add(HiddenFrame({
    onReady: function () {
      assert.equal(this.element.contentWindow.location, "about:blank",
                       "HiddenFrame loads about:blank by default.");

      function onDOMReady() {
        hiddenFrame.element.removeEventListener("DOMContentLoaded", onDOMReady,
                                                false);
        assert.equal(hiddenFrame.element.contentWindow.location, url,
                         "HiddenFrame loads the specified content.");
        done();
      }

      this.element.addEventListener("DOMContentLoaded", onDOMReady, false);
      this.element.setAttribute("src", url);
    }
  }));
};

exports["test frame removed properly"] = function(assert, done) {
  let url = "data:text/html;charset=utf-8,<!DOCTYPE%20html>";

  let hiddenFrame = hiddenFrames.add(HiddenFrame({
    onReady: function () {
      let frame = this.element;
      assert.ok(frame.parentNode, "frame has a parent node");
      hiddenFrames.remove(hiddenFrame);
      assert.ok(!frame.parentNode, "frame no longer has a parent node");
      done();
    }
  }));
};

exports["test unload detaches panels"] = function(assert, done) {
  let loader = Loader(module);
  let { add, remove, HiddenFrame } = loader.require("sdk/frame/hidden-frame");
  let frames = []

  function ready() {
    frames.push(this.element);
    if (frames.length === 2) complete();
  }

  add(HiddenFrame({ onReady: ready }));
  add(HiddenFrame({ onReady: ready }));

  function complete() {
    frames.forEach(function(frame) {
      assert.ok(frame.parentNode, "frame is in the document");
    })
    loader.unload();
    frames.forEach(function(frame) {
      assert.ok(!frame.parentNode, "frame isn't in the document'");
    });
    done();
  }
};

require("test").run(exports);
