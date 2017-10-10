/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the Web Console shows weak crypto warnings (SHA-1 Certificate)

"use strict";

const TEST_URI = "data:text/html;charset=utf8,Web Console weak crypto warnings test";
const TEST_URI_PATH = "/browser/devtools/client/webconsole/new-console-output/test/" +
                      "mochitest/test-certificate-messages.html";

var tests = [{
  url: "https://sha1ee.example.com" + TEST_URI_PATH,
  name: "SHA1 warning displayed successfully",
  warning: ["SHA-1"],
  nowarning: ["SSL 3.0", "RC4"]
}, {
  url: "https://sha256ee.example.com" + TEST_URI_PATH,
  name: "SSL warnings appropriately not present",
  warning: [],
  nowarning: ["SHA-1", "SSL 3.0", "RC4"]
}];
const TRIGGER_MSG = "If you haven't seen ssl warnings yet, you won't";

add_task(async function () {
  const hud = await openNewTabAndConsole(TEST_URI);
  for (let test of tests) {
    await runTest(test, hud);
  }
});

async function runTest(test, hud) {
  info(`Testing ${test.name}`);
  await loadDocument(test.url);
  await waitFor(() => findMessage(hud, TRIGGER_MSG));

  for (let warning of test.warning) {
    ok(hud.outputNode.textContent.indexOf(warning) >= 0,
      `There is a warning message for ${warning}`);
  }

  for (let nowarning of test.nowarning) {
    ok(hud.outputNode.textContent.indexOf(nowarning) < 0,
      `There is no warning message for ${nowarning}`);
  }
}
