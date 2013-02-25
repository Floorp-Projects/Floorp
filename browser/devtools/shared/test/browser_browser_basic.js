/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests exports from Browser.jsm

const TEST_URI = "data:text/html;charset=utf-8,<p id=id>Text</p>";

let imported = {};
Components.utils.import("resource:///modules/devtools/Browser.jsm", imported);

registerCleanupFunction(function tearDown() {
  imported = undefined;
});

function test() {
  addTab(TEST_URI, function(browser, tab, document) {
    runTest(browser, tab, document);
  });
}

function runTest(browser, tab, document) {
  let timeout = imported.setTimeout(shouldNotBeCalled, 100);
  imported.clearTimeout(timeout);

  var p = document.getElementById("id");

  ok(p instanceof imported.Node, "Node correctly defined");
  ok(p instanceof imported.HTMLElement, "HTMLElement correctly defined");

  let timeout = imported.setTimeout(function() {
    finish();
  }, 100);
}

function shouldNotBeCalled() {
  ok(false, "Timeout cleared");
}
