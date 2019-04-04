/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the Web Console shows weak crypto warnings (SHA-1 Certificate)

"use strict";

const TEST_URI = "data:text/html;charset=utf8,Web Console weak crypto warnings test";
const TEST_URI_PATH = "/browser/devtools/client/webconsole/test/" +
                      "mochitest/test-certificate-messages.html";

const SHA1_URL = "https://sha1ee.example.com" + TEST_URI_PATH;
const SHA256_URL = "https://sha256ee.example.com" + TEST_URI_PATH;
const TRIGGER_MSG = "If you haven't seen ssl warnings yet, you won't";
const TLS_1_0_URL = "https://tls1.example.com" + TEST_URI_PATH;

const TLS_expected_message = "This site uses a deprecated version of TLS that"
+ " will be disabled in March 2020. Please upgrade"
+ " to TLS 1.2 or 1.3.";

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);

  info("Test SHA1 warnings");
  let onContentLog = waitForMessage(hud, TRIGGER_MSG);
  const onSha1Warning = waitForMessage(hud, "SHA-1");
  await loadDocument(SHA1_URL);
  await Promise.all([onContentLog, onSha1Warning]);

  let {textContent} = hud.outputNode;
  ok(!textContent.includes("SSL 3.0"), "There is no warning message for SSL 3.0");
  ok(!textContent.includes("RC4"), "There is no warning message for RC4");

  info("Test SSL warnings appropriately not present");
  onContentLog = waitForMessage(hud, TRIGGER_MSG);
  await loadDocument(SHA256_URL);
  await onContentLog;

  textContent = hud.outputNode.textContent;
  ok(!textContent.includes("SHA-1"), "There is no warning message for SHA-1");
  ok(!textContent.includes("SSL 3.0"), "There is no warning message for SSL 3.0");
  ok(!textContent.includes("RC4"), "There is no warning message for RC4");
  ok(!textContent.includes(TLS_expected_message), "There is not TLS warning message");

  info("Test TLS warnings");
  onContentLog = waitForMessage(hud, TRIGGER_MSG);
  await loadDocument(TLS_1_0_URL);
  await onContentLog;

  textContent = hud.outputNode.textContent;
  ok(textContent.includes(TLS_expected_message), "TLS warning message found");

  Services.cache2.clear();
});
