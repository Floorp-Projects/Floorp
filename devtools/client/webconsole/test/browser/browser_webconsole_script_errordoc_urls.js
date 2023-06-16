/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Ensure that [Learn More] links appear alongside any errors listed
// in "errordocs.js". Note: this only tests script execution.

"use strict";

const ErrorDocs = require("resource://devtools/server/actors/errordocs.js");
const TEST_URI = "data:text/html;charset=utf8,<!DOCTYPE html>errordoc tests";

function makeURIData(script) {
  return `data:text/html;charset=utf8,<!DOCTYPE html><script>${script}</script>`;
}

const TestData = [
  {
    jsmsg: "JSMSG_READ_ONLY",
    script:
      "'use strict'; (Object.freeze({name: 'Elsa', score: 157})).score = 0;",
    selector: ".error",
    isException: true,
    expected: 'TypeError: "score" is read-only',
  },
  {
    jsmsg: "JSMSG_STMT_AFTER_RETURN",
    script: "function a() { return; 1 + 1; };",
    selector: ".warn",
    isException: false,
    expected: "unreachable code after return statement",
  },
];

add_task(async function () {
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

  await navigateTo(makeURIData(testData.script));

  const msg = "the expected error message was displayed";
  info(`waiting for ${msg} to be displayed`);
  await waitFor(() =>
    findMessageByType(hud, testData.expected, testData.selector)
  );
  ok(true, msg);

  // grab the most current error doc URL.
  const urlObj = new URL(
    ErrorDocs.GetURL({ errorMessageName: testData.jsmsg })
  );

  // strip all params from the URL.
  const url = `${urlObj.origin}${urlObj.pathname}`;

  // Gather all URLs displayed in the console. [Learn More] links have no href
  // but have the URL in the title attribute.
  const hrefs = new Set();
  for (const link of hud.ui.outputNode.querySelectorAll("a")) {
    hrefs.add(link.title);
  }

  ok(hrefs.has(url), `Expected a link to ${url}.`);

  await clearOutput(hud);
}
