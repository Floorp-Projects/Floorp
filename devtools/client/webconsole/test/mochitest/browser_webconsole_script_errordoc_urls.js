/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Ensure that [Learn More] links appear alongside any errors listed
// in "errordocs.js". Note: this only tests script execution.

"use strict";

const ErrorDocs = require("devtools/server/actors/errordocs");
const TEST_URI = "data:text/html;charset=utf8,errordoc tests";

function makeURIData(script) {
  return `data:text/html;charset=utf8,<script>${script}</script>`;
}

const TestData = [
  {
    jsmsg: "JSMSG_READ_ONLY",
    script: "'use strict'; (Object.freeze({name: 'Elsa', score: 157})).score = 0;",
    isException: true,
    expected: 'TypeError: "score" is read-only',
  },
  {
    jsmsg: "JSMSG_STMT_AFTER_RETURN",
    script: "function a() { return; 1 + 1; };",
    isException: false,
    expected: "unreachable code after return statement",
  }
];

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);

  for (const data of TestData) {
    await testScriptError(hud, data);
  }
});

async function testScriptError(hud, testData) {
  const isE10s = Services.appinfo.browserTabsRemoteAutostart;
  if (testData.isException && !isE10s) {
    expectUncaughtException();
  }

  await loadDocument(makeURIData(testData.script));

  const msg = "the expected error message was displayed";
  info(`waiting for ${msg} to be displayed`);
  await waitFor(() => findMessage(hud, testData.expected));
  ok(true, msg);

  // grab the most current error doc URL.
  const urlObj = new URL(ErrorDocs.GetURL({ errorMessageName: testData.jsmsg }));

  // strip all params from the URL.
  const url = `${urlObj.origin}${urlObj.pathname}`;

  // Gather all URLs displayed in the console. [Learn More] links have no href
  // but have the URL in the title attribute.
  const hrefs = new Set();
  for (const link of hud.ui.outputNode.querySelectorAll("a")) {
    hrefs.add(link.title);
  }

  ok(hrefs.has(url), `Expected a link to ${url}.`);

  hud.jsterm.clearOutput();
}
