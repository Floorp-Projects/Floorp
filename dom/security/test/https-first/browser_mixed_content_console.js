// Bug 1713593: HTTPS-First: Add test for mixed content blocker.
"use strict";

const testPath = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "http://example.com"
);

let threeMessagesArrived = 0;
let messageImageSeen = false;

const kTestURI = testPath + "file_mixed_content_console.html";

add_task(async function() {
  // A longer timeout is necessary for this test than the plain mochitests
  // due to opening a new tab with the web console.
  requestLongerTimeout(4);

  // Enable HTTPS-First Mode and register console-listener
  await SpecialPowers.pushPrefEnv({
    set: [["dom.security.https_first", true]],
  });
  Services.console.registerListener(on_console_message);
  BrowserTestUtils.loadURI(gBrowser.selectedBrowser, kTestURI);

  await BrowserTestUtils.waitForCondition(() => threeMessagesArrived === 3);

  Services.console.unregisterListener(on_console_message);
});

function on_console_message(msgObj) {
  const message = msgObj.message;

  // The first console message is:
  // "HTTPS-First Mode: Upgrading insecure request
  // ‘http://example.com/browser/dom/security/test/https-first/file_mixed_content_console.html’ to use ‘https’"
  if (message.includes("HTTPS-First Mode:")) {
    ok(message.includes("Upgrading insecure request"), "request got upgraded");
    ok(
      message.includes(
        "“http://example.com/browser/dom/security/test/https-first/file_mixed_content_console.html” to use “https”."
      ),
      "correct top-level request"
    );
    threeMessagesArrived++;
  }
  // The second console message is:
  // "Loading mixed (insecure) display content
  // “http://example.com/browser/dom/security/test/https-first/auto_upgrading_identity.png” on a secure page".
  // Since the message is send twice, prevent reading the image message two times
  else if (message.includes("Loading mixed") && !messageImageSeen) {
    ok(
      message.includes("Loading mixed (insecure) display content"),
      "display content got load"
    );
    ok(
      message.includes(
        "“http://example.com/browser/dom/security/test/https-first/auto_upgrading_identity.png” on a secure page"
      ),
      "img loaded insecure"
    );
    threeMessagesArrived++;
    messageImageSeen = true;
  }
  // The third message is:
  // "Blocked loading mixed active content
  // "http://example.com/browser/dom/security/test/https-first/barfoo""
  else if (message.includes("Blocked loading")) {
    ok(
      message.includes("Blocked loading mixed active content"),
      "script got blocked"
    );
    ok(
      message.includes(
        "http://example.com/browser/dom/security/test/https-first/barfoo"
      ),
      "the right script got blocked"
    );
    threeMessagesArrived++;
  }
}
