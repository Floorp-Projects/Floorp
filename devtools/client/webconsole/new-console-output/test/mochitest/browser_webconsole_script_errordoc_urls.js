/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Ensure that [Learn More] links appear alongside any errors listed
// in "errordocs.js". Note: this only tests script execution.

"use strict";

const ErrorDocs = require("devtools/server/actors/errordocs");

function makeURIData(script) {
  return `data:text/html;charset=utf8,<script>${script}</script>`;
}

const TestData = [
  {
    jsmsg: "JSMSG_READ_ONLY",
    script: "'use strict'; (Object.freeze({name: 'Elsa', score: 157})).score = 0;",
    isException: true,
  },
  {
    jsmsg: "JSMSG_STMT_AFTER_RETURN",
    script: "function a() { return; 1 + 1; };",
    isException: false,
  }
];

add_task(function* () {
  yield loadTab("data:text/html;charset=utf8,errordoc tests");

  let hud = yield openConsole();

  for (let i = 0; i < TestData.length; i++) {
    yield testScriptError(hud, TestData[i]);
  }
});

function* testScriptError(hud, testData) {
  if (testData.isException === true) {
    expectUncaughtException();
  }

  BrowserTestUtils.loadURI(gBrowser.selectedBrowser, makeURIData(testData.script));

  yield waitForMessages({
    webconsole: hud,
    messages: [
      {
        category: CATEGORY_JS
      }
    ]
  });

  // grab the most current error doc URL
  let url = ErrorDocs.GetURL({ errorMessageName: testData.jsmsg });

  let hrefs = {};
  for (let link of hud.jsterm.outputNode.querySelectorAll("a")) {
    hrefs[link.href] = true;
  }

  ok(url in hrefs, `Expected a link to ${url}.`);

  hud.jsterm.clearOutput();
}
