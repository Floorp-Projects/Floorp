/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const tabs = require("sdk/tabs"); // From addon-kit
const windowUtils = require("sdk/deprecated/window-utils");
const app = require("sdk/system/xul-app");
const { viewFor } = require("sdk/view/core");
const { modelFor } = require("sdk/model/core");
const { getTabId, isTab } = require("sdk/tabs/utils");
const { defer } = require("sdk/lang/functional");

exports["test behavior on close"] = function(assert, done) {
  tabs.open({
    url: "about:mozilla",
    onReady: function(tab) {
      assert.equal(tab.url, "about:mozilla", "Tab has the expected url");
      // if another test ends before closing a tab then index != 1 here
      assert.ok(tab.index >= 1, "Tab has the expected index, a value greater than 0");
      tab.close(function () {
        assert.equal(tab.url, undefined,
                     "After being closed, tab attributes are undefined (url)");
        assert.equal(tab.index, undefined,
                     "After being closed, tab attributes are undefined (index)");
        if (app.is("Firefox")) {
          // Ensure that we can call destroy multiple times without throwing;
          // Fennec doesn't use this internal utility
          tab.destroy();
          tab.destroy();
        }

        done();
      });
    }
  });
};

exports["test viewFor(tab)"] = (assert, done) => {
  // Note we defer handlers as length collection is updated after
  // handler is invoked, so if test is finished before counnts are
  // updated wrong length will show up in followup tests.
  tabs.once("open", defer(tab => {
    const view = viewFor(tab);
    assert.ok(view, "view is returned");
    assert.equal(getTabId(view), tab.id, "tab has a same id");

    tab.close(defer(done));
  }));

  tabs.open({ url: "about:mozilla" });
};

exports["test modelFor(xulTab)"] = (assert, done) => {
  tabs.open({
    url: "about:mozilla",
    onReady: tab => {
      const view = viewFor(tab);
      assert.ok(view, "view is returned");
      assert.ok(isTab(view), "view is underlaying tab");
      assert.equal(getTabId(view), tab.id, "tab has a same id");
      assert.equal(modelFor(view), tab, "modelFor(view) is SDK tab");

      tab.close(defer(done));
    }
  });
};

exports["test tab.readyState"] = (assert, done) => {
  tabs.open({
    url: "data:text/html;charset=utf-8,test_readyState",
    onOpen: (tab) => {
      assert.notEqual(["uninitialized", "loading"].indexOf(tab.readyState), -1,
        "tab is either uninitialized or loading when onOpen");
    },
    onReady: (tab) => {
      assert.notEqual(["interactive", "complete"].indexOf(tab.readyState), -1,
        "tab is either interactive or complete when onReady");
    },
    onLoad: (tab) => {
      assert.equal(tab.readyState, "complete", "tab is complete onLoad");
      tab.close(defer(done));
    }
  });
}

// require("sdk/test").run(exports);
