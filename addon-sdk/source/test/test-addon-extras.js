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

function getUnsafeWindow (win) {
  return win.wrappedJSObject || win;
}

require("sdk/test").run(exports);
