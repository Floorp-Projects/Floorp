/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
 'use strict';

require("sdk/context-menu");

const { defer } = require("sdk/core/promise");
const packaging = require('@loader/options');

// These should match the same constants in the module.
const OVERFLOW_THRESH_DEFAULT = 10;
const OVERFLOW_THRESH_PREF =
  "extensions.addon-sdk.context-menu.overflowThreshold";

const TEST_DOC_URL = module.uri.replace(/\.js$/, ".html");
const data = require("./fixtures");

const { TestHelper } = require("./context-menu/test-helper.js")

// Tests that when present the separator is placed before the separator from
// the old context-menu module
exports.testSeparatorPosition = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  // Create the old separator
  let oldSeparator = test.contextMenuPopup.ownerDocument.createElement("menuseparator");
  oldSeparator.id = "jetpack-context-menu-separator";
  test.contextMenuPopup.appendChild(oldSeparator);

  // Create an item.
  let item = new loader.cm.Item({ label: "item" });

  test.showMenu(null, function (popup) {
    assert.equal(test.contextMenuSeparator.nextSibling.nextSibling, oldSeparator,
                     "New separator should appear before the old one");
    test.contextMenuPopup.removeChild(oldSeparator);
    test.done();
  });
};

// Destroying items that were previously created should cause them to be absent
// from the menu.
exports.testConstructDestroy = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  // Create an item.
  let item = new loader.cm.Item({ label: "item" });
  assert.equal(item.parentMenu, loader.cm.contentContextMenu,
                   "item's parent menu should be correct");

  test.showMenu(null, function (popup) {

    // It should be present when the menu is shown.
    test.checkMenu([item], [], []);
    popup.hidePopup();

    // Destroy the item.  Multiple destroys should be harmless.
    item.destroy();
    item.destroy();
    test.showMenu(null, function (popup) {

      // It should be removed from the menu.
      test.checkMenu([item], [], [item]);
      test.done();
    });
  });
};


// Destroying an item twice should not cause an error.
exports.testDestroyTwice = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let item = new loader.cm.Item({ label: "item" });
  item.destroy();
  item.destroy();

  test.pass("Destroying an item twice should not cause an error.");
  test.done();
};


// CSS selector contexts should cause their items to be present in the menu
// when the menu is invoked on nodes that match the selectors.
exports.testSelectorContextMatch = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let item = new loader.cm.Item({
    label: "item",
    data: "item",
    context: loader.cm.SelectorContext("img")
  });

  test.withTestDoc(function (window, doc) {
    test.showMenu("#image", function (popup) {
      test.checkMenu([item], [], []);
      test.done();
    });
  });
};


// CSS selector contexts should cause their items to be present in the menu
// when the menu is invoked on nodes that have ancestors that match the
// selectors.
exports.testSelectorAncestorContextMatch = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let item = new loader.cm.Item({
    label: "item",
    data: "item",
    context: loader.cm.SelectorContext("a[href]")
  });

  test.withTestDoc(function (window, doc) {
    test.showMenu("#span-link", function (popup) {
      test.checkMenu([item], [], []);
      test.done();
    });
  });
};


// CSS selector contexts should cause their items to be absent from the menu
// when the menu is not invoked on nodes that match or have ancestors that
// match the selectors.
exports.testSelectorContextNoMatch = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let item = new loader.cm.Item({
    label: "item",
    data: "item",
    context: loader.cm.SelectorContext("img")
  });

  test.showMenu(null, function (popup) {
    test.checkMenu([item], [item], []);
    test.done();
  });
};


// Page contexts should cause their items to be present in the menu when the
// menu is not invoked on an active element.
exports.testPageContextMatch = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let items = [
    new loader.cm.Item({
      label: "item 0"
    }),
    new loader.cm.Item({
      label: "item 1",
      context: undefined
    }),
    new loader.cm.Item({
      label: "item 2",
      context: loader.cm.PageContext()
    }),
    new loader.cm.Item({
      label: "item 3",
      context: [loader.cm.PageContext()]
    })
  ];

  test.showMenu(null, function (popup) {
    test.checkMenu(items, [], []);
    test.done();
  });
};


// Page contexts should cause their items to be absent from the menu when the
// menu is invoked on an active element.
exports.testPageContextNoMatch = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let items = [
    new loader.cm.Item({
      label: "item 0"
    }),
    new loader.cm.Item({
      label: "item 1",
      context: undefined
    }),
    new loader.cm.Item({
      label: "item 2",
      context: loader.cm.PageContext()
    }),
    new loader.cm.Item({
      label: "item 3",
      context: [loader.cm.PageContext()]
    })
  ];

  test.withTestDoc(function (window, doc) {
    test.showMenu("#image", function (popup) {
      test.checkMenu(items, items, []);
      test.done();
    });
  });
};


// Selection contexts should cause items to appear when a selection exists.
exports.testSelectionContextMatch = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let item = loader.cm.Item({
    label: "item",
    context: loader.cm.SelectionContext()
  });

  test.withTestDoc(function (window, doc) {
    window.getSelection().selectAllChildren(doc.body);
    test.showMenu(null, function (popup) {
      test.checkMenu([item], [], []);
      test.done();
    });
  });
};


// Selection contexts should cause items to appear when a selection exists in
// a text field.
exports.testSelectionContextMatchInTextField = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let item = loader.cm.Item({
    label: "item",
    context: loader.cm.SelectionContext()
  });

  test.withTestDoc(function (window, doc) {
    test.selectRange("#textfield", 0, null);
    test.showMenu("#textfield", function (popup) {
      test.checkMenu([item], [], []);
      test.done();
    });
  });
};


// Selection contexts should not cause items to appear when a selection does
// not exist in a text field.
exports.testSelectionContextNoMatchInTextField = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let item = loader.cm.Item({
    label: "item",
    context: loader.cm.SelectionContext()
  });

  test.withTestDoc(function (window, doc) {
    test.selectRange("#textfield", 0, 0);
    test.showMenu("#textfield", function (popup) {
      test.checkMenu([item], [item], []);
      test.done();
    });
  });
};


// Selection contexts should not cause items to appear when a selection does
// not exist.
exports.testSelectionContextNoMatch = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let item = loader.cm.Item({
    label: "item",
    context: loader.cm.SelectionContext()
  });

  test.showMenu(null, function (popup) {
    test.checkMenu([item], [item], []);
    test.done();
  });
};


// Selection contexts should cause items to appear when a selection exists even
// for newly opened pages
exports.testSelectionContextInNewTab = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let item = loader.cm.Item({
    label: "item",
    context: loader.cm.SelectionContext()
  });

  test.withTestDoc(function (window, doc) {
    let link = doc.getElementById("targetlink");
    link.click();

    let tablistener = event => {
      this.tabBrowser.tabContainer.removeEventListener("TabOpen", tablistener, false);
      let tab = event.target;
      let browser = tab.linkedBrowser;
      this.loadFrameScript(browser);
      this.delayedEventListener(browser, "load", () => {
        let window = browser.contentWindow;
        let doc = browser.contentDocument;
        window.getSelection().selectAllChildren(doc.body);

        test.showMenu(null, function (popup) {
          test.checkMenu([item], [], []);
          popup.hidePopup();

          test.tabBrowser.removeTab(test.tabBrowser.selectedTab);
          test.tabBrowser.selectedTab = test.tab;

          test.showMenu(null, function (popup) {
            test.checkMenu([item], [item], []);
            test.done();
          });
        });
      }, true);
    };
    this.tabBrowser.tabContainer.addEventListener("TabOpen", tablistener, false);
  });
};


// Selection contexts should work when right clicking a form button
exports.testSelectionContextButtonMatch = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let item = loader.cm.Item({
    label: "item",
    context: loader.cm.SelectionContext()
  });

  test.withTestDoc(function (window, doc) {
    window.getSelection().selectAllChildren(doc.body);
    test.showMenu("#button", function (popup) {
      test.checkMenu([item], [], []);
      test.done();
    });
  });
};


//Selection contexts should work when right clicking a form button
exports.testSelectionContextButtonNoMatch = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let item = loader.cm.Item({
    label: "item",
    context: loader.cm.SelectionContext()
  });

  test.withTestDoc(function (window, doc) {
    test.showMenu("#button", function (popup) {
      test.checkMenu([item], [item], []);
      test.done();
    });
  });
};


// URL contexts should cause items to appear on pages that match.
exports.testURLContextMatch = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let items = [
    loader.cm.Item({
      label: "item 0",
      context: loader.cm.URLContext(TEST_DOC_URL)
    }),
    loader.cm.Item({
      label: "item 1",
      context: loader.cm.URLContext([TEST_DOC_URL, "*.bogus.com"])
    }),
    loader.cm.Item({
      label: "item 2",
      context: loader.cm.URLContext([new RegExp(".*\\.html")])
    })
  ];

  test.withTestDoc(function (window, doc) {
    test.showMenu(null, function (popup) {
      test.checkMenu(items, [], []);
      test.done();
    });
  });
};


// URL contexts should not cause items to appear on pages that do not match.
exports.testURLContextNoMatch = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let items = [
    loader.cm.Item({
      label: "item 0",
      context: loader.cm.URLContext("*.bogus.com")
    }),
    loader.cm.Item({
      label: "item 1",
      context: loader.cm.URLContext(["*.bogus.com", "*.gnarly.com"])
    }),
    loader.cm.Item({
      label: "item 2",
      context: loader.cm.URLContext([new RegExp(".*\\.js")])
    })
  ];

  test.withTestDoc(function (window, doc) {
    test.showMenu(null, function (popup) {
      test.checkMenu(items, items, []);
      test.done();
    });
  });
};


// Loading a new page in the same tab should correctly start a new worker for
// any content scripts
exports.testPageReload = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let item = loader.cm.Item({
    label: "Item",
    contentScript: "var doc = document; self.on('context', function(node) doc.body.getAttribute('showItem') == 'true');"
  });

  test.withTestDoc(function (window, doc) {
    // Set a flag on the document that the item uses
    doc.body.setAttribute("showItem", "true");

    test.showMenu(null, function (popup) {
      // With the attribute true the item should be visible in the menu
      test.checkMenu([item], [], []);
      test.hideMenu(function() {
        let browser = this.tabBrowser.getBrowserForTab(this.tab)
        test.delayedEventListener(browser, "load", function() {
          test.delayedEventListener(browser, "load", function() {
            window = browser.contentWindow;
            doc = window.document;

            // Set a flag on the document that the item uses
            doc.body.setAttribute("showItem", "false");

            test.showMenu(null, function (popup) {
              // In the new document with the attribute false the item should be
              // hidden, but if the contentScript hasn't been reloaded it will
              // still see the old value
              test.checkMenu([item], [item], []);

              test.done();
            });
          }, true);
          browser.loadURI(TEST_DOC_URL, null, null);
        }, true);
        // Required to make sure we load a new page in history rather than
        // just reloading the current page which would unload it
        browser.loadURI("about:blank", null, null);
      });
    });
  });
};

// Closing a page after it's been used with a worker should cause the worker
// to be destroyed
/*exports.testWorkerDestroy = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let loadExpected = false;

  let item = loader.cm.Item({
    label: "item",
    contentScript: 'self.postMessage("loaded"); self.on("detach", function () { console.log("saw detach"); self.postMessage("detach") });',
    onMessage: function (msg) {
      switch (msg) {
      case "loaded":
        assert.ok(loadExpected, "Should have seen the load event at the right time");
        loadExpected = false;
        break;
      case "detach":
        test.done();
        break;
      }
    }
  });

  test.withTestDoc(function (window, doc) {
    loadExpected = true;
    test.showMenu(null, function (popup) {
      assert.ok(!loadExpected, "Should have seen a message");

      test.checkMenu([item], [], []);

      test.closeTab();
    });
  });
};*/


// Content contexts that return true should cause their items to be present
// in the menu.
exports.testContentContextMatch = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let item = new loader.cm.Item({
    label: "item",
    contentScript: 'self.on("context", function () true);'
  });

  test.showMenu(null, function (popup) {
    test.checkMenu([item], [], []);
    test.done();
  });
};


// Content contexts that return false should cause their items to be absent
// from the menu.
exports.testContentContextNoMatch = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let item = new loader.cm.Item({
    label: "item",
    contentScript: 'self.on("context", function () false);'
  });

  test.showMenu(null, function (popup) {
    test.checkMenu([item], [item], []);
    test.done();
  });
};


// Content contexts that return undefined should cause their items to be absent
// from the menu.
exports.testContentContextUndefined = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let item = new loader.cm.Item({
    label: "item",
    contentScript: 'self.on("context", function () {});'
  });

  test.showMenu(null, function (popup) {
    test.checkMenu([item], [item], []);
    test.done();
  });
};


// Content contexts that return an empty string should cause their items to be
// absent from the menu and shouldn't wipe the label
exports.testContentContextEmptyString = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let item = new loader.cm.Item({
    label: "item",
    contentScript: 'self.on("context", function () "");'
  });

  test.showMenu(null, function (popup) {
    test.checkMenu([item], [item], []);
    assert.equal(item.label, "item", "Label should still be correct");
    test.done();
  });
};


// If any content contexts returns true then their items should be present in
// the menu.
exports.testMultipleContentContextMatch1 = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let item = new loader.cm.Item({
    label: "item",
    contentScript: 'self.on("context", function () true); ' +
                   'self.on("context", function () false);',
    onMessage: function() {
      test.fail("Should not have called the second context listener");
    }
  });

  test.showMenu(null, function (popup) {
    test.checkMenu([item], [], []);
    test.done();
  });
};


// If any content contexts returns true then their items should be present in
// the menu.
exports.testMultipleContentContextMatch2 = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let item = new loader.cm.Item({
    label: "item",
    contentScript: 'self.on("context", function () false); ' +
                   'self.on("context", function () true);'
  });

  test.showMenu(null, function (popup) {
    test.checkMenu([item], [], []);
    test.done();
  });
};


// If any content contexts returns a string then their items should be present
// in the menu.
exports.testMultipleContentContextString1 = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let item = new loader.cm.Item({
    label: "item",
    contentScript: 'self.on("context", function () "new label"); ' +
                   'self.on("context", function () false);'
  });

  test.showMenu(null, function (popup) {
    test.checkMenu([item], [], []);
    assert.equal(item.label, "new label", "Label should have changed");
    test.done();
  });
};


// If any content contexts returns a string then their items should be present
// in the menu.
exports.testMultipleContentContextString2 = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let item = new loader.cm.Item({
    label: "item",
    contentScript: 'self.on("context", function () false); ' +
                   'self.on("context", function () "new label");'
  });

  test.showMenu(null, function (popup) {
    test.checkMenu([item], [], []);
    assert.equal(item.label, "new label", "Label should have changed");
    test.done();
  });
};


// If many content contexts returns a string then the first should take effect
exports.testMultipleContentContextString3 = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let item = new loader.cm.Item({
    label: "item",
    contentScript: 'self.on("context", function () "new label 1"); ' +
                   'self.on("context", function () "new label 2");'
  });

  test.showMenu(null, function (popup) {
    test.checkMenu([item], [], []);
    assert.equal(item.label, "new label 1", "Label should have changed");
    test.done();
  });
};


// Content contexts that return true should cause their items to be present
// in the menu when context clicking an active element.
exports.testContentContextMatchActiveElement = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let items = [
    new loader.cm.Item({
      label: "item 1",
      contentScript: 'self.on("context", function () true);'
    }),
    new loader.cm.Item({
      label: "item 2",
      context: undefined,
      contentScript: 'self.on("context", function () true);'
    }),
    // These items will always be hidden by the declarative usage of PageContext
    new loader.cm.Item({
      label: "item 3",
      context: loader.cm.PageContext(),
      contentScript: 'self.on("context", function () true);'
    }),
    new loader.cm.Item({
      label: "item 4",
      context: [loader.cm.PageContext()],
      contentScript: 'self.on("context", function () true);'
    })
  ];

  test.withTestDoc(function (window, doc) {
    test.showMenu("#image", function (popup) {
      test.checkMenu(items, [items[2], items[3]], []);
      test.done();
    });
  });
};


// Content contexts that return false should cause their items to be absent
// from the menu when context clicking an active element.
exports.testContentContextNoMatchActiveElement = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let items = [
    new loader.cm.Item({
      label: "item 1",
      contentScript: 'self.on("context", function () false);'
    }),
    new loader.cm.Item({
      label: "item 2",
      context: undefined,
      contentScript: 'self.on("context", function () false);'
    }),
    // These items will always be hidden by the declarative usage of PageContext
    new loader.cm.Item({
      label: "item 3",
      context: loader.cm.PageContext(),
      contentScript: 'self.on("context", function () false);'
    }),
    new loader.cm.Item({
      label: "item 4",
      context: [loader.cm.PageContext()],
      contentScript: 'self.on("context", function () false);'
    })
  ];

  test.withTestDoc(function (window, doc) {
    test.showMenu("#image", function (popup) {
      test.checkMenu(items, items, []);
      test.done();
    });
  });
};


// Content contexts that return undefined should cause their items to be absent
// from the menu when context clicking an active element.
exports.testContentContextNoMatchActiveElement = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let items = [
    new loader.cm.Item({
      label: "item 1",
      contentScript: 'self.on("context", function () {});'
    }),
    new loader.cm.Item({
      label: "item 2",
      context: undefined,
      contentScript: 'self.on("context", function () {});'
    }),
    // These items will always be hidden by the declarative usage of PageContext
    new loader.cm.Item({
      label: "item 3",
      context: loader.cm.PageContext(),
      contentScript: 'self.on("context", function () {});'
    }),
    new loader.cm.Item({
      label: "item 4",
      context: [loader.cm.PageContext()],
      contentScript: 'self.on("context", function () {});'
    })
  ];

  test.withTestDoc(function (window, doc) {
    test.showMenu("#image", function (popup) {
      test.checkMenu(items, items, []);
      test.done();
    });
  });
};


// Content contexts that return a string should cause their items to be present
// in the menu and the items' labels to be updated.
exports.testContentContextMatchString = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let item = new loader.cm.Item({
    label: "first label",
    contentScript: 'self.on("context", function () "second label");'
  });

  test.showMenu(null, function (popup) {
    test.checkMenu([item], [], []);
    assert.equal(item.label, "second label",
                     "item's label should be updated");
    test.done();
  });
};


// Ensure that contentScriptFile is working correctly
exports.testContentScriptFile = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();
  let { defer, all } = require("sdk/core/promise");
  let itemScript = [defer(), defer()];
  let menuShown = defer();
  let menuPromises = itemScript.concat(menuShown).map(({promise}) => promise);

  // Reject remote files
  assert.throws(function() {
      new loader.cm.Item({
        label: "item",
        contentScriptFile: "http://mozilla.com/context-menu.js"
      });
    },
    /The `contentScriptFile` option must be a local URL or an array of URLs/,
    "Item throws when contentScriptFile is a remote URL");

  // But accept files from data folder
  let item = new loader.cm.Item({
    label: "item",
    contentScriptFile: data.url("test-contentScriptFile.js"),
    onMessage: (message) => {
      assert.equal(message, "msg from contentScriptFile",
        "contentScriptFile loaded with absolute url");
      itemScript[0].resolve();
    }
  });

  let item2 = new loader.cm.Item({
    label: "item2",
    contentScriptFile: "./test-contentScriptFile.js",
    onMessage: (message) => {
      assert.equal(message, "msg from contentScriptFile",
        "contentScriptFile loaded with relative url");
      itemScript[1].resolve();
    }
  });

  test.showMenu(null, function (popup) {
    test.checkMenu([item, item2], [], []);
    menuShown.resolve();
  });

  all(menuPromises).then(() => test.done());
};


// The args passed to context listeners should be correct.
exports.testContentContextArgs = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();
  let callbacks = 0;

  let item = new loader.cm.Item({
    label: "item",
    contentScript: 'self.on("context", function (node) {' +
                   '  self.postMessage(node.tagName);' +
                   '  return false;' +
                   '});',
    onMessage: function (tagName) {
      assert.equal(tagName, "HTML", "node should be an HTML element");
      if (++callbacks == 2) test.done();
    }
  });

  test.showMenu(null, function () {
    if (++callbacks == 2) test.done();
  });
};

// Multiple contexts imply intersection, not union.
exports.testMultipleContexts = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let item = new loader.cm.Item({
    label: "item",
    context: [loader.cm.SelectorContext("a[href]"), loader.cm.PageContext()],
  });

  test.withTestDoc(function (window, doc) {
    test.showMenu("#span-link", function (popup) {
      test.checkMenu([item], [item], []);
      test.done();
    });
  });
};

// Once a context is removed, it should no longer cause its item to appear.
exports.testRemoveContext = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let ctxt = loader.cm.SelectorContext("img");
  let item = new loader.cm.Item({
    label: "item",
    context: ctxt
  });

  test.withTestDoc(function (window, doc) {
    test.showMenu("#image", function (popup) {

      // The item should be present at first.
      test.checkMenu([item], [], []);
      popup.hidePopup();

      // Remove the img context and check again.
      item.context.remove(ctxt);
      test.showMenu("#image", function (popup) {
        test.checkMenu([item], [item], []);
        test.done();
      });
    });
  });
};

// Once a context is removed, it should no longer cause its item to appear.
exports.testSetContextRemove = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let ctxt = loader.cm.SelectorContext("img");
  let item = new loader.cm.Item({
    label: "item",
    context: ctxt
  });

  test.withTestDoc(function (window, doc) {
    test.showMenu("#image", function (popup) {

      // The item should be present at first.
      test.checkMenu([item], [], []);
      popup.hidePopup();

      // Remove the img context and check again.
      item.context = [];
      test.showMenu("#image", function (popup) {
        test.checkMenu([item], [item], []);
        test.done();
      });
    });
  });
};

// Once a context is added, it should affect whether the item appears.
exports.testAddContext = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let ctxt = loader.cm.SelectorContext("img");
  let item = new loader.cm.Item({
    label: "item"
  });

  test.withTestDoc(function (window, doc) {
    test.showMenu("#image", function (popup) {

      // The item should not be present at first.
      test.checkMenu([item], [item], []);
      popup.hidePopup();

      // Add the img context and check again.
      item.context.add(ctxt);
      test.showMenu("#image", function (popup) {
        test.checkMenu([item], [], []);
        test.done();
      });
    });
  });
};

// Once a context is added, it should affect whether the item appears.
exports.testSetContextAdd = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let ctxt = loader.cm.SelectorContext("img");
  let item = new loader.cm.Item({
    label: "item"
  });

  test.withTestDoc(function (window, doc) {
    test.showMenu("#image", function (popup) {

      // The item should not be present at first.
      test.checkMenu([item], [item], []);
      popup.hidePopup();

      // Add the img context and check again.
      item.context = [ctxt];
      test.showMenu("#image", function (popup) {
        test.checkMenu([item], [], []);
        test.done();
      });
    });
  });
};

// Lots of items should overflow into the overflow submenu.
exports.testOverflow = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let items = [];
  for (let i = 0; i < OVERFLOW_THRESH_DEFAULT + 1; i++) {
    let item = new loader.cm.Item({ label: "item " + i });
    items.push(item);
  }

  test.showMenu(null, function (popup) {
    test.checkMenu(items, [], []);
    test.done();
  });
};


// Module unload should cause all items to be removed.
exports.testUnload = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let item = new loader.cm.Item({ label: "item" });

  test.showMenu(null, function (popup) {

    // The menu should contain the item.
    test.checkMenu([item], [], []);
    popup.hidePopup();

    // Unload the module.
    loader.unload();
    test.showMenu(null, function (popup) {

      // The item should be removed from the menu.
      test.checkMenu([item], [], [item]);
      test.done();
    });
  });
};


// Using multiple module instances to add items without causing overflow should
// work OK.  Assumes OVERFLOW_THRESH_DEFAULT >= 2.
exports.testMultipleModulesAdd = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader0 = test.newLoader();
  let loader1 = test.newLoader();

  // Use each module to add an item, then unload each module in turn.
  let item0 = new loader0.cm.Item({ label: "item 0" });
  let item1 = new loader1.cm.Item({ label: "item 1" });

  test.showMenu(null, function (popup) {

    // The menu should contain both items.
    test.checkMenu([item0, item1], [], []);
    popup.hidePopup();

    // Unload the first module.
    loader0.unload();
    test.showMenu(null, function (popup) {

      // The first item should be removed from the menu.
      test.checkMenu([item0, item1], [], [item0]);
      popup.hidePopup();

      // Unload the second module.
      loader1.unload();
      test.showMenu(null, function (popup) {

        // Both items should be removed from the menu.
        test.checkMenu([item0, item1], [], [item0, item1]);
        test.done();
      });
    });
  });
};


// Using multiple module instances to add items causing overflow should work OK.
exports.testMultipleModulesAddOverflow = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader0 = test.newLoader();
  let loader1 = test.newLoader();

  // Use module 0 to add OVERFLOW_THRESH_DEFAULT items.
  let items0 = [];
  for (let i = 0; i < OVERFLOW_THRESH_DEFAULT; i++) {
    let item = new loader0.cm.Item({ label: "item 0 " + i });
    items0.push(item);
  }

  // Use module 1 to add one item.
  let item1 = new loader1.cm.Item({ label: "item 1" });

  let allItems = items0.concat(item1);

  test.showMenu(null, function (popup) {

    // The menu should contain all items in overflow.
    test.checkMenu(allItems, [], []);
    popup.hidePopup();

    // Unload the first module.
    loader0.unload();
    test.showMenu(null, function (popup) {

      // The first items should be removed from the menu, which should not
      // overflow.
      test.checkMenu(allItems, [], items0);
      popup.hidePopup();

      // Unload the second module.
      loader1.unload();
      test.showMenu(null, function (popup) {

        // All items should be removed from the menu.
        test.checkMenu(allItems, [], allItems);
        test.done();
      });
    });
  });
};


// Using multiple module instances to modify the menu without causing overflow
// should work OK.  This test creates two loaders and:
// loader0 create item -> loader1 create item -> loader0.unload ->
// loader1.unload
exports.testMultipleModulesDiffContexts1 = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader0 = test.newLoader();
  let loader1 = test.newLoader();

  let item0 = new loader0.cm.Item({
    label: "item 0",
    context: loader0.cm.SelectorContext("img")
  });

  let item1 = new loader1.cm.Item({ label: "item 1" });

  test.showMenu(null, function (popup) {

    // The menu should contain item1.
    test.checkMenu([item0, item1], [item0], []);
    popup.hidePopup();

    // Unload module 0.
    loader0.unload();
    test.showMenu(null, function (popup) {

      // item0 should be removed from the menu.
      test.checkMenu([item0, item1], [], [item0]);
      popup.hidePopup();

      // Unload module 1.
      loader1.unload();
      test.showMenu(null, function (popup) {

        // Both items should be removed from the menu.
        test.checkMenu([item0, item1], [], [item0, item1]);
        test.done();
      });
    });
  });
};


// Using multiple module instances to modify the menu without causing overflow
// should work OK.  This test creates two loaders and:
// loader1 create item -> loader0 create item -> loader0.unload ->
// loader1.unload
exports.testMultipleModulesDiffContexts2 = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader0 = test.newLoader();
  let loader1 = test.newLoader();

  let item1 = new loader1.cm.Item({ label: "item 1" });

  let item0 = new loader0.cm.Item({
    label: "item 0",
    context: loader0.cm.SelectorContext("img")
  });

  test.showMenu(null, function (popup) {

    // The menu should contain item1.
    test.checkMenu([item0, item1], [item0], []);
    popup.hidePopup();

    // Unload module 0.
    loader0.unload();
    test.showMenu(null, function (popup) {

      // item0 should be removed from the menu.
      test.checkMenu([item0, item1], [], [item0]);
      popup.hidePopup();

      // Unload module 1.
      loader1.unload();
      test.showMenu(null, function (popup) {

        // Both items should be removed from the menu.
        test.checkMenu([item0, item1], [], [item0, item1]);
        test.done();
      });
    });
  });
};


// Using multiple module instances to modify the menu without causing overflow
// should work OK.  This test creates two loaders and:
// loader0 create item -> loader1 create item -> loader1.unload ->
// loader0.unload
exports.testMultipleModulesDiffContexts3 = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader0 = test.newLoader();
  let loader1 = test.newLoader();

  let item0 = new loader0.cm.Item({
    label: "item 0",
    context: loader0.cm.SelectorContext("img")
  });

  let item1 = new loader1.cm.Item({ label: "item 1" });

  test.showMenu(null, function (popup) {

    // The menu should contain item1.
    test.checkMenu([item0, item1], [item0], []);
    popup.hidePopup();

    // Unload module 1.
    loader1.unload();
    test.showMenu(null, function (popup) {

      // item1 should be removed from the menu.
      test.checkMenu([item0, item1], [item0], [item1]);
      popup.hidePopup();

      // Unload module 0.
      loader0.unload();
      test.showMenu(null, function (popup) {

        // Both items should be removed from the menu.
        test.checkMenu([item0, item1], [], [item0, item1]);
        test.done();
      });
    });
  });
};


// Using multiple module instances to modify the menu without causing overflow
// should work OK.  This test creates two loaders and:
// loader1 create item -> loader0 create item -> loader1.unload ->
// loader0.unload
exports.testMultipleModulesDiffContexts4 = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader0 = test.newLoader();
  let loader1 = test.newLoader();

  let item1 = new loader1.cm.Item({ label: "item 1" });

  let item0 = new loader0.cm.Item({
    label: "item 0",
    context: loader0.cm.SelectorContext("img")
  });

  test.showMenu(null, function (popup) {

    // The menu should contain item1.
    test.checkMenu([item0, item1], [item0], []);
    popup.hidePopup();

    // Unload module 1.
    loader1.unload();
    test.showMenu(null, function (popup) {

      // item1 should be removed from the menu.
      test.checkMenu([item0, item1], [item0], [item1]);
      popup.hidePopup();

      // Unload module 0.
      loader0.unload();
      test.showMenu(null, function (popup) {

        // Both items should be removed from the menu.
        test.checkMenu([item0, item1], [], [item0, item1]);
        test.done();
      });
    });
  });
};


// Test interactions between a loaded module, unloading another module, and the
// menu separator and overflow submenu.
exports.testMultipleModulesAddRemove = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader0 = test.newLoader();
  let loader1 = test.newLoader();

  let item = new loader0.cm.Item({ label: "item" });

  test.showMenu(null, function (popup) {

    // The menu should contain the item.
    test.checkMenu([item], [], []);
    popup.hidePopup();

    // Remove the item.
    item.destroy();
    test.showMenu(null, function (popup) {

      // The item should be removed from the menu.
      test.checkMenu([item], [], [item]);
      popup.hidePopup();

      // Unload module 1.
      loader1.unload();
      test.showMenu(null, function (popup) {

        // There shouldn't be any errors involving the menu separator or
        // overflow submenu.
        test.checkMenu([item], [], [item]);
        test.done();
      });
    });
  });
};


// Checks that the order of menu items is correct when adding/removing across
// multiple modules. All items from a single module should remain in a group
exports.testMultipleModulesOrder = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader0 = test.newLoader();
  let loader1 = test.newLoader();

  // Use each module to add an item, then unload each module in turn.
  let item0 = new loader0.cm.Item({ label: "item 0" });
  let item1 = new loader1.cm.Item({ label: "item 1" });

  test.showMenu(null, function (popup) {

    // The menu should contain both items.
    test.checkMenu([item0, item1], [], []);
    popup.hidePopup();

    let item2 = new loader0.cm.Item({ label: "item 2" });

    test.showMenu(null, function (popup) {

      // The new item should be grouped with the same items from loader0.
      test.checkMenu([item0, item2, item1], [], []);
      popup.hidePopup();

      let item3 = new loader1.cm.Item({ label: "item 3" });

      test.showMenu(null, function (popup) {

        // Same again
        test.checkMenu([item0, item2, item1, item3], [], []);
        test.done();
      });
    });
  });
};


// Checks that the order of menu items is correct when adding/removing across
// multiple modules when overflowing. All items from a single module should
// remain in a group
exports.testMultipleModulesOrderOverflow = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader0 = test.newLoader();
  let loader1 = test.newLoader();

  let prefs = loader0.loader.require("sdk/preferences/service");
  prefs.set(OVERFLOW_THRESH_PREF, 0);

  // Use each module to add an item, then unload each module in turn.
  let item0 = new loader0.cm.Item({ label: "item 0" });
  let item1 = new loader1.cm.Item({ label: "item 1" });

  test.showMenu(null, function (popup) {

    // The menu should contain both items.
    test.checkMenu([item0, item1], [], []);
    popup.hidePopup();

    let item2 = new loader0.cm.Item({ label: "item 2" });

    test.showMenu(null, function (popup) {

      // The new item should be grouped with the same items from loader0.
      test.checkMenu([item0, item2, item1], [], []);
      popup.hidePopup();

      let item3 = new loader1.cm.Item({ label: "item 3" });

      test.showMenu(null, function (popup) {

        // Same again
        test.checkMenu([item0, item2, item1, item3], [], []);
        test.done();
      });
    });
  });
};


// Checks that if a module's items are all hidden then the overflow menu doesn't
// get hidden
exports.testMultipleModulesOverflowHidden = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader0 = test.newLoader();
  let loader1 = test.newLoader();

  let prefs = loader0.loader.require("sdk/preferences/service");
  prefs.set(OVERFLOW_THRESH_PREF, 0);

  // Use each module to add an item, then unload each module in turn.
  let item0 = new loader0.cm.Item({ label: "item 0" });
  let item1 = new loader1.cm.Item({
    label: "item 1",
    context: loader1.cm.SelectorContext("a")
  });

  test.showMenu(null, function (popup) {
    // One should be hidden
    test.checkMenu([item0, item1], [item1], []);
    test.done();
  });
};


// Checks that if a module's items are all hidden then the overflow menu doesn't
// get hidden (reverse order to above)
exports.testMultipleModulesOverflowHidden2 = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader0 = test.newLoader();
  let loader1 = test.newLoader();

  let prefs = loader0.loader.require("sdk/preferences/service");
  prefs.set(OVERFLOW_THRESH_PREF, 0);

  // Use each module to add an item, then unload each module in turn.
  let item0 = new loader0.cm.Item({
    label: "item 0",
    context: loader0.cm.SelectorContext("a")
  });
  let item1 = new loader1.cm.Item({ label: "item 1" });

  test.showMenu(null, function (popup) {
    // One should be hidden
    test.checkMenu([item0, item1], [item0], []);
    test.done();
  });
};


// Checks that we don't overflow if there are more items than the overflow
// threshold but not all of them are visible
exports.testOverflowIgnoresHidden = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let prefs = loader.loader.require("sdk/preferences/service");
  prefs.set(OVERFLOW_THRESH_PREF, 2);

  let allItems = [
    new loader.cm.Item({
      label: "item 0"
    }),
    new loader.cm.Item({
      label: "item 1"
    }),
    new loader.cm.Item({
      label: "item 2",
      context: loader.cm.SelectorContext("a")
    })
  ];

  test.showMenu(null, function (popup) {
    // One should be hidden
    test.checkMenu(allItems, [allItems[2]], []);
    test.done();
  });
};


// Checks that we don't overflow if there are more items than the overflow
// threshold but not all of them are visible
exports.testOverflowIgnoresHiddenMultipleModules1 = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader0 = test.newLoader();
  let loader1 = test.newLoader();

  let prefs = loader0.loader.require("sdk/preferences/service");
  prefs.set(OVERFLOW_THRESH_PREF, 2);

  let allItems = [
    new loader0.cm.Item({
      label: "item 0"
    }),
    new loader0.cm.Item({
      label: "item 1"
    }),
    new loader1.cm.Item({
      label: "item 2",
      context: loader1.cm.SelectorContext("a")
    }),
    new loader1.cm.Item({
      label: "item 3",
      context: loader1.cm.SelectorContext("a")
    })
  ];

  test.showMenu(null, function (popup) {
    // One should be hidden
    test.checkMenu(allItems, [allItems[2], allItems[3]], []);
    test.done();
  });
};


// Checks that we don't overflow if there are more items than the overflow
// threshold but not all of them are visible
exports.testOverflowIgnoresHiddenMultipleModules2 = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader0 = test.newLoader();
  let loader1 = test.newLoader();

  let prefs = loader0.loader.require("sdk/preferences/service");
  prefs.set(OVERFLOW_THRESH_PREF, 2);

  let allItems = [
    new loader0.cm.Item({
      label: "item 0"
    }),
    new loader0.cm.Item({
      label: "item 1",
      context: loader0.cm.SelectorContext("a")
    }),
    new loader1.cm.Item({
      label: "item 2"
    }),
    new loader1.cm.Item({
      label: "item 3",
      context: loader1.cm.SelectorContext("a")
    })
  ];

  test.showMenu(null, function (popup) {
    // One should be hidden
    test.checkMenu(allItems, [allItems[1], allItems[3]], []);
    test.done();
  });
};


// Checks that we don't overflow if there are more items than the overflow
// threshold but not all of them are visible
exports.testOverflowIgnoresHiddenMultipleModules3 = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader0 = test.newLoader();
  let loader1 = test.newLoader();

  let prefs = loader0.loader.require("sdk/preferences/service");
  prefs.set(OVERFLOW_THRESH_PREF, 2);

  let allItems = [
    new loader0.cm.Item({
      label: "item 0",
      context: loader0.cm.SelectorContext("a")
    }),
    new loader0.cm.Item({
      label: "item 1",
      context: loader0.cm.SelectorContext("a")
    }),
    new loader1.cm.Item({
      label: "item 2"
    }),
    new loader1.cm.Item({
      label: "item 3"
    })
  ];

  test.showMenu(null, function (popup) {
    // One should be hidden
    test.checkMenu(allItems, [allItems[0], allItems[1]], []);
    test.done();
  });
};


// Tests that we transition between overflowing to non-overflowing to no items
// and back again
exports.testOverflowTransition = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let prefs = loader.loader.require("sdk/preferences/service");
  prefs.set(OVERFLOW_THRESH_PREF, 2);

  let pItems = [
    new loader.cm.Item({
      label: "item 0",
      context: loader.cm.SelectorContext("p")
    }),
    new loader.cm.Item({
      label: "item 1",
      context: loader.cm.SelectorContext("p")
    })
  ];

  let aItems = [
    new loader.cm.Item({
      label: "item 2",
      context: loader.cm.SelectorContext("a")
    }),
    new loader.cm.Item({
      label: "item 3",
      context: loader.cm.SelectorContext("a")
    })
  ];

  let allItems = pItems.concat(aItems);

  test.withTestDoc(function (window, doc) {
    test.showMenu("#link", function (popup) {
      // The menu should contain all items and will overflow
      test.checkMenu(allItems, [], []);
      popup.hidePopup();

      test.showMenu("#text", function (popup) {
        // Only contains hald the items and will not overflow
        test.checkMenu(allItems, aItems, []);
        popup.hidePopup();

        test.showMenu(null, function (popup) {
          // None of the items will be visible
          test.checkMenu(allItems, allItems, []);
          popup.hidePopup();

          test.showMenu("#text", function (popup) {
            // Only contains hald the items and will not overflow
            test.checkMenu(allItems, aItems, []);
            popup.hidePopup();

            test.showMenu("#link", function (popup) {
              // The menu should contain all items and will overflow
              test.checkMenu(allItems, [], []);
              popup.hidePopup();

              test.showMenu(null, function (popup) {
                // None of the items will be visible
                test.checkMenu(allItems, allItems, []);
                popup.hidePopup();

                test.showMenu("#link", function (popup) {
                  // The menu should contain all items and will overflow
                  test.checkMenu(allItems, [], []);
                  test.done();
                });
              });
            });
          });
        });
      });
    });
  });
};


// An item's command listener should work.
exports.testItemCommand = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let item = new loader.cm.Item({
    label: "item",
    data: "item data",
    contentScript: 'self.on("click", function (node, data) {' +
                   '  self.postMessage({' +
                   '    tagName: node.tagName,' +
                   '    data: data' +
                   '  });' +
                   '});',
    onMessage: function (data) {
      assert.equal(this, item, "`this` inside onMessage should be item");
      assert.equal(data.tagName, "HTML", "node should be an HTML element");
      assert.equal(data.data, item.data, "data should be item data");
      test.done();
    }
  });

  test.showMenu(null, function (popup) {
    test.checkMenu([item], [], []);
    let elt = test.getItemElt(popup, item);

    // create a command event
    let evt = elt.ownerDocument.createEvent('Event');
    evt.initEvent('command', true, true);
    elt.dispatchEvent(evt);
  });
};


// A menu's click listener should work and receive bubbling 'command' events from
// sub-items appropriately.  This also tests menus and ensures that when a CSS
// selector context matches the clicked node's ancestor, the matching ancestor
// is passed to listeners as the clicked node.
exports.testMenuCommand = function (assert, done) {
  // Create a top-level menu, submenu, and item, like this:
  // topMenu -> submenu -> item
  // Click the item and make sure the click bubbles.
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let item = new loader.cm.Item({
    label: "submenu item",
    data: "submenu item data",
    context: loader.cm.SelectorContext("a"),
  });

  let submenu = new loader.cm.Menu({
    label: "submenu",
    context: loader.cm.SelectorContext("a"),
    items: [item]
  });

  let topMenu = new loader.cm.Menu({
    label: "top menu",
    contentScript: 'self.on("click", function (node, data) {' +
                   '  self.postMessage({' +
                   '    tagName: node.tagName,' +
                   '    data: data' +
                   '  });' +
                   '});',
    onMessage: function (data) {
      assert.equal(this, topMenu, "`this` inside top menu should be menu");
      assert.equal(data.tagName, "A", "Clicked node should be anchor");
      assert.equal(data.data, item.data,
                       "Clicked item data should be correct");
      test.done();
    },
    items: [submenu],
    context: loader.cm.SelectorContext("a")
  });

  test.withTestDoc(function (window, doc) {
    test.showMenu("#span-link", function (popup) {
      test.checkMenu([topMenu], [], []);
      let topMenuElt = test.getItemElt(popup, topMenu);
      let topMenuPopup = topMenuElt.firstChild;
      let submenuElt = test.getItemElt(topMenuPopup, submenu);
      let submenuPopup = submenuElt.firstChild;
      let itemElt = test.getItemElt(submenuPopup, item);

      // create a command event
      let evt = itemElt.ownerDocument.createEvent('Event');
      evt.initEvent('command', true, true);
      itemElt.dispatchEvent(evt);
    });
  });
};


// Click listeners should work when multiple modules are loaded.
exports.testItemCommandMultipleModules = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader0 = test.newLoader();
  let loader1 = test.newLoader();

  let item0 = loader0.cm.Item({
    label: "loader 0 item",
    contentScript: 'self.on("click", self.postMessage);',
    onMessage: function () {
      test.fail("loader 0 item should not emit click event");
    }
  });
  let item1 = loader1.cm.Item({
    label: "loader 1 item",
    contentScript: 'self.on("click", self.postMessage);',
    onMessage: function () {
      test.pass("loader 1 item clicked as expected");
      test.done();
    }
  });

  test.showMenu(null, function (popup) {
    test.checkMenu([item0, item1], [], []);
    let item1Elt = test.getItemElt(popup, item1);

    // create a command event
    let evt = item1Elt.ownerDocument.createEvent('Event');
    evt.initEvent('command', true, true);
    item1Elt.dispatchEvent(evt);
  });
};




// An item's click listener should work.
exports.testItemClick = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let item = new loader.cm.Item({
    label: "item",
    data: "item data",
    contentScript: 'self.on("click", function (node, data) {' +
                   '  self.postMessage({' +
                   '    tagName: node.tagName,' +
                   '    data: data' +
                   '  });' +
                   '});',
    onMessage: function (data) {
      assert.equal(this, item, "`this` inside onMessage should be item");
      assert.equal(data.tagName, "HTML", "node should be an HTML element");
      assert.equal(data.data, item.data, "data should be item data");
      test.done();
    }
  });

  test.showMenu(null, function (popup) {
    test.checkMenu([item], [], []);
    let elt = test.getItemElt(popup, item);
    elt.click();
  });
};


// A menu's click listener should work and receive bubbling clicks from
// sub-items appropriately.  This also tests menus and ensures that when a CSS
// selector context matches the clicked node's ancestor, the matching ancestor
// is passed to listeners as the clicked node.
exports.testMenuClick = function (assert, done) {
  // Create a top-level menu, submenu, and item, like this:
  // topMenu -> submenu -> item
  // Click the item and make sure the click bubbles.
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let item = new loader.cm.Item({
    label: "submenu item",
    data: "submenu item data",
    context: loader.cm.SelectorContext("a"),
  });

  let submenu = new loader.cm.Menu({
    label: "submenu",
    context: loader.cm.SelectorContext("a"),
    items: [item]
  });

  let topMenu = new loader.cm.Menu({
    label: "top menu",
    contentScript: 'self.on("click", function (node, data) {' +
                   '  self.postMessage({' +
                   '    tagName: node.tagName,' +
                   '    data: data' +
                   '  });' +
                   '});',
    onMessage: function (data) {
      assert.equal(this, topMenu, "`this` inside top menu should be menu");
      assert.equal(data.tagName, "A", "Clicked node should be anchor");
      assert.equal(data.data, item.data,
                       "Clicked item data should be correct");
      test.done();
    },
    items: [submenu],
    context: loader.cm.SelectorContext("a")
  });

  test.withTestDoc(function (window, doc) {
    test.showMenu("#span-link", function (popup) {
      test.checkMenu([topMenu], [], []);
      let topMenuElt = test.getItemElt(popup, topMenu);
      let topMenuPopup = topMenuElt.firstChild;
      let submenuElt = test.getItemElt(topMenuPopup, submenu);
      let submenuPopup = submenuElt.firstChild;
      let itemElt = test.getItemElt(submenuPopup, item);
      itemElt.click();
    });
  });
};

// Click listeners should work when multiple modules are loaded.
exports.testItemClickMultipleModules = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader0 = test.newLoader();
  let loader1 = test.newLoader();

  let item0 = loader0.cm.Item({
    label: "loader 0 item",
    contentScript: 'self.on("click", self.postMessage);',
    onMessage: function () {
      test.fail("loader 0 item should not emit click event");
    }
  });
  let item1 = loader1.cm.Item({
    label: "loader 1 item",
    contentScript: 'self.on("click", self.postMessage);',
    onMessage: function () {
      test.pass("loader 1 item clicked as expected");
      test.done();
    }
  });

  test.showMenu(null, function (popup) {
    test.checkMenu([item0, item1], [], []);
    let item1Elt = test.getItemElt(popup, item1);
    item1Elt.click();
  });
};


// Adding a separator to a submenu should work OK.
exports.testSeparator = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let menu = new loader.cm.Menu({
    label: "submenu",
    items: [new loader.cm.Separator()]
  });

  test.showMenu(null, function (popup) {
    test.checkMenu([menu], [], []);
    test.done();
  });
};


// The parentMenu option should work
exports.testParentMenu = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let menu = new loader.cm.Menu({
    label: "submenu",
    items: [loader.cm.Item({ label: "item 1" })],
    parentMenu: loader.cm.contentContextMenu
  });

  let item = loader.cm.Item({
    label: "item 2",
    parentMenu: menu,
  });

  assert.equal(menu.items[1], item, "Item should be in the sub menu");

  test.showMenu(null, function (popup) {
    test.checkMenu([menu], [], []);
    test.done();
  });
};


// Existing context menu modifications should apply to new windows.
exports.testNewWindow = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let item = new loader.cm.Item({ label: "item" });

  test.withNewWindow(function () {
    test.showMenu(null, function (popup) {
      test.checkMenu([item], [], []);
      test.done();
    });
  });
};


// When a new window is opened, items added by an unloaded module should not
// be present in the menu.
exports.testNewWindowMultipleModules = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();
  let item = new loader.cm.Item({ label: "item" });

  test.showMenu(null, function (popup) {
    test.checkMenu([item], [], []);
    popup.hidePopup();
    loader.unload();
    test.withNewWindow(function () {
      test.showMenu(null, function (popup) {
        test.checkMenu([item], [], [item]);
        test.done();
      });
    });
  });
};


// Existing context menu modifications should not apply to new private windows.
exports.testNewPrivateWindow = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let item = new loader.cm.Item({ label: "item" });

  test.showMenu(null, function (popup) {
    test.checkMenu([item], [], []);
    popup.hidePopup();

    test.withNewPrivateWindow(function () {
      test.showMenu(null, function (popup) {
        test.checkMenu([], [], []);
        test.done();
      });
    });
  });
};


// Existing context menu modifications should apply to new private windows when
// private browsing support is enabled.
exports.testNewPrivateEnabledWindow = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newPrivateLoader();

  let item = new loader.cm.Item({ label: "item" });

  test.showMenu(null, function (popup) {
    test.checkMenu([item], [], []);
    popup.hidePopup();

    test.withNewPrivateWindow(function () {
      test.showMenu(null, function (popup) {
        test.checkMenu([item], [], []);
        test.done();
      });
    });
  });
};


// Existing context menu modifications should apply to new private windows when
// private browsing support is enabled unless unloaded.
exports.testNewPrivateEnabledWindowUnloaded = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newPrivateLoader();

  let item = new loader.cm.Item({ label: "item" });

  test.showMenu(null, function (popup) {
    test.checkMenu([item], [], []);
    popup.hidePopup();

    loader.unload();

    test.withNewPrivateWindow(function () {
      test.showMenu(null, function (popup) {
        test.checkMenu([], [], []);
        test.done();
      });
    });
  });
};


// Items in the context menu should be sorted according to locale.
exports.testSorting = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  // Make an unsorted items list.  It'll look like this:
  //   item 1, item 0, item 3, item 2, item 5, item 4, ...
  let items = [];
  for (let i = 0; i < OVERFLOW_THRESH_DEFAULT; i += 2) {
    items.push(new loader.cm.Item({ label: "item " + (i + 1) }));
    items.push(new loader.cm.Item({ label: "item " + i }));
  }

  test.showMenu(null, function (popup) {
    test.checkMenu(items, [], []);
    test.done();
  });
};


// Items in the overflow menu should be sorted according to locale.
exports.testSortingOverflow = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  // Make an unsorted items list.  It'll look like this:
  //   item 1, item 0, item 3, item 2, item 5, item 4, ...
  let items = [];
  for (let i = 0; i < OVERFLOW_THRESH_DEFAULT * 2; i += 2) {
    items.push(new loader.cm.Item({ label: "item " + (i + 1) }));
    items.push(new loader.cm.Item({ label: "item " + i }));
  }

  test.showMenu(null, function (popup) {
    test.checkMenu(items, [], []);
    test.done();
  });
};


// Multiple modules shouldn't interfere with sorting.
exports.testSortingMultipleModules = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader0 = test.newLoader();
  let loader1 = test.newLoader();

  let items0 = [];
  let items1 = [];
  for (let i = 0; i < OVERFLOW_THRESH_DEFAULT; i++) {
    if (i % 2) {
      let item = new loader0.cm.Item({ label: "item " + i });
      items0.push(item);
    }
    else {
      let item = new loader1.cm.Item({ label: "item " + i });
      items1.push(item);
    }
  }
  let allItems = items0.concat(items1);

  test.showMenu(null, function (popup) {

    // All items should be present and sorted.
    test.checkMenu(allItems, [], []);
    popup.hidePopup();
    loader0.unload();
    loader1.unload();
    test.showMenu(null, function (popup) {

      // All items should be removed.
      test.checkMenu(allItems, [], allItems);
      test.done();
    });
  });
};


// Content click handlers and context handlers should be able to communicate,
// i.e., they're eval'ed in the same worker and sandbox.
exports.testContentCommunication = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let item = new loader.cm.Item({
    label: "item",
    contentScript: 'var potato;' +
                   'self.on("context", function () {' +
                   '  potato = "potato";' +
                   '  return true;' +
                   '});' +
                   'self.on("click", function () {' +
                   '  self.postMessage(potato);' +
                   '});',
  });

  item.on("message", function (data) {
    assert.equal(data, "potato", "That's a lot of potatoes!");
    test.done();
  });

  test.showMenu(null, function (popup) {
    test.checkMenu([item], [], []);
    let elt = test.getItemElt(popup, item);
    elt.click();
  });
};


// When the context menu is invoked on a tab that was already open when the
// module was loaded, it should contain the expected items and content workers
// should function as expected.
exports.testLoadWithOpenTab = function (assert, done) {
  let test = new TestHelper(assert, done);
  test.withTestDoc(function (window, doc) {
    let loader = test.newLoader();
    let item = new loader.cm.Item({
      label: "item",
      contentScript:
        'self.on("click", function () self.postMessage("click"));',
      onMessage: function (msg) {
        if (msg === "click")
          test.done();
      }
    });
    test.showMenu(null, function (popup) {
      test.checkMenu([item], [], []);
      test.getItemElt(popup, item).click();
    });
  });
};

// Bug 732716: Ensure that the node given in `click` event works fine
// (i.e. is correctly wrapped)
exports.testDrawImageOnClickNode = function (assert, done) {
  let test = new TestHelper(assert, done);
  test.withTestDoc(function (window, doc) {
    let loader = test.newLoader();
    let item = new loader.cm.Item({
      label: "item",
      context: loader.cm.SelectorContext("img"),
      contentScript: "new " + function() {
        self.on("click", function (img, data) {
          let ctx = document.createElement("canvas").getContext("2d");
          ctx.drawImage(img, 1, 1, 1, 1);
          self.postMessage("done");
        });
      },
      onMessage: function (msg) {
        if (msg === "done")
          test.done();
      }
    });
    test.showMenu("#image", function (popup) {
      test.checkMenu([item], [], []);
      test.getItemElt(popup, item).click();
    });
  });
};


// Setting an item's label before the menu is ever shown should correctly change
// its label.
exports.testSetLabelBeforeShow = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let items = [
    new loader.cm.Item({ label: "a" }),
    new loader.cm.Item({ label: "b" })
  ]
  items[0].label = "z";
  assert.equal(items[0].label, "z");

  test.showMenu(null, function (popup) {
    test.checkMenu(items, [], []);
    test.done();
  });
};


// Setting an item's label after the menu is shown should correctly change its
// label.
exports.testSetLabelAfterShow = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let items = [
    new loader.cm.Item({ label: "a" }),
    new loader.cm.Item({ label: "b" })
  ];

  test.showMenu(null, function (popup) {
    test.checkMenu(items, [], []);
    popup.hidePopup();

    items[0].label = "z";
    assert.equal(items[0].label, "z");
    test.showMenu(null, function (popup) {
      test.checkMenu(items, [], []);
      test.done();
    });
  });
};


// Setting an item's label before the menu is ever shown should correctly change
// its label.
exports.testSetLabelBeforeShowOverflow = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let prefs = loader.loader.require("sdk/preferences/service");
  prefs.set(OVERFLOW_THRESH_PREF, 0);

  let items = [
    new loader.cm.Item({ label: "a" }),
    new loader.cm.Item({ label: "b" })
  ]
  items[0].label = "z";
  assert.equal(items[0].label, "z");

  test.showMenu(null, function (popup) {
    test.checkMenu(items, [], []);
    test.done();
  });
};


// Setting an item's label after the menu is shown should correctly change its
// label.
exports.testSetLabelAfterShowOverflow = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let prefs = loader.loader.require("sdk/preferences/service");
  prefs.set(OVERFLOW_THRESH_PREF, 0);

  let items = [
    new loader.cm.Item({ label: "a" }),
    new loader.cm.Item({ label: "b" })
  ];

  test.showMenu(null, function (popup) {
    test.checkMenu(items, [], []);
    popup.hidePopup();

    items[0].label = "z";
    assert.equal(items[0].label, "z");
    test.showMenu(null, function (popup) {
      test.checkMenu(items, [], []);
      test.done();
    });
  });
};


// Setting the label of an item in a Menu should work.
exports.testSetLabelMenuItem = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let menu = loader.cm.Menu({
    label: "menu",
    items: [loader.cm.Item({ label: "a" })]
  });
  menu.items[0].label = "z";

  assert.equal(menu.items[0].label, "z");

  test.showMenu(null, function (popup) {
    test.checkMenu([menu], [], []);
    test.done();
  });
};


// Menu.addItem() should work.
exports.testMenuAddItem = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let menu = loader.cm.Menu({
    label: "menu",
    items: [
      loader.cm.Item({ label: "item 0" })
    ]
  });
  menu.addItem(loader.cm.Item({ label: "item 1" }));
  menu.addItem(loader.cm.Item({ label: "item 2" }));

  assert.equal(menu.items.length, 3,
                   "menu should have correct number of items");
  for (let i = 0; i < 3; i++) {
    assert.equal(menu.items[i].label, "item " + i,
                     "item label should be correct");
    assert.equal(menu.items[i].parentMenu, menu,
                     "item's parent menu should be correct");
  }

  test.showMenu(null, function (popup) {
    test.checkMenu([menu], [], []);
    test.done();
  });
};


// Adding the same item twice to a menu should work as expected.
exports.testMenuAddItemTwice = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let menu = loader.cm.Menu({
    label: "menu",
    items: []
  });
  let subitem = loader.cm.Item({ label: "item 1" })
  menu.addItem(subitem);
  menu.addItem(loader.cm.Item({ label: "item 0" }));
  menu.addItem(subitem);

  assert.equal(menu.items.length, 2,
                   "menu should have correct number of items");
  for (let i = 0; i < 2; i++) {
    assert.equal(menu.items[i].label, "item " + i,
                     "item label should be correct");
  }

  test.showMenu(null, function (popup) {
    test.checkMenu([menu], [], []);
    test.done();
  });
};


// Menu.removeItem() should work.
exports.testMenuRemoveItem = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let subitem = loader.cm.Item({ label: "item 1" });
  let menu = loader.cm.Menu({
    label: "menu",
    items: [
      loader.cm.Item({ label: "item 0" }),
      subitem,
      loader.cm.Item({ label: "item 2" })
    ]
  });

  // Removing twice should be harmless.
  menu.removeItem(subitem);
  menu.removeItem(subitem);

  assert.equal(subitem.parentMenu, null,
                   "item's parent menu should be correct");

  assert.equal(menu.items.length, 2,
                   "menu should have correct number of items");
  assert.equal(menu.items[0].label, "item 0",
                   "item label should be correct");
  assert.equal(menu.items[1].label, "item 2",
                   "item label should be correct");

  test.showMenu(null, function (popup) {
    test.checkMenu([menu], [], []);
    test.done();
  });
};


// Adding an item currently contained in one menu to another menu should work.
exports.testMenuItemSwap = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let subitem = loader.cm.Item({ label: "item" });
  let menu0 = loader.cm.Menu({
    label: "menu 0",
    items: [subitem]
  });
  let menu1 = loader.cm.Menu({
    label: "menu 1",
    items: []
  });
  menu1.addItem(subitem);

  assert.equal(menu0.items.length, 0,
                   "menu should have correct number of items");

  assert.equal(menu1.items.length, 1,
                   "menu should have correct number of items");
  assert.equal(menu1.items[0].label, "item",
                   "item label should be correct");

  assert.equal(subitem.parentMenu, menu1,
                   "item's parent menu should be correct");

  test.showMenu(null, function (popup) {
    test.checkMenu([menu0, menu1], [menu0], []);
    test.done();
  });
};


// Destroying an item should remove it from its parent menu.
exports.testMenuItemDestroy = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let subitem = loader.cm.Item({ label: "item" });
  let menu = loader.cm.Menu({
    label: "menu",
    items: [subitem]
  });
  subitem.destroy();

  assert.equal(menu.items.length, 0,
                   "menu should have correct number of items");
  assert.equal(subitem.parentMenu, null,
                   "item's parent menu should be correct");

  test.showMenu(null, function (popup) {
    test.checkMenu([menu], [menu], []);
    test.done();
  });
};


// Setting Menu.items should work.
exports.testMenuItemsSetter = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let menu = loader.cm.Menu({
    label: "menu",
    items: [
      loader.cm.Item({ label: "old item 0" }),
      loader.cm.Item({ label: "old item 1" })
    ]
  });
  menu.items = [
    loader.cm.Item({ label: "new item 0" }),
    loader.cm.Item({ label: "new item 1" }),
    loader.cm.Item({ label: "new item 2" })
  ];

  assert.equal(menu.items.length, 3,
                   "menu should have correct number of items");
  for (let i = 0; i < 3; i++) {
    assert.equal(menu.items[i].label, "new item " + i,
                     "item label should be correct");
    assert.equal(menu.items[i].parentMenu, menu,
                     "item's parent menu should be correct");
  }

  test.showMenu(null, function (popup) {
    test.checkMenu([menu], [], []);
    test.done();
  });
};


// Setting Item.data should work.
exports.testItemDataSetter = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let item = loader.cm.Item({ label: "old item 0", data: "old" });
  item.data = "new";

  assert.equal(item.data, "new", "item should have correct data");

  test.showMenu(null, function (popup) {
    test.checkMenu([item], [], []);
    test.done();
  });
};


// Open the test doc, load the module, make sure items appear when context-
// clicking the iframe.
exports.testAlreadyOpenIframe = function (assert, done) {
  let test = new TestHelper(assert, done);
  test.withTestDoc(function (window, doc) {
    let loader = test.newLoader();
    let item = new loader.cm.Item({
      label: "item"
    });
    test.showMenu("#iframe", function (popup) {
      test.checkMenu([item], [], []);
      test.done();
    });
  });
};


// Tests that a missing label throws an exception
exports.testItemNoLabel = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  try {
    new loader.cm.Item({});
    assert.ok(false, "Should have seen exception");
  }
  catch (e) {
    assert.ok(true, "Should have seen exception");
  }

  try {
    new loader.cm.Item({ label: null });
    assert.ok(false, "Should have seen exception");
  }
  catch (e) {
    assert.ok(true, "Should have seen exception");
  }

  try {
    new loader.cm.Item({ label: undefined });
    assert.ok(false, "Should have seen exception");
  }
  catch (e) {
    assert.ok(true, "Should have seen exception");
  }

  try {
    new loader.cm.Item({ label: "" });
    assert.ok(false, "Should have seen exception");
  }
  catch (e) {
    assert.ok(true, "Should have seen exception");
  }

  test.done();
}


// Tests that items can have an empty data property
exports.testItemNoData = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  function checkData(data) {
    assert.equal(data, undefined, "Data should be undefined");
  }

  let item1 = new loader.cm.Item({
    label: "item 1",
    contentScript: 'self.on("click", function(node, data) self.postMessage(data))',
    onMessage: checkData
  });
  let item2 = new loader.cm.Item({
    label: "item 2",
    data: null,
    contentScript: 'self.on("click", function(node, data) self.postMessage(data))',
    onMessage: checkData
  });
  let item3 = new loader.cm.Item({
    label: "item 3",
    data: undefined,
    contentScript: 'self.on("click", function(node, data) self.postMessage(data))',
    onMessage: checkData
  });

  assert.equal(item1.data, undefined, "Should be no defined data");
  assert.equal(item2.data, null, "Should be no defined data");
  assert.equal(item3.data, undefined, "Should be no defined data");

  test.showMenu(null, function (popup) {
    test.checkMenu([item1, item2, item3], [], []);

    let itemElt = test.getItemElt(popup, item1);
    itemElt.click();

    test.hideMenu(function() {
      test.showMenu(null, function (popup) {
        let itemElt = test.getItemElt(popup, item2);
        itemElt.click();

        test.hideMenu(function() {
          test.showMenu(null, function (popup) {
            let itemElt = test.getItemElt(popup, item3);
            itemElt.click();

            test.done();
          });
        });
      });
    });
  });
}


exports.testItemNoAccessKey = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let item1 = new loader.cm.Item({ label: "item 1" });
  let item2 = new loader.cm.Item({ label: "item 2", accesskey: null });
  let item3 = new loader.cm.Item({ label: "item 3", accesskey: undefined });

  assert.equal(item1.accesskey, undefined, "Should be no defined image");
  assert.equal(item2.accesskey, null, "Should be no defined image");
  assert.equal(item3.accesskey, undefined, "Should be no defined image");

  test.showMenu().
  then((popup) => test.checkMenu([item1, item2, item3], [], [])).
  then(test.done).
  catch(assert.fail);
}


// Test accesskey support.
exports.testItemAccessKey = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let item = new loader.cm.Item({ label: "item", accesskey: "i" });
  assert.equal(item.accesskey, "i", "Should have set the image to i");

  let menu = new loader.cm.Menu({ label: "menu", accesskey: "m", items: [
    loader.cm.Item({ label: "subitem" })
  ]});
  assert.equal(menu.accesskey, "m", "Should have set the accesskey to m");

  test.showMenu().then((popup) => {
    test.checkMenu([item, menu], [], []);

    let accesskey = "e";
    menu.accesskey = item.accesskey = accesskey;
    assert.equal(item.accesskey, accesskey, "Should have set the accesskey to " + accesskey);
    assert.equal(menu.accesskey, accesskey, "Should have set the accesskey to " + accesskey);
    test.checkMenu([item, menu], [], []);

    item.accesskey = null;
    menu.accesskey = null;
    assert.equal(item.accesskey, null, "Should have set the accesskey to " + accesskey);
    assert.equal(menu.accesskey, null, "Should have set the accesskey to " + accesskey);
    test.checkMenu([item, menu], [], []);
  }).
  then(test.done).
  catch(assert.fail);
};


// Tests that items without an image don't attempt to show one
exports.testItemNoImage = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let item1 = new loader.cm.Item({ label: "item 1" });
  let item2 = new loader.cm.Item({ label: "item 2", image: null });
  let item3 = new loader.cm.Item({ label: "item 3", image: undefined });

  assert.equal(item1.image, undefined, "Should be no defined image");
  assert.equal(item2.image, null, "Should be no defined image");
  assert.equal(item3.image, undefined, "Should be no defined image");

  test.showMenu(null, function (popup) {
    test.checkMenu([item1, item2, item3], [], []);

    test.done();
  });
}


// Test image support.
exports.testItemImage = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let imageURL = data.url("moz_favicon.ico");
  let item = new loader.cm.Item({ label: "item", image: imageURL });
  let menu = new loader.cm.Menu({ label: "menu", image: imageURL, items: [
    loader.cm.Item({ label: "subitem" })
  ]});
  assert.equal(item.image, imageURL, "Should have set the image correctly");
  assert.equal(menu.image, imageURL, "Should have set the image correctly");

  test.showMenu(null, function (popup) {
    test.checkMenu([item, menu], [], []);

    let imageURL2 = data.url("dummy.ico");
    item.image = imageURL2;
    menu.image = imageURL2;
    assert.equal(item.image, imageURL2, "Should have set the image correctly");
    assert.equal(menu.image, imageURL2, "Should have set the image correctly");
    test.checkMenu([item, menu], [], []);

    item.image = null;
    menu.image = null;
    assert.equal(item.image, null, "Should have set the image correctly");
    assert.equal(menu.image, null, "Should have set the image correctly");
    test.checkMenu([item, menu], [], []);

    test.done();
  });
};

// Test image URL validation.
exports.testItemImageValidURL = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  assert.throws(function(){
      new loader.cm.Item({
        label: "item 1",
        image: "foo"
      })
    }, /Image URL validation failed/
  );

  assert.throws(function(){
      new loader.cm.Item({
        label: "item 2",
        image: false
      })
    }, /Image URL validation failed/
  );

  assert.throws(function(){
      new loader.cm.Item({
        label: "item 3",
        image: 0
      })
    }, /Image URL validation failed/
  );

  let imageURL = data.url("moz_favicon.ico");
  let item4 = new loader.cm.Item({ label: "item 4", image: imageURL });
  let item5 = new loader.cm.Item({ label: "item 5", image: null });
  let item6 = new loader.cm.Item({ label: "item 6", image: undefined });

  assert.equal(item4.image, imageURL, "Should be proper image URL");
  assert.equal(item5.image, null, "Should be null image");
  assert.equal(item6.image, undefined, "Should be undefined image");

  test.done();
};


// Menu.destroy should destroy the item tree rooted at that menu.
exports.testMenuDestroy = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let menu = loader.cm.Menu({
    label: "menu",
    items: [
      loader.cm.Item({ label: "item 0" }),
      loader.cm.Menu({
        label: "item 1",
        items: [
          loader.cm.Item({ label: "subitem 0" }),
          loader.cm.Item({ label: "subitem 1" }),
          loader.cm.Item({ label: "subitem 2" })
        ]
      }),
      loader.cm.Item({ label: "item 2" })
    ]
  });
  menu.destroy();

  /*let numRegistryEntries = 0;
  loader.globalScope.browserManager.browserWins.forEach(function (bwin) {
    for (let itemID in bwin.items)
      numRegistryEntries++;
  });
  assert.equal(numRegistryEntries, 0, "All items should be unregistered.");*/

  test.showMenu(null, function (popup) {
    test.checkMenu([menu], [], [menu]);
    test.done();
  });
};

// Checks that if a menu contains sub items that are hidden then the menu is
// hidden too. Also checks that content scripts and contexts work for sub items.
exports.testSubItemContextNoMatchHideMenu = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let items = [
    loader.cm.Menu({
      label: "menu 1",
      items: [
        loader.cm.Item({
          label: "subitem 1",
          context: loader.cm.SelectorContext(".foo")
        })
      ]
    }),
    loader.cm.Menu({
      label: "menu 2",
      items: [
        loader.cm.Item({
          label: "subitem 2",
          contentScript: 'self.on("context", function () false);'
        })
      ]
    }),
    loader.cm.Menu({
      label: "menu 3",
      items: [
        loader.cm.Item({
          label: "subitem 3",
          context: loader.cm.SelectorContext(".foo")
        }),
        loader.cm.Item({
          label: "subitem 4",
          contentScript: 'self.on("context", function () false);'
        })
      ]
    })
  ];

  test.showMenu(null, function (popup) {
    test.checkMenu(items, items, []);
    test.done();
  });
};


// Checks that if a menu contains a combination of hidden and visible sub items
// then the menu is still visible too.
exports.testSubItemContextMatch = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let hiddenItems = [
    loader.cm.Item({
      label: "subitem 3",
      context: loader.cm.SelectorContext(".foo")
    }),
    loader.cm.Item({
      label: "subitem 6",
      contentScript: 'self.on("context", function () false);'
    })
  ];

  let items = [
    loader.cm.Menu({
      label: "menu 1",
      items: [
        loader.cm.Item({
          label: "subitem 1",
          context: loader.cm.URLContext(TEST_DOC_URL)
        })
      ]
    }),
    loader.cm.Menu({
      label: "menu 2",
      items: [
        loader.cm.Item({
          label: "subitem 2",
          contentScript: 'self.on("context", function () true);'
        })
      ]
    }),
    loader.cm.Menu({
      label: "menu 3",
      items: [
        hiddenItems[0],
        loader.cm.Item({
          label: "subitem 4",
          contentScript: 'self.on("context", function () true);'
        })
      ]
    }),
    loader.cm.Menu({
      label: "menu 4",
      items: [
        loader.cm.Item({
          label: "subitem 5",
          context: loader.cm.URLContext(TEST_DOC_URL)
        }),
        hiddenItems[1]
      ]
    }),
    loader.cm.Menu({
      label: "menu 5",
      items: [
        loader.cm.Item({
          label: "subitem 7",
          context: loader.cm.URLContext(TEST_DOC_URL)
        }),
        loader.cm.Item({
          label: "subitem 8",
          contentScript: 'self.on("context", function () true);'
        })
      ]
    })
  ];

  test.withTestDoc(function (window, doc) {
    test.showMenu(null, function (popup) {
      test.checkMenu(items, hiddenItems, []);
      test.done();
    });
  });
};


// Child items should default to visible, not to PageContext
exports.testSubItemDefaultVisible = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let items = [
    loader.cm.Menu({
      label: "menu 1",
      context: loader.cm.SelectorContext("img"),
      items: [
        loader.cm.Item({
          label: "subitem 1"
        }),
        loader.cm.Item({
          label: "subitem 2",
          context: loader.cm.SelectorContext("img")
        }),
        loader.cm.Item({
          label: "subitem 3",
          context: loader.cm.SelectorContext("a")
        })
      ]
    })
  ];

  // subitem 3 will be hidden
  let hiddenItems = [items[0].items[2]];

  test.withTestDoc(function (window, doc) {
    test.showMenu("#image", function (popup) {
      test.checkMenu(items, hiddenItems, []);
      test.done();
    });
  });
};

// Tests that the click event on sub menuitem
// tiggers the click event for the sub menuitem and the parent menu
exports.testSubItemClick = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let state = 0;

  let items = [
    loader.cm.Menu({
      label: "menu 1",
      items: [
        loader.cm.Item({
          label: "subitem 1",
          data: "foobar",
          contentScript: 'self.on("click", function (node, data) {' +
                         '  self.postMessage({' +
                         '    tagName: node.tagName,' +
                         '    data: data' +
                         '  });' +
                         '});',
          onMessage: function(msg) {
            assert.equal(msg.tagName, "HTML", "should have seen the right node");
            assert.equal(msg.data, "foobar", "should have seen the right data");
            assert.equal(state, 0, "should have seen the event at the right time");
            state++;
          }
        })
      ],
      contentScript: 'self.on("click", function (node, data) {' +
                     '  self.postMessage({' +
                     '    tagName: node.tagName,' +
                     '    data: data' +
                     '  });' +
                     '});',
      onMessage: function(msg) {
        assert.equal(msg.tagName, "HTML", "should have seen the right node");
        assert.equal(msg.data, "foobar", "should have seen the right data");
        assert.equal(state, 1, "should have seen the event at the right time");

        test.done();
      }
    })
  ];

  test.withTestDoc(function (window, doc) {
    test.showMenu(null, function (popup) {
      test.checkMenu(items, [], []);

      let topMenuElt = test.getItemElt(popup, items[0]);
      let topMenuPopup = topMenuElt.firstChild;
      let itemElt = test.getItemElt(topMenuPopup, items[0].items[0]);
      itemElt.click();
    });
  });
};

// Tests that the command event on sub menuitem
// tiggers the click event for the sub menuitem and the parent menu
exports.testSubItemCommand = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let state = 0;

  let items = [
    loader.cm.Menu({
      label: "menu 1",
      items: [
        loader.cm.Item({
          label: "subitem 1",
          data: "foobar",
          contentScript: 'self.on("click", function (node, data) {' +
                         '  self.postMessage({' +
                         '    tagName: node.tagName,' +
                         '    data: data' +
                         '  });' +
                         '});',
          onMessage: function(msg) {
            assert.equal(msg.tagName, "HTML", "should have seen the right node");
            assert.equal(msg.data, "foobar", "should have seen the right data");
            assert.equal(state, 0, "should have seen the event at the right time");
            state++;
          }
        })
      ],
      contentScript: 'self.on("click", function (node, data) {' +
                     '  self.postMessage({' +
                     '    tagName: node.tagName,' +
                     '    data: data' +
                     '  });' +
                     '});',
      onMessage: function(msg) {
        assert.equal(msg.tagName, "HTML", "should have seen the right node");
        assert.equal(msg.data, "foobar", "should have seen the right data");
        assert.equal(state, 1, "should have seen the event at the right time");
        state++

        test.done();
      }
    })
  ];

  test.withTestDoc(function (window, doc) {
    test.showMenu(null, function (popup) {
      test.checkMenu(items, [], []);

      let topMenuElt = test.getItemElt(popup, items[0]);
      let topMenuPopup = topMenuElt.firstChild;
      let itemElt = test.getItemElt(topMenuPopup, items[0].items[0]);

      // create a command event
      let evt = itemElt.ownerDocument.createEvent('Event');
      evt.initEvent('command', true, true);
      itemElt.dispatchEvent(evt);
    });
  });
};

// Tests that opening a context menu for an outer frame when an inner frame
// has a selection doesn't activate the SelectionContext
exports.testSelectionInInnerFrameNoMatch = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let state = 0;

  let items = [
    loader.cm.Item({
      label: "test item",
      context: loader.cm.SelectionContext()
    })
  ];

  test.withTestDoc(function (window, doc) {
    let frame = doc.getElementById("iframe");
    frame.contentWindow.getSelection().selectAllChildren(frame.contentDocument.body);

    test.showMenu(null, function (popup) {
      test.checkMenu(items, items, []);
      test.done();
    });
  });
};

// Tests that opening a context menu for an inner frame when the inner frame
// has a selection does activate the SelectionContext
exports.testSelectionInInnerFrameMatch = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let state = 0;

  let items = [
    loader.cm.Item({
      label: "test item",
      context: loader.cm.SelectionContext()
    })
  ];

  test.withTestDoc(function (window, doc) {
    let frame = doc.getElementById("iframe");
    frame.contentWindow.getSelection().selectAllChildren(frame.contentDocument.body);

    test.showMenu(["#iframe", "#text"], function (popup) {
      test.checkMenu(items, [], []);
      test.done();
    });
  });
};

// Tests that opening a context menu for an inner frame when the outer frame
// has a selection doesn't activate the SelectionContext
exports.testSelectionInOuterFrameNoMatch = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let state = 0;

  let items = [
    loader.cm.Item({
      label: "test item",
      context: loader.cm.SelectionContext()
    })
  ];

  test.withTestDoc(function (window, doc) {
    let frame = doc.getElementById("iframe");
    window.getSelection().selectAllChildren(doc.body);

    test.showMenu(["#iframe", "#text"], function (popup) {
      test.checkMenu(items, items, []);
      test.done();
    });
  });
};


// Test that the return value of the predicate function determines if
// item is shown
exports.testPredicateContextControl = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let itemTrue = loader.cm.Item({
    label: "visible",
    context: loader.cm.PredicateContext(function () { return true; })
  });

  let itemFalse = loader.cm.Item({
    label: "hidden",
    context: loader.cm.PredicateContext(function () { return false; })
  });

  test.showMenu(null, function (popup) {
    test.checkMenu([itemTrue, itemFalse], [itemFalse], []);
    test.done();
  });
};

// Test that the data object has the correct document type
exports.testPredicateContextDocumentType = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let items = [loader.cm.Item({
    label: "item",
    context: loader.cm.PredicateContext(function (data) {
      assert.equal(data.documentType, 'text/html');
      return true;
    })
  })];

  test.withTestDoc(function (window, doc) {
    test.showMenu(null, function (popup) {
      test.checkMenu(items, [], []);
      test.done();
    });
  });
};

// Test that the data object has the correct document URL
exports.testPredicateContextDocumentURL = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let items = [loader.cm.Item({
    label: "item",
    context: loader.cm.PredicateContext(function (data) {
      assert.equal(data.documentURL, TEST_DOC_URL);
      return true;
    })
  })];

  test.withTestDoc(function (window, doc) {
    test.showMenu(null, function (popup) {
      test.checkMenu(items, [], []);
      test.done();
    });
  });
};


// Test that the data object has the correct element name
exports.testPredicateContextTargetName = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let items = [loader.cm.Item({
    label: "item",
    context: loader.cm.PredicateContext(function (data) {
      assert.strictEqual(data.targetName, "input");
      return true;
    })
  })];

  test.withTestDoc(function (window, doc) {
    test.showMenu("#button", function (popup) {
      test.checkMenu(items, [], []);
      test.done();
    });
  });
};


// Test that the data object has the correct ID
exports.testPredicateContextTargetIDSet = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let items = [loader.cm.Item({
    label: "item",
    context: loader.cm.PredicateContext(function (data) {
      assert.strictEqual(data.targetID, "button");
      return true;
    })
  })];

  test.withTestDoc(function (window, doc) {
    test.showMenu("#button", function (popup) {
      test.checkMenu(items, [], []);
      test.done();
    });
  });
};

// Test that the data object has the correct ID
exports.testPredicateContextTargetIDNotSet = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let items = [loader.cm.Item({
    label: "item",
    context: loader.cm.PredicateContext(function (data) {
      assert.strictEqual(data.targetID, null);
      return true;
    })
  })];

  test.withTestDoc(function (window, doc) {
    test.showMenu(".predicate-test-a", function (popup) {
      test.checkMenu(items, [], []);
      test.done();
    });
  });
};

// Test that the data object is showing editable correctly for regular text inputs
exports.testPredicateContextTextBoxIsEditable = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let items = [loader.cm.Item({
    label: "item",
    context: loader.cm.PredicateContext(function (data) {
      assert.strictEqual(data.isEditable, true);
      return true;
    })
  })];

  test.withTestDoc(function (window, doc) {
    test.showMenu("#textbox", function (popup) {
      test.checkMenu(items, [], []);
      test.done();
    });
  });
};

// Test that the data object is showing editable correctly for readonly text inputs
exports.testPredicateContextReadonlyTextBoxIsNotEditable = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let items = [loader.cm.Item({
    label: "item",
    context: loader.cm.PredicateContext(function (data) {
      assert.strictEqual(data.isEditable, false);
      return true;
    })
  })];

  test.withTestDoc(function (window, doc) {
    test.showMenu("#readonly-textbox", function (popup) {
      test.checkMenu(items, [], []);
      test.done();
    });
  });
};

// Test that the data object is showing editable correctly for disabled text inputs
exports.testPredicateContextDisabledTextBoxIsNotEditable = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let items = [loader.cm.Item({
    label: "item",
    context: loader.cm.PredicateContext(function (data) {
      assert.strictEqual(data.isEditable, false);
      return true;
    })
  })];

  test.withTestDoc(function (window, doc) {
    test.showMenu("#disabled-textbox", function (popup) {
      test.checkMenu(items, [], []);
      test.done();
    });
  });
};

// Test that the data object is showing editable correctly for text areas
exports.testPredicateContextTextAreaIsEditable = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let items = [loader.cm.Item({
    label: "item",
    context: loader.cm.PredicateContext(function (data) {
      assert.strictEqual(data.isEditable, true);
      return true;
    })
  })];

  test.withTestDoc(function (window, doc) {
    test.showMenu("#textfield", function (popup) {
      test.checkMenu(items, [], []);
      test.done();
    });
  });
};

// Test that non-text inputs are not considered editable
exports.testPredicateContextButtonIsNotEditable = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let items = [loader.cm.Item({
    label: "item",
    context: loader.cm.PredicateContext(function (data) {
      assert.strictEqual(data.isEditable, false);
      return true;
    })
  })];

  test.withTestDoc(function (window, doc) {
    test.showMenu("#button", function (popup) {
      test.checkMenu(items, [], []);
      test.done();
    });
  });
};


// Test that the data object is showing editable correctly
exports.testPredicateContextNonInputIsNotEditable = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let items = [loader.cm.Item({
    label: "item",
    context: loader.cm.PredicateContext(function (data) {
      assert.strictEqual(data.isEditable, false);
      return true;
    })
  })];

  test.withTestDoc(function (window, doc) {
    test.showMenu("#image", function (popup) {
      test.checkMenu(items, [], []);
      test.done();
    });
  });
};


// Test that the data object is showing editable correctly for HTML contenteditable elements
exports.testPredicateContextEditableElement = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let items = [loader.cm.Item({
    label: "item",
    context: loader.cm.PredicateContext(function (data) {
      assert.strictEqual(data.isEditable, true);
      return true;
    })
  })];

  test.withTestDoc(function (window, doc) {
    test.showMenu("#editable", function (popup) {
      test.checkMenu(items, [], []);
      test.done();
    });
  });
};


// Test that the data object does not have a selection when there is none
exports.testPredicateContextNoSelectionInPage = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let items = [loader.cm.Item({
    label: "item",
    context: loader.cm.PredicateContext(function (data) {
      assert.strictEqual(data.selectionText, null);
      return true;
    })
  })];

  test.withTestDoc(function (window, doc) {
    test.showMenu(null, function (popup) {
      test.checkMenu(items, [], []);
      test.done();
    });
  });
};

// Test that the data object includes the selected page text
exports.testPredicateContextSelectionInPage = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let items = [loader.cm.Item({
    label: "item",
    context: loader.cm.PredicateContext(function (data) {
      // since we might get whitespace
      assert.ok(data.selectionText && data.selectionText.search(/^\s*Some text.\s*$/) != -1,
		'Expected "Some text.", got "' + data.selectionText + '"');
      return true;
    })
  })];

  test.withTestDoc(function (window, doc) {
    window.getSelection().selectAllChildren(doc.getElementById("text"));
    test.showMenu(null, function (popup) {
      test.checkMenu(items, [], []);
      test.done();
    });
  });
};

// Test that the data object includes the selected input text
exports.testPredicateContextSelectionInTextBox = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let items = [loader.cm.Item({
    label: "item",
    context: loader.cm.PredicateContext(function (data) {
      // since we might get whitespace
      assert.strictEqual(data.selectionText, "t v");
      return true;
    })
  })];

  test.withTestDoc(function (window, doc) {
    let textbox = doc.getElementById("textbox");
    test.selectRange("#textbox", 3, 6);
    test.showMenu("#textbox", function (popup) {
      test.checkMenu(items, [], []);
      test.done();
    });
  });
};

// Test that the data object has the correct src for an image
exports.testPredicateContextTargetSrcSet = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();
  let image;

  let items = [loader.cm.Item({
    label: "item",
    context: loader.cm.PredicateContext(function (data) {
      assert.strictEqual(data.srcURL, image.src);
      return true;
    })
  })];

  test.withTestDoc(function (window, doc) {
    image = doc.getElementById("image");
    test.showMenu("#image", function (popup) {
      test.checkMenu(items, [], []);
      test.done();
    });
  });
};

// Test that the data object has no src for a link
exports.testPredicateContextTargetSrcNotSet = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let items = [loader.cm.Item({
    label: "item",
    context: loader.cm.PredicateContext(function (data) {
      assert.strictEqual(data.srcURL, null);
      return true;
    })
  })];

  test.withTestDoc(function (window, doc) {
    test.showMenu("#link", function (popup) {
      test.checkMenu(items, [], []);
      test.done();
    });
  });
};


// Test that the data object has the correct link set
exports.testPredicateContextTargetLinkSet = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();
  let image;

  let items = [loader.cm.Item({
    label: "item",
    context: loader.cm.PredicateContext(function (data) {
      assert.strictEqual(data.linkURL, TEST_DOC_URL + "#test");
      return true;
    })
  })];

  test.withTestDoc(function (window, doc) {
    test.showMenu(".predicate-test-a", function (popup) {
      test.checkMenu(items, [], []);
      test.done();
    });
  });
};

// Test that the data object has no link for an image
exports.testPredicateContextTargetLinkNotSet = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let items = [loader.cm.Item({
    label: "item",
    context: loader.cm.PredicateContext(function (data) {
      assert.strictEqual(data.linkURL, null);
      return true;
    })
  })];

  test.withTestDoc(function (window, doc) {
    test.showMenu("#image", function (popup) {
      test.checkMenu(items, [], []);
      test.done();
    });
  });
};

// Test that the data object has the correct link for a nested image
exports.testPredicateContextTargetLinkSetNestedImage = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let items = [loader.cm.Item({
    label: "item",
    context: loader.cm.PredicateContext(function (data) {
      assert.strictEqual(data.linkURL, TEST_DOC_URL + "#nested-image");
      return true;
    })
  })];

  test.withTestDoc(function (window, doc) {
    test.showMenu("#predicate-test-nested-image", function (popup) {
      test.checkMenu(items, [], []);
      test.done();
    });
  });
};

// Test that the data object has the correct link for a complex nested structure
exports.testPredicateContextTargetLinkSetNestedStructure = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let items = [loader.cm.Item({
    label: "item",
    context: loader.cm.PredicateContext(function (data) {
      assert.strictEqual(data.linkURL, TEST_DOC_URL + "#nested-structure");
      return true;
    })
  })];

  test.withTestDoc(function (window, doc) {
    test.showMenu("#predicate-test-nested-structure", function (popup) {
      test.checkMenu(items, [], []);
      test.done();
    });
  });
};

// Test that the data object has the value for an input textbox
exports.testPredicateContextTargetValueSet = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();
  let image;

  let items = [loader.cm.Item({
    label: "item",
    context: loader.cm.PredicateContext(function (data) {
      assert.strictEqual(data.value, "test value");
      return true;
    })
  })];

  test.withTestDoc(function (window, doc) {
    test.showMenu("#textbox", function (popup) {
      test.checkMenu(items, [], []);
      test.done();
    });
  });
};

// Test that the data object has no value for an image
exports.testPredicateContextTargetValueNotSet = function (assert, done) {
  let test = new TestHelper(assert, done);
  let loader = test.newLoader();

  let items = [loader.cm.Item({
    label: "item",
    context: loader.cm.PredicateContext(function (data) {
      assert.strictEqual(data.value, null);
      return true;
    })
  })];

  test.withTestDoc(function (window, doc) {
    test.showMenu("#image", function (popup) {
      test.checkMenu(items, [], []);
      test.done();
    });
  });
};

if (packaging.isNative) {
  module.exports = {
    "test skip on jpm": (assert) => assert.pass("skipping this file with jpm")
  };
}

require('sdk/test').run(exports);
