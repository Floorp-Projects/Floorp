/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the console object still exists after a page reload.

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/test-console.html";

var browser;

function test() {
  loadTab(TEST_URI).then(() => {
    openConsole().then((tab) => {
      browser = tab.browser;

      browser.addEventListener("DOMContentLoaded", testPageReload, false);
      content.location.reload();
    });
  });
  browser.addEventListener("DOMContentLoaded", onLoad, false);
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

  browser = null;
  finishTest();
}
