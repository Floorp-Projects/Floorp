/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that messages are logged and observed with the correct category. See Bug 595934.

const TEST_URI = "data:text/html;charset=utf-8,Web Console test for " +
                 "bug 595934 - message categories coverage.";
const TESTS_PATH = "http://example.com/browser/devtools/client/webconsole/" +
                   "test/mochitest/";
const TESTS = [
  {
    // #0
    file: "test-message-categories-css-loader.html",
    category: "CSS Loader",
    matchString: "text/css",
  },
  {
    // #1
    file: "test-message-categories-imagemap.html",
    category: "Layout: ImageMap",
    matchString: "shape=\"rect\"",
  },
  {
    // #2
    file: "test-message-categories-html.html",
    category: "HTML",
    matchString: "multipart/form-data",
    onload: function() {
      ContentTask.spawn(gBrowser.selectedBrowser, null, async function() {
        const form = content.document.querySelector("form");
        form.submit();
      });
    },
  },
  {
    // #3
    file: "test-message-categories-workers.html",
    category: "Web Worker",
    matchString: "fooBarWorker",
  },
  {
    // #4
    file: "test-message-categories-malformedxml.xhtml",
    category: "malformed-xml",
    matchString: "no root element found",
  },
  {
    // #5
    file: "test-message-categories-svg.xhtml",
    category: "SVG",
    matchString: "fooBarSVG",
  },
  {
    // #6
    file: "test-message-categories-css-parser.html",
    category: "CSS Parser",
    matchString: "foobarCssParser",
  },
  {
    // #7
    file: "test-message-categories-malformedxml-external.html",
    category: "malformed-xml",
    matchString: "</html>",
  },
  {
    // #8
    file: "test-message-categories-empty-getelementbyid.html",
    category: "DOM",
    matchString: "getElementById",
  },
  {
    // #9
    file: "test-message-categories-canvas-css.html",
    category: "CSS Parser",
    matchString: "foobarCanvasCssParser",
  },
  {
    // #10
    file: "test-message-categories-image.html",
    category: "Image",
    matchString: "corrupt",
    // This message is not displayed in the main console in e10s. Bug 1431731
    skipInE10s: true,
  },
];

add_task(async function() {
  requestLongerTimeout(2);

  await pushPref("devtools.webconsole.filter.css", true);
  await pushPref("devtools.webconsole.filter.net", true);

  const hud = await openNewTabAndConsole(TEST_URI);
  for (let i = 0; i < TESTS.length; i++) {
    const test = TESTS[i];
    info("Running test #" + i);
    await runTest(test, hud);
  }
});

async function runTest(test, hud) {
  const {file, category, matchString, onload, skipInE10s} = test;

  if (skipInE10s && Services.appinfo.browserTabsRemoteAutostart) {
    return;
  }

  const onMessageLogged = waitForMessage(hud, matchString);

  const onMessageObserved = new Promise(resolve => {
    Services.console.registerListener(function listener(subject) {
      if (!(subject instanceof Ci.nsIScriptError)) {
        return;
      }

      if (subject.category != category) {
        return;
      }

      ok(true, "Expected category [" + category + "] received in observer");
      Services.console.unregisterListener(listener);
      resolve();
    });
  });

  info("Load test file " + file);
  await loadDocument(TESTS_PATH + file);

  // Call test specific callback if defined
  if (onload) {
    onload();
  }

  info("Wait for log message to be observed with the correct category");
  await onMessageObserved;

  info("Wait for log message to be displayed in the hud");
  await onMessageLogged;
}
