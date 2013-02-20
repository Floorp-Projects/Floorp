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

const { defer } = require("sdk/core/promise");
const tabs = require("sdk/tabs");
const { getActiveTab, getTabContentWindow, closeTab } = require("sdk/tabs/utils")
const { getMostRecentBrowserWindow } = require("sdk/window/utils");
const { Loader } = require("sdk/test/loader");
const { setTimeout } = require("sdk/timers");

// General purpose utility functions

/**
 * Opens the url given and return a promise, that will be resolved with the
 * content window when the document is ready.
 *
 * I believe this approach could be useful in most of our unit test, that
 * requires to open a tab and need to access to its content.
 */
function open(url) {
  let { promise, resolve } = defer();

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
function close() {
  // Here we assuming that the most recent browser window is the one we're
  // doing the test, and the active tab is the one we just opened.
  closeTab(getActiveTab(getMostRecentBrowserWindow()));
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

  textarea.dispatchEvent(event)
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

  }).then(close).then(loader.unload).then(done);
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

  }).then(close).then(loader.unload).then(done);
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

  }).then(close).then(loader.unload).then(done);
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

  }).then(close).then(loader.unload).then(done);
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

  }).then(close).then(loader.unload).then(done);
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

  }).then(close).then(loader.unload).then(done);
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

  }).then(close).then(loader.unload).then(done);
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

  }).then(close).then(loader.unload).then(done);
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

  }).then(close).then(loader.unload).then(done);
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

  }).then(close).then(loader.unload).then(done);
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

  }).then(close).then(loader.unload).then(done);
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
  }).then(done);
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
  }).then(done)
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
    then(loader.unload);
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
    then(loader.unload);
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
    then(done)
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
    then(done)
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
    then(loader.unload)
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
    then(loader.unload)
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
    then(loader.unload);
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
    then(loader.unload);
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
