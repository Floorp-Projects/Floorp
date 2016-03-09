/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// When strings containing URLs are entered into the webconsole, check
// its output and ensure that the output can be clicked to open those URLs.

"use strict";

const inputTests = [

  // 0: URL opens page when clicked.
  {
    input: "'http://example.com'",
    output: "http://example.com",
    expectedTab: "http://example.com/",
  },

  // 1: URL opens page using https when clicked.
  {
    input: "'https://example.com'",
    output: "https://example.com",
    expectedTab: "https://example.com/",
  },

  // 2: URL with port opens page when clicked.
  {
    input: "'https://example.com:443'",
    output: "https://example.com:443",
    expectedTab: "https://example.com/",
  },

  // 3: URL containing non-empty path opens page when clicked.
  {
    input: "'http://example.com/foo'",
    output: "http://example.com/foo",
    expectedTab: "http://example.com/foo",
  },

  // 4: URL opens page when clicked, even when surrounded by non-URL tokens.
  {
    input: "'foo http://example.com bar'",
    output: "foo http://example.com bar",
    expectedTab: "http://example.com/",
  },

  // 5: URL opens page when clicked, and whitespace is be preserved.
  {
    input: "'foo\\nhttp://example.com\\nbar'",
    output: "foo\nhttp://example.com\nbar",
    expectedTab: "http://example.com/",
  },

  // 6: URL opens page when clicked when multiple links are present.
  {
    input: "'http://example.com http://example.com'",
    output: "http://example.com http://example.com",
    expectedTab: "http://example.com/",
  },

  // 7: URL without scheme does not open page when clicked.
  {
    input: "'example.com'",
    output: "example.com",
  },

  // 8: URL with invalid scheme does not open page when clicked.
  {
    input: "'foo://example.com'",
    output: "foo://example.com",
  },

  // 9: Shortened URL in an array
  {
    input: "['http://example.com/abcdefghijabcdefghij some other text']",
    output: "Array [ \"http://example.com/abcdefghijabcdef\u2026\" ]",
    printOutput: "http://example.com/abcdefghijabcdefghij some other text",
    expectedTab: "http://example.com/abcdefghijabcdefghij",
    getClickableNode: (msg) => msg.querySelectorAll("a")[1],
  },

  // 10: Shortened URL in an object
  {
    input: "{test: 'http://example.com/abcdefghijabcdefghij some other text'}",
    output: "Object { test: \"http://example.com/abcdefghijabcdef\u2026\" }",
    printOutput: "[object Object]",
    evalOutput: "http://example.com/abcdefghijabcdefghij some other text",
    noClick: true,
    consoleLogClick: true,
    expectedTab: "http://example.com/abcdefghijabcdefghij",
    getClickableNode: (msg) => msg.querySelectorAll("a")[1],
  },

];

const url = "data:text/html;charset=utf8,Bug 1005909 - Clickable URLS";

add_task(function* () {
  yield BrowserTestUtils.openNewForegroundTab(gBrowser, url);
  let hud = yield openConsole();
  yield checkOutputForInputs(hud, inputTests);
});
