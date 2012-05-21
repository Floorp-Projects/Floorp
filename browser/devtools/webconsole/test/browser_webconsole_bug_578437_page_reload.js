/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests that the console object still exists after a page reload.
const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-console.html";

function test() {
  addTab(TEST_URI);
  browser.addEventListener("DOMContentLoaded", onLoad, false);
}

function onLoad() {
  browser.removeEventListener("DOMContentLoaded", onLoad, false);

  openConsole();

  browser.addEventListener("DOMContentLoaded", testPageReload, false);
  content.location.reload();
}

function testPageReload() {

  browser.removeEventListener("DOMContentLoaded", testPageReload, false);

  let console = browser.contentWindow.wrappedJSObject.console;

  is(typeof console, "object", "window.console is an object, after page reload");
  is(typeof console.log, "function", "console.log is a function");
  is(typeof console.info, "function", "console.info is a function");
  is(typeof console.warn, "function", "console.warn is a function");
  is(typeof console.error, "function", "console.error is a function");
  is(typeof console.exception, "function", "console.exception is a function");

  finishTest();
}

