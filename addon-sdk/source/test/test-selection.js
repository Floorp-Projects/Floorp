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
const tabs = require("sdk/tabs");
const { setTabURL } = require("sdk/tabs/utils");
const { getActiveTab, getTabContentWindow, closeTab } = require("sdk/tabs/utils")
const { getMostRecentBrowserWindow } = require("sdk/window/utils");
const { open: openNewWindow } = require("sdk/window/helpers");
const { Loader } = require("sdk/test/loader");
const { setTimeout } = require("sdk/timers");
const { Cu } = require("chrome");
const { merge } = require("sdk/util/object");
const { isPrivate } = require("sdk/private-browsing");
const events = require("sdk/system/events");
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
 * Close the Active Tab
 */
function close(window) {
  if (window && window.top && typeof(window.top).close === "function") {
    window.top.close();
  } else {
    // Here we assuming that the most recent browser window is the one we're
    // doing the test, and the active tab is the one we just opened.
    let tab = getActiveTab(getMostRecentBrowserWindow());

    closeTab(tab);
  }
}

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
 * Returns the frame's window once the document is loaded
 */
function getFrameWindow(window) {
  let { promise, resolve } = defer();

  let frame = window.frames[0];
  let { document } = frame;

  frame.focus();

  if (document.readyState === "complete")
    return frame;

  document.addEventListener("readystatechange", function readystate() {
    if (this.readyState === "complete") {
      this.removeEventListener("readystatechange", readystate);
      frame.focus();
      resolve(frame);
    }
  });

  return promise;
}

/**
 * Hide the frame in order to destroy the selection object, and show it again
 * after ~500 msec, to give time to attach the code on `document-shown`
 * notification.
 * In the process, call `Cu.forgeGC` to ensure that the `document-shown` code
 * is not garbaged.
 */
function hideAndShowFrame(window) {
  let { promise, resolve } = defer();
  let iframe = window.document.querySelector("iframe");

  iframe.style.display = "none";

  Cu.forceGC();

  setTimeout(function() {
    events.on("document-shown", function shown(event) {
      if (iframe.contentWindow !== event.subject.defaultView)
        return;

      events.off("document-shown", shown);
      setTimeout(resolve, 0, window);
    }, true);

    iframe.style.display = "";
  }, 0)

  return promise;
}

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

/**
 * Creates empty ranges and add them to selections
 */
function createEmptySelections(window) {
  selectAllDivs(window);

  let selection = window.getSelection();

  for (let i = 0; i < selection.rangeCount; i++)
    selection.getRangeAt(i).collapse(true);
}

// Test cases

exports["test No Selection"] = function(assert, done) {
  let loader = Loader(module);
  let selection = loader.require("sdk/selection");

  open(URL).then(function() {

    assert.equal(selection.isContiguous, false,
      "selection.isContiguous without selection works.");

    assert.strictEqual(selection.text, null,
      "selection.text without selection works.");

    assert.strictEqual(selection.html, null,
      "selection.html without selection works.");

    let selectionCount = 0;
    for each (let sel in selection)
      selectionCount++;

    assert.equal(selectionCount, 0,
      "No iterable selections");

  }).then(close).then(loader.unload).then(done, assert.fail);
};

exports["test Single DOM Selection"] = function(assert, done) {
  let loader = Loader(module);
  let selection = loader.require("sdk/selection");

  open(URL).then(selectFirstDiv).then(function() {

    assert.equal(selection.isContiguous, true,
      "selection.isContiguous with single DOM Selection works.");

    assert.equal(selection.text, "foo",
      "selection.text with single DOM Selection works.");

    assert.equal(selection.html, "<div>foo</div>",
      "selection.html with single DOM Selection works.");

    let selectionCount = 0;
    for each (let sel in selection) {
      selectionCount++;

      assert.equal(sel.text, "foo",
        "iterable selection.text with single DOM Selection works.");

      assert.equal(sel.html, "<div>foo</div>",
        "iterable selection.html with single DOM Selection works.");
    }

    assert.equal(selectionCount, 1,
      "One iterable selection");

  }).then(close).then(loader.unload).then(done, assert.fail);
};

exports["test Multiple DOM Selection"] = function(assert, done) {
  let loader = Loader(module);
  let selection = loader.require("sdk/selection");

  open(URL).then(selectAllDivs).then(function() {
    let expectedText = ["foo", "and"];
    let expectedHTML = ["<div>foo</div>", "<div>and</div>"];

    assert.equal(selection.isContiguous, false,
      "selection.isContiguous with multiple DOM Selection works.");

    assert.equal(selection.text, expectedText[0],
      "selection.text with multiple DOM Selection works.");

    assert.equal(selection.html, expectedHTML[0],
      "selection.html with multiple DOM Selection works.");

    let selectionCount = 0;
    for each (let sel in selection) {
      assert.equal(sel.text, expectedText[selectionCount],
        "iterable selection.text with multiple DOM Selection works.");

      assert.equal(sel.html, expectedHTML[selectionCount],
        "iterable selection.text with multiple DOM Selection works.");

      selectionCount++;
    }

    assert.equal(selectionCount, 2,
      "Two iterable selections");

  }).then(close).then(loader.unload).then(done, assert.fail);
};

exports["test Textarea Selection"] = function(assert, done) {
  let loader = Loader(module);
  let selection = loader.require("sdk/selection");

  open(URL).then(selectTextarea).then(function() {

    assert.equal(selection.isContiguous, true,
      "selection.isContiguous with Textarea Selection works.");

    assert.equal(selection.text, "noodles",
      "selection.text with Textarea Selection works.");

    assert.strictEqual(selection.html, null,
      "selection.html with Textarea Selection works.");

    let selectionCount = 0;
    for each (let sel in selection) {
      selectionCount++;

      assert.equal(sel.text, "noodles",
        "iterable selection.text with Textarea Selection works.");

      assert.strictEqual(sel.html, null,
        "iterable selection.html with Textarea Selection works.");
    }

    assert.equal(selectionCount, 1,
      "One iterable selection");

  }).then(close).then(loader.unload).then(done, assert.fail);
};

exports["test Set Text in Multiple DOM Selection"] = function(assert, done) {
  let loader = Loader(module);
  let selection = loader.require("sdk/selection");

  open(URL).then(selectAllDivs).then(function() {
    let expectedText = ["bar", "and"];
    let expectedHTML = ["bar", "<div>and</div>"];

    selection.text = "bar";

    assert.equal(selection.text, expectedText[0],
      "set selection.text with single DOM Selection works.");

    assert.equal(selection.html, expectedHTML[0],
      "selection.html with single DOM Selection works.");

    let selectionCount = 0;
    for each (let sel in selection) {

      assert.equal(sel.text, expectedText[selectionCount],
        "iterable selection.text with multiple DOM Selection works.");

      assert.equal(sel.html, expectedHTML[selectionCount],
        "iterable selection.html with multiple DOM Selection works.");

      selectionCount++;
    }

    assert.equal(selectionCount, 2,
      "Two iterable selections");

  }).then(close).then(loader.unload).then(done, assert.fail);
};

exports["test Set HTML in Multiple DOM Selection"] = function(assert, done) {
  let loader = Loader(module);
  let selection = loader.require("sdk/selection");

  open(URL).then(selectAllDivs).then(function() {
    let html = "<span>b<b>a</b>r</span>";

    let expectedText = ["bar", "and"];
    let expectedHTML = [html, "<div>and</div>"];

    selection.html = html;

    assert.equal(selection.text, expectedText[0],
      "set selection.text with DOM Selection works.");

    assert.equal(selection.html, expectedHTML[0],
      "selection.html with DOM Selection works.");

    let selectionCount = 0;
    for each (let sel in selection) {

      assert.equal(sel.text, expectedText[selectionCount],
        "iterable selection.text with multiple DOM Selection works.");

      assert.equal(sel.html, expectedHTML[selectionCount],
        "iterable selection.html with multiple DOM Selection works.");

      selectionCount++;
    }

    assert.equal(selectionCount, 2,
      "Two iterable selections");

  }).then(close).then(loader.unload).then(done, assert.fail);
};

exports["test Set HTML as text in Multiple DOM Selection"] = function(assert, done) {
  let loader = Loader(module);
  let selection = loader.require("sdk/selection");

  open(URL).then(selectAllDivs).then(function() {
    let text = "<span>b<b>a</b>r</span>";
    let html = "&lt;span&gt;b&lt;b&gt;a&lt;/b&gt;r&lt;/span&gt;";

    let expectedText = [text, "and"];
    let expectedHTML = [html, "<div>and</div>"];

    selection.text = text;

    assert.equal(selection.text, expectedText[0],
      "set selection.text with DOM Selection works.");

    assert.equal(selection.html, expectedHTML[0],
      "selection.html with DOM Selection works.");

    let selectionCount = 0;
    for each (let sel in selection) {

      assert.equal(sel.text, expectedText[selectionCount],
        "iterable selection.text with multiple DOM Selection works.");

      assert.equal(sel.html, expectedHTML[selectionCount],
        "iterable selection.html with multiple DOM Selection works.");

      selectionCount++;
    }

    assert.equal(selectionCount, 2,
      "Two iterable selections");

  }).then(close).then(loader.unload).then(done, assert.fail);
};

exports["test Set Text in Textarea Selection"] = function(assert, done) {
  let loader = Loader(module);
  let selection = loader.require("sdk/selection");

  open(URL).then(selectTextarea).then(function() {

    let text = "bar";

    selection.text = text;

    assert.equal(selection.text, text,
      "set selection.text with Textarea Selection works.");

    assert.strictEqual(selection.html, null,
      "selection.html with Textarea Selection works.");

    let selectionCount = 0;
    for each (let sel in selection) {
      selectionCount++;

      assert.equal(sel.text, text,
        "iterable selection.text with Textarea Selection works.");

      assert.strictEqual(sel.html, null,
        "iterable selection.html with Textarea Selection works.");
    }

    assert.equal(selectionCount, 1,
      "One iterable selection");

  }).then(close).then(loader.unload).then(done, assert.fail);
};

exports["test Set HTML in Textarea Selection"] = function(assert, done) {
  let loader = Loader(module);
  let selection = loader.require("sdk/selection");

  open(URL).then(selectTextarea).then(function() {

    let html = "<span>b<b>a</b>r</span>";

    // Textarea can't have HTML so set `html` property is equals to set `text`
    // property
    selection.html = html;

    assert.equal(selection.text, html,
      "set selection.text with Textarea Selection works.");

    assert.strictEqual(selection.html, null,
      "selection.html with Textarea Selection works.");

    let selectionCount = 0;
    for each (let sel in selection) {
      selectionCount++;

      assert.equal(sel.text, html,
        "iterable selection.text with Textarea Selection works.");

      assert.strictEqual(sel.html, null,
        "iterable selection.html with Textarea Selection works.");
    }

    assert.equal(selectionCount, 1,
      "One iterable selection");

  }).then(close).then(loader.unload).then(done, assert.fail);
};

exports["test Empty Selections"] = function(assert, done) {
  let loader = Loader(module);
  let selection = loader.require("sdk/selection");

  open(URL).then(createEmptySelections).then(function(){
    assert.equal(selection.isContiguous, false,
      "selection.isContiguous with empty selections works.");

    assert.strictEqual(selection.text, null,
      "selection.text with empty selections works.");

    assert.strictEqual(selection.html, null,
      "selection.html with empty selections works.");

    let selectionCount = 0;
    for each (let sel in selection)
      selectionCount++;

    assert.equal(selectionCount, 0,
      "No iterable selections");

  }).then(close).then(loader.unload).then(done, assert.fail);
}


exports["test No Selection Exception"] = function(assert, done) {
  const NO_SELECTION = /It isn't possible to change the selection/;

  let loader = Loader(module);
  let selection = loader.require("sdk/selection");

  open(URL).then(function() {

    // We're trying to change a selection when there is no selection
    assert.throws(function() {
      selection.text = "bar";
    }, NO_SELECTION);

    assert.throws(function() {
      selection.html = "bar";
    }, NO_SELECTION);

  }).then(close).then(loader.unload).then(done, assert.fail);
};

exports["test for...of without selections"] = function(assert, done) {
  let loader = Loader(module);
  let selection = loader.require("sdk/selection");

  open(URL).then(function() {
    let selectionCount = 0;

    for (let sel of selection)
      selectionCount++;

    assert.equal(selectionCount, 0,
      "No iterable selections");

  }).then(close).then(loader.unload).then(null, function(error) {
    // iterable are not supported yet in Firefox 16, for example, but
    // they are in Firefox 17.
    if (error.message.indexOf("is not iterable") > -1)
      assert.pass("`iterable` method not supported in this application");
    else
      assert.fail(error);
  }).then(done, assert.fail);
}

exports["test for...of with selections"] = function(assert, done) {
  let loader = Loader(module);
  let selection = loader.require("sdk/selection");

  open(URL).then(selectAllDivs).then(function(){
    let expectedText = ["foo", "and"];
    let expectedHTML = ["<div>foo</div>", "<div>and</div>"];

    let selectionCount = 0;

    for (let sel of selection) {
      assert.equal(sel.text, expectedText[selectionCount],
        "iterable selection.text with for...of works.");

      assert.equal(sel.html, expectedHTML[selectionCount],
        "iterable selection.text with for...of works.");

      selectionCount++;
    }

    assert.equal(selectionCount, 2,
      "Two iterable selections");

  }).then(close).then(loader.unload).then(null, function(error) {
    // iterable are not supported yet in Firefox 16, for example, but
    // they are in Firefox 17.
    if (error.message.indexOf("is not iterable") > -1)
      assert.pass("`iterable` method not supported in this application");
    else
      assert.fail(error);
  }).then(done, assert.fail)
}

exports["test Selection Listener"] = function(assert, done) {
  let loader = Loader(module);
  let selection = loader.require("sdk/selection");

  selection.once("select", function() {
    assert.equal(selection.text, "fo");
    done();
  });

  open(URL).then(selectContentFirstDiv).
    then(dispatchSelectionEvent).
    then(close).
    then(loader.unload, assert.fail);
};

exports["test Textarea OnSelect Listener"] = function(assert, done) {
  let loader = Loader(module);
  let selection = loader.require("sdk/selection");

  selection.once("select", function() {
    assert.equal(selection.text, "noodles");
    done();
  });

  open(URL).then(selectTextarea).
    then(dispatchOnSelectEvent).
    then(close).
    then(loader.unload, assert.fail);
};

exports["test Selection listener removed on unload"] = function(assert, done) {
  let loader = Loader(module);
  let selection = loader.require("sdk/selection");

  selection.once("select", function() {
    assert.fail("Shouldn't be never called");
  });

  loader.unload();

  assert.pass();

  open(URL).
    then(selectContentFirstDiv).
    then(dispatchSelectionEvent).
    then(close).
    then(done, assert.fail);
};

exports["test Textarea onSelect Listener removed on unload"] = function(assert, done) {
  let loader = Loader(module);
  let selection = loader.require("sdk/selection");

  selection.once("select", function() {
    assert.fail("Shouldn't be never called");
  });

  loader.unload();

  assert.pass();

  open(URL).
    then(selectTextarea).
    then(dispatchOnSelectEvent).
    then(close).
    then(done, assert.fail);
};


exports["test Selection Listener on existing document"] = function(assert, done) {
  let loader = Loader(module);

  open(URL).then(function(window){
    let selection = loader.require("sdk/selection");

    selection.once("select", function() {
      assert.equal(selection.text, "fo");
      done();
    });

    return window;
  }).then(selectContentFirstDiv).
    then(dispatchSelectionEvent).
    then(close).
    then(loader.unload, assert.fail);
};


exports["test Textarea OnSelect Listener on existing document"] = function(assert, done) {
  let loader = Loader(module);

  open(URL).then(function(window){
    let selection = loader.require("sdk/selection");

    selection.once("select", function() {
      assert.equal(selection.text, "noodles");
      done();
    });

    return window;
  }).then(selectTextarea).
    then(dispatchOnSelectEvent).
    then(close).
    then(loader.unload, assert.fail);
};

exports["test Selection Listener on document reload"] = function(assert, done) {
  let loader = Loader(module);
  let selection = loader.require("sdk/selection");

  selection.once("select", function() {
    assert.equal(selection.text, "fo");
    done();
  });

  open(URL).
    then(reload).
    then(selectContentFirstDiv).
    then(dispatchSelectionEvent).
    then(close).
    then(loader.unload, assert.fail);
};

exports["test Textarea OnSelect Listener on document reload"] = function(assert, done) {
  let loader = Loader(module);
  let selection = loader.require("sdk/selection");

  selection.once("select", function() {
    assert.equal(selection.text, "noodles");
    done();
  });

  open(URL).
    then(reload).
    then(selectTextarea).
    then(dispatchOnSelectEvent).
    then(close).
    then(loader.unload, assert.fail);
};

exports["test Selection Listener on frame"] = function(assert, done) {
  let loader = Loader(module);
  let selection = loader.require("sdk/selection");

  selection.once("select", function() {
    assert.equal(selection.text, "fo");
    close();
    loader.unload();
    done();
  });

  open(FRAME_URL).
    then(hideAndShowFrame).
    then(getFrameWindow).
    then(selectContentFirstDiv).
    then(dispatchSelectionEvent).
    then(null, assert.fail);
};

exports["test Textarea onSelect Listener on frame"] = function(assert, done) {
  let loader = Loader(module);
  let selection = loader.require("sdk/selection");

  selection.once("select", function() {
    assert.equal(selection.text, "noodles");
    close();
    loader.unload();
    done();
  });

  open(FRAME_URL).
    then(hideAndShowFrame).
    then(getFrameWindow).
    then(selectTextarea).
    then(dispatchOnSelectEvent).
    then(null, assert.fail);
};


exports["test PBPW Selection Listener"] = function(assert, done) {
  let loader = Loader(module);
  let selection = loader.require("sdk/selection");

  selection.once("select", function() {
    assert.fail("Shouldn't be never called");
  });

  assert.pass();

  open(URL, {private: true}).
    then(selectContentFirstDiv).
    then(dispatchSelectionEvent).
    then(close).
    then(loader.unload).
    then(done, assert.fail);
};

exports["test PBPW Textarea OnSelect Listener"] = function(assert, done) {
  let loader = Loader(module);
  let selection = loader.require("sdk/selection");

  selection.once("select", function() {
    assert.fail("Shouldn't be never called");
  });

  assert.pass();

  open(URL, {private: true}).
    then(selectTextarea).
    then(dispatchOnSelectEvent).
    then(close).
    then(loader.unload).
    then(done, assert.fail);
};


exports["test PBPW Single DOM Selection"] = function(assert, done) {
  let loader = Loader(module);
  let selection = loader.require("sdk/selection");

  open(URL, {private: true}).then(selectFirstDiv).then(function(window) {

    assert.equal(selection.isContiguous, false,
      "selection.isContiguous with single DOM Selection in PBPW works.");

    assert.equal(selection.text, null,
      "selection.text with single DOM Selection in PBPW works.");

    assert.equal(selection.html, null,
      "selection.html with single DOM Selection in PBPW works.");

    let selectionCount = 0;
    for each (let sel in selection)
      selectionCount++;

    assert.equal(selectionCount, 0,
      "No iterable selection in PBPW");

    return window;
  }).then(close).then(loader.unload).then(done, assert.fail);
};

exports["test PBPW Textarea Selection"] = function(assert, done) {
  let loader = Loader(module);
  let selection = loader.require("sdk/selection");

  open(URL, {private: true}).then(selectTextarea).then(function(window) {

    assert.equal(selection.isContiguous, false,
      "selection.isContiguous with Textarea Selection in PBPW works.");

    assert.equal(selection.text, null,
      "selection.text with Textarea Selection in PBPW works.");

    assert.strictEqual(selection.html, null,
      "selection.html with Textarea Selection in PBPW works.");

    let selectionCount = 0;
    for each (let sel in selection) {
      selectionCount++;

      assert.equal(sel.text, null,
        "iterable selection.text with Textarea Selection in PBPW works.");

      assert.strictEqual(sel.html, null,
        "iterable selection.html with Textarea Selection in PBPW works.");
    }

    assert.equal(selectionCount, 0,
      "No iterable selection in PBPW");

    return window;
  }).then(close).then(loader.unload).then(done, assert.fail);
};

// TODO: test Selection Listener on long-held connection (Bug 661884)
//
//  I didn't find a way to do so with httpd, using `processAsync` I'm able to
//  Keep the connection but not to flush the buffer to the client in two steps,
//  that is what I need for this test (e.g. flush "Hello" to the client, makes
//  selection when the connection is still hold, and check that the listener
//  is executed before the server send "World" and close the connection).
//
//  Because this test is needed to the refactoring of context-menu as well, I
//  believe we will find a proper solution quickly.
/*
exports["test Selection Listener on long-held connection"] = function(assert, done) {

};
*/

// If the platform doesn't support the PBPW, we're replacing PBPW tests
if (!require("sdk/private-browsing/utils").isWindowPBSupported) {
  Object.keys(module.exports).forEach(function(key) {
    if (key.indexOf("test PBPW") === 0) {
      module.exports[key] = function Unsupported (assert) {
        assert.pass("Private Window Per Browsing is not supported on this platform.");
      }
    }
  });
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

require("test").run(exports)
