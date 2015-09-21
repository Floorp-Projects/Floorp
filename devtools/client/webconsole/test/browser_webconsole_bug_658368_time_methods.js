/* vim:set ts=2 sw=2 sts=2 et: */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests that the Console API implements the time() and timeEnd() methods.

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/test-bug-658368-time-methods.html";

const TEST_URI2 = "data:text/html;charset=utf-8,<script>" +
                  "console.timeEnd('bTimer');</script>";

const TEST_URI3 = "data:text/html;charset=utf-8,<script>" +
                  "console.time('bTimer');</script>";

const TEST_URI4 = "data:text/html;charset=utf-8," +
                  "<script>console.timeEnd('bTimer');</script>";

var test = asyncTest(function* () {
  yield loadTab(TEST_URI);

  let hud1 = yield openConsole();

  yield waitForMessages({
    webconsole: hud1,
    messages: [{
      name: "aTimer started",
      consoleTime: "aTimer",
    }, {
      name: "aTimer end",
      consoleTimeEnd: "aTimer",
    }],
  });

  // The next test makes sure that timers with the same name but in separate
  // tabs, do not contain the same value.
  let { browser } = yield loadTab(TEST_URI2);
  let hud2 = yield openConsole();

  testLogEntry(hud2.outputNode, "bTimer: timer started",
               "bTimer was not started", false, true);

  // The next test makes sure that timers with the same name but in separate
  // pages, do not contain the same value.
  content.location = TEST_URI3;

  yield waitForMessages({
    webconsole: hud2,
    messages: [{
      name: "bTimer started",
      consoleTime: "bTimer",
    }],
  });

  hud2.jsterm.clearOutput();

  // Now the following console.timeEnd() call shouldn't display anything,
  // if the timers in different pages are not related.
  content.location = TEST_URI4;
  yield loadBrowser(browser);

  testLogEntry(hud2.outputNode, "bTimer: timer started",
               "bTimer was not started", false, true);
});
