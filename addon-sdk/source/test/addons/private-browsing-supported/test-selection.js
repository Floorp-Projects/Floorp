/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const HTML = "<html>\
  <body>\
    <div>foo</div>\
    <div>and</div>\
    <textarea>noodles</textarea>\
  </body>\
</html>";

const URL = "data:text/html;charset=utf-8," + encodeURIComponent(HTML);

const FRAME_HTML = "<iframe src='" + URL + "'><iframe>";
const FRAME_URL = "data:text/html;charset=utf-8," + encodeURIComponent(FRAME_HTML);

const { defer } = require("sdk/core/promise");
const { browserWindows } = require("sdk/windows");
const tabs = require("sdk/tabs");
const { setTabURL, getActiveTab, getTabContentWindow, closeTab, getTabs,
  getTabTitle } = require("sdk/tabs/utils");
const { getMostRecentBrowserWindow, isFocused } = require("sdk/window/utils");
const { open: openNewWindow, close: closeWindow, focus } = require("sdk/window/helpers");
const { Loader } = require("sdk/test/loader");
const { merge } = require("sdk/util/object");
const { isPrivate } = require("sdk/private-browsing");

// General purpose utility functions

/**
 * Opens the url given and return a promise, that will be resolved with the
 * content window when the document is ready.
 *
 * I believe this approach could be useful in most of our unit test, that
 * requires to open a tab and need to access to its content.
 */
function open(url, options) {
  let { promise, resolve } = defer();

  if (options && typeof(options) === "object") {
    openNewWindow("", {
      features: merge({ toolbar: true }, options)
    }).then(function(chromeWindow) {
      if (isPrivate(chromeWindow) !== !!options.private)
        throw new Error("Window should have Private set to " + !!options.private);

      let tab = getActiveTab(chromeWindow);

      tab.linkedBrowser.addEventListener("load", function ready(event) {
        let { document } = getTabContentWindow(tab);

        if (document.readyState === "complete" && document.URL === url) {
          this.removeEventListener(event.type, ready);

          if (options.title)
            document.title = options.title;

          resolve(document.defaultView);
        }
      }, true);

      setTabURL(tab, url);
    });

    return promise;
  };

  tabs.open({
    url: url,
    onReady: function(tab) {
      // Unfortunately there is no way to get a XUL Tab from SDK Tab on Firefox,
      // only on Fennec. We should implement `tabNS` also on Firefox in order
      // to have that.

      // Here we assuming that the most recent browser window is the one we're
      // doing the test, and the active tab is the one we just opened.
      let window = getTabContentWindow(getActiveTab(getMostRecentBrowserWindow()));

      resolve(window);
    }
  });

  return promise;
};

/**
 * Reload the window given and return a promise, that will be resolved with the
 * content window after a small delay.
 */
function reload(window) {
  let { promise, resolve } = defer();

  // Here we assuming that the most recent browser window is the one we're
  // doing the test, and the active tab is the one we just opened.
  let tab = tabs.activeTab;

  tab.once("ready", function () {
    resolve(window);
  });

  window.location.reload(true);

  return promise;
}

// Selection's unit test utility function

/**
 * Select the first div in the page, adding the range to the selection.
 */
function selectFirstDiv(window) {
  let div = window.document.querySelector("div");
  let selection = window.getSelection();
  let range = window.document.createRange();

  if (selection.rangeCount > 0)
    selection.removeAllRanges();

  range.selectNode(div);
  selection.addRange(range);

  return window;
}

/**
 * Select all divs in the page, adding the ranges to the selection.
 */
function selectAllDivs(window) {
  let divs = window.document.getElementsByTagName("div");
  let selection = window.getSelection();

  if (selection.rangeCount > 0)
    selection.removeAllRanges();

  for (let i = 0; i < divs.length; i++) {
    let range = window.document.createRange();

    range.selectNode(divs[i]);
    selection.addRange(range);
  }

  return window;
}

/**
 * Select the textarea content
 */
function selectTextarea(window) {
  let selection = window.getSelection();
  let textarea = window.document.querySelector("textarea");

  if (selection.rangeCount > 0)
    selection.removeAllRanges();

  textarea.setSelectionRange(0, textarea.value.length);
  textarea.focus();

  return window;
}

/**
 * Select the content of the first div
 */
function selectContentFirstDiv(window) {
  let div = window.document.querySelector("div");
  let selection = window.getSelection();
  let range = window.document.createRange();

  if (selection.rangeCount > 0)
    selection.removeAllRanges();

  range.selectNodeContents(div);
  selection.addRange(range);

  return window;
}

/**
 * Dispatch the selection event for the selection listener added by
 * `nsISelectionPrivate.addSelectionListener`
 */
function dispatchSelectionEvent(window) {
  // We modify the selection in order to dispatch the selection's event, by
  // contract the selection by one character. So if the text selected is "foo"
  // will be "fo".
  window.getSelection().modify("extend", "backward", "character");

  return window;
}

/**
 * Dispatch the selection event for the selection listener added by
 * `window.onselect` / `window.addEventListener`
 */
function dispatchOnSelectEvent(window) {
  let { document } = window;
  let textarea = document.querySelector("textarea");
  let event = document.createEvent("UIEvents");

  event.initUIEvent("select", true, true, window, 1);

  textarea.dispatchEvent(event);

  return window;
}

// Test cases

exports["test PWPB Selection Listener"] = function(assert, done) {
  let loader = Loader(module);
  let selection = loader.require("sdk/selection");

  open(URL, {private: true, title: "PWPB Selection Listener"}).
    then(function(window) {
      selection.once("select", function() {
        assert.equal(browserWindows.length, 2, "there should be only two windows open.");
        assert.equal(getTabs().length, 2, "there should be only two tabs open: '" +
                     getTabs().map(tab => getTabTitle(tab)).join("', '") +
                     "'."
        );

        // window should be focused, but force the focus anyhow.. see bug 841823
        focus(window).then(function() {
          // check state of window
          assert.ok(isFocused(window), "the window is focused");
          assert.ok(isPrivate(window), "the window should be a private window");

          assert.equal(selection.text, "fo");

          closeWindow(window).
            then(loader.unload).
            then(done).
            then(null, assert.fail);
        });
      });
      return window;
    }).
    then(selectContentFirstDiv).
    then(dispatchSelectionEvent).
    then(null, assert.fail);
};

exports["test PWPB Textarea OnSelect Listener"] = function(assert, done) {
  let loader = Loader(module);
  let selection = loader.require("sdk/selection");

  open(URL, {private: true, title: "PWPB OnSelect Listener"}).
    then(function(window) {
      selection.once("select", function() {
        assert.equal(browserWindows.length, 2, "there should be only two windows open.");
        assert.equal(getTabs().length, 2, "there should be only two tabs open: '" +
                     getTabs().map(tab => getTabTitle(tab)).join("', '") +
                     "'."
        );

        // window should be focused, but force the focus anyhow.. see bug 841823
        focus(window).then(function() {
          assert.equal(selection.text, "noodles");

          closeWindow(window).
            then(loader.unload).
            then(done).
            then(null, assert.fail);
        });
      });
      return window;
    }).
    then(selectTextarea).
    then(dispatchOnSelectEvent).
    then(null, assert.fail);
};

exports["test PWPB Single DOM Selection"] = function(assert, done) {
  let loader = Loader(module);
  let selection = loader.require("sdk/selection");

  open(URL, {private: true, title: "PWPB Single DOM Selection"}).
    then(selectFirstDiv).
    then(focus).then(function(window) {
      assert.equal(selection.isContiguous, true,
        "selection.isContiguous with single DOM Selection works.");

      assert.equal(selection.text, "foo",
        "selection.text with single DOM Selection works.");

      assert.equal(selection.html, "<div>foo</div>",
        "selection.html with single DOM Selection works.");

      let selectionCount = 0;
      for (let sel of selection) {
        selectionCount++;

        assert.equal(sel.text, "foo",
          "iterable selection.text with single DOM Selection works.");

        assert.equal(sel.html, "<div>foo</div>",
          "iterable selection.html with single DOM Selection works.");
      }

      assert.equal(selectionCount, 1,
        "One iterable selection");

      return closeWindow(window);
    }).then(loader.unload).then(done).then(null, assert.fail);
}

exports["test PWPB Textarea Selection"] = function(assert, done) {
  let loader = Loader(module);
  let selection = loader.require("sdk/selection");

  open(URL, {private: true, title: "PWPB Textarea Listener"}).
    then(selectTextarea).
    then(focus).
    then(function(window) {

      assert.equal(selection.isContiguous, true,
        "selection.isContiguous with Textarea Selection works.");

      assert.equal(selection.text, "noodles",
        "selection.text with Textarea Selection works.");

      assert.strictEqual(selection.html, null,
        "selection.html with Textarea Selection works.");

      let selectionCount = 0;
      for (let sel of selection) {
        selectionCount++;

        assert.equal(sel.text, "noodles",
          "iterable selection.text with Textarea Selection works.");

        assert.strictEqual(sel.html, null,
          "iterable selection.html with Textarea Selection works.");
      }

      assert.equal(selectionCount, 1,
        "One iterable selection");

      return closeWindow(window);
    }).then(loader.unload).then(done).then(null, assert.fail);
};

exports["test PWPB Set HTML in Multiple DOM Selection"] = function(assert, done) {
  let loader = Loader(module);
  let selection = loader.require("sdk/selection");

  open(URL, {private: true, title: "PWPB Set HTML in Multiple DOM Selection"}).
    then(selectAllDivs).
    then(focus).
    then(function(window) {
      let html = "<span>b<b>a</b>r</span>";

      let expectedText = ["bar", "and"];
      let expectedHTML = [html, "<div>and</div>"];

      selection.html = html;

      assert.equal(selection.text, expectedText[0],
        "set selection.text with DOM Selection works.");

      assert.equal(selection.html, expectedHTML[0],
        "selection.html with DOM Selection works.");

      let selectionCount = 0;
      for (let sel of selection) {

        assert.equal(sel.text, expectedText[selectionCount],
          "iterable selection.text with multiple DOM Selection works.");

        assert.equal(sel.html, expectedHTML[selectionCount],
          "iterable selection.html with multiple DOM Selection works.");

        selectionCount++;
      }

      assert.equal(selectionCount, 2,
        "Two iterable selections");

      return closeWindow(window);
    }).then(loader.unload).then(done).then(null, assert.fail);
};

exports["test PWPB Set Text in Textarea Selection"] = function(assert, done) {
  let loader = Loader(module);
  let selection = loader.require("sdk/selection");

  open(URL, {private: true, title: "test PWPB Set Text in Textarea Selection"}).
    then(selectTextarea).
    then(focus).
    then(function(window) {

      let text = "bar";

      selection.text = text;

      assert.equal(selection.text, text,
        "set selection.text with Textarea Selection works.");

      assert.strictEqual(selection.html, null,
        "selection.html with Textarea Selection works.");

      let selectionCount = 0;
      for (let sel of selection) {
        selectionCount++;

        assert.equal(sel.text, text,
          "iterable selection.text with Textarea Selection works.");

        assert.strictEqual(sel.html, null,
          "iterable selection.html with Textarea Selection works.");
      }

      assert.equal(selectionCount, 1,
        "One iterable selection");

      return closeWindow(window);
    }).then(loader.unload).then(done).then(null, assert.fail);
};

// If the platform doesn't support the PBPW, we're replacing PBPW tests
if (!require("sdk/private-browsing/utils").isWindowPBSupported) {
  module.exports = {
    "test PBPW Unsupported": function Unsupported (assert) {
      assert.pass("Private Window Per Browsing is not supported on this platform.");
    }
  }
}

// If the module doesn't support the app we're being run in, require() will
// throw.  In that case, remove all tests above from exports, and add one dummy
// test that passes.
try {
  require("sdk/selection");
}
catch (err) {
  if (!/^Unsupported Application/.test(err.message))
    throw err;

  module.exports = {
    "test Unsupported Application": function Unsupported (assert) {
      assert.pass(err.message);
    }
  }
}
