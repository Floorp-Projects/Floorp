/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test to see if retrieving cached messages in a page with a cross-domain iframe does
// not crash the console.

"use strict";

const TEST_URI =
  "http://example.com/browser/devtools/client/webconsole/" +
  "test/browser/test-iframe-parent.html";

add_task(async function() {
  // test-iframe-parent has an iframe pointing to http://mochi.test:8888/browser/devtools/client/webconsole/test/browser/test-iframe-child.html
  info("Open the tab first");
  await addTab(TEST_URI);

  info("Evaluate an expression that will throw, so we'll have cached messages");
  await ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
    content.wrappedJSObject.document.querySelector("button").click();
  });

  info("Then open the console, to retrieve cached messages");
  await openConsole();

  // TODO: Make the test fail without the fix.
  ok(true, "Everything is okay");
});
