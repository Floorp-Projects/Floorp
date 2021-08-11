"use strict";

const kTestPath = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);
const kTestXFOEmbedURI = kTestPath + "file_framing_xfo_embed.html";
const kTestXFOObjectURI = kTestPath + "file_framing_xfo_object.html";

const errorMessage = `The loading of “https://example.com/browser/dom/security/test/general/file_framing_xfo_embed_object.sjs” in a frame is denied by “X-Frame-Options“ directive set to “deny“`;

let xfoBlocked = false;

function onXFOMessage(msgObj) {
  const message = msgObj.message;

  if (message.includes(errorMessage)) {
    ok(true, "XFO error message logged");
    xfoBlocked = true;
  }
}

add_task(async function open_test_xfo_embed_blocked() {
  xfoBlocked = false;
  await BrowserTestUtils.withNewTab("about:blank", async function(browser) {
    Services.console.registerListener(onXFOMessage);
    BrowserTestUtils.loadURI(browser, kTestXFOEmbedURI);
    await BrowserTestUtils.waitForCondition(() => xfoBlocked);
    Services.console.unregisterListener(onXFOMessage);
  });
});

add_task(async function open_test_xfo_object_blocked() {
  xfoBlocked = false;
  await BrowserTestUtils.withNewTab("about:blank", async function(browser) {
    Services.console.registerListener(onXFOMessage);
    BrowserTestUtils.loadURI(browser, kTestXFOObjectURI);
    await BrowserTestUtils.waitForCondition(() => xfoBlocked);
    Services.console.unregisterListener(onXFOMessage);
  });
});
