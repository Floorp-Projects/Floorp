/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Ci, Cu, Cc, components } = require("chrome");
const self = require("sdk/self");
const { before, after } = require("sdk/test/utils");
const fixtures = require("./fixtures");
const { Loader } = require("sdk/test/loader");
const { merge } = require("sdk/util/object");

exports["test changing result from addon extras in panel"] = function(assert, done) {
  let loader = Loader(module, null, null, {
    modules: {
      "sdk/self": merge({}, self, {
        data: merge({}, self.data, {url: fixtures.url})
      })
    }
  });

  const { Panel } = loader.require("sdk/panel");
  const { events } = loader.require("sdk/content/sandbox/events");
  const { on } = loader.require("sdk/event/core");
  const { isAddonContent } = loader.require("sdk/content/utils");

  var result = 1;
  var extrasVal = {
    test: function() {
      return result;
    }
  };

  on(events, "content-script-before-inserted", ({ window, worker }) => {
    assert.pass("content-script-before-inserted");

    if (isAddonContent({ contentURL: window.location.href })) {
      let extraStuff = Cu.cloneInto(extrasVal, window, {
        cloneFunctions: true
      });
      getUnsafeWindow(window).extras = extraStuff;

      assert.pass("content-script-before-inserted done!");
    }
  });

  let panel = Panel({
    contentURL: "./test-addon-extras.html"
  });

  panel.port.once("result1", (result) => {
    assert.equal(result, 1, "result is a number");
    result = true;
    panel.port.emit("get-result");
  });

  panel.port.once("result2", (result) => {
    assert.equal(result, true, "result is a boolean");
    loader.unload();
    done();
  });

  panel.port.emit("get-result");
}

exports["test window result from addon extras in panel"] = function*(assert) {
  let loader = Loader(module, null, null, {
    modules: {
      "sdk/self": merge({}, self, {
        data: merge({}, self.data, {url: fixtures.url})
      })
    }
  });

  const { Panel } = loader.require('sdk/panel');
  const { Page } = loader.require('sdk/page-worker');
  const { getActiveView } = loader.require("sdk/view/core");
  const { getDocShell } = loader.require('sdk/frame/utils');
  const { events } = loader.require("sdk/content/sandbox/events");
  const { on } = loader.require("sdk/event/core");
  const { isAddonContent } = loader.require("sdk/content/utils");

  // make a page worker and wait for it to load
  var page = yield new Promise(resolve => {
    assert.pass("Creating the background page");

    let page = Page({
      contentURL: "./test.html",
      contentScriptWhen: "end",
      contentScript: "self.port.emit('end', unsafeWindow.getTestURL() + '')"
    });

    page.port.once("end", (url) => {
      assert.equal(url, fixtures.url("./test.html"), "url is correct");
      resolve(page);
    });
  });
  assert.pass("Created the background page");

  var extrasVal = {
    test: function() {
      assert.pass("start test function");
      let frame = getActiveView(page);
      let window = getUnsafeWindow(frame.contentWindow);
      assert.equal(typeof window.getTestURL, "function", "window.getTestURL is a function");
      return window;
    }
  };

  on(events, "content-script-before-inserted", ({ window, worker }) => {
    let url = window.location.href;
    assert.pass("content-script-before-inserted " + url);

    if (isAddonContent({ contentURL: url })) {
      let extraStuff = Cu.cloneInto(extrasVal, window, {
        cloneFunctions: true
      });
      getUnsafeWindow(window).extras = extraStuff;

      assert.pass("content-script-before-inserted done!");
    }
  });

  let panel = Panel({
    contentURL: "./test-addon-extras-window.html"
  });


  yield new Promise(resolve => {
    panel.port.once("result1", (result) => {
      assert.equal(result, fixtures.url("./test.html"), "result1 is a window");
      resolve();
    });

    assert.pass("emit get-result");
    panel.port.emit("get-result");
  });

  page.destroy();


  page = yield new Promise(resolve => {
    let page = Page({
      contentURL: "./index.html",
      contentScriptWhen: "end",
      contentScript: "self.port.emit('end')"
    });
    page.port.once("end", () => resolve(page));
  });


  yield new Promise(resolve => {
    panel.port.once("result2", (result) => {
      assert.equal(result, fixtures.url("./index.html"), "result2 is a window");
      resolve();
    });

    assert.pass("emit get-result");
    panel.port.emit("get-result");
  });

  loader.unload();
}



function getUnsafeWindow (win) {
  return win.wrappedJSObject || win;
}

require("sdk/test").run(exports);
