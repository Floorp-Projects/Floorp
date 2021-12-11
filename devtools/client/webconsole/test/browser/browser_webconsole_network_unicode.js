/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that Unicode characters within the domain are displayed
// encoded and not in Punycode or somehow garbled.

"use strict";

const TEST_URL =
  "http://example.com/browser/devtools/client/webconsole/test/browser/test-network-request.html";

add_task(async function() {
  await pushPref("devtools.webconsole.filter.netxhr", true);

  const toolbox = await openNewTabAndToolbox(TEST_URL, "webconsole");
  const hud = toolbox.getCurrentPanel().hud;

  const onMessage = waitForMessage(hud, "testxhr", ".network");

  const XHR_TEST_URL_WITHOUT_PARAMS = "http://flüge.example.com/testxhr";
  const XHR_TEST_URL = XHR_TEST_URL_WITHOUT_PARAMS + "?foo";
  SpecialPowers.spawn(gBrowser.selectedBrowser, [XHR_TEST_URL], url => {
    content.fetch(url);
  });

  info("Wait for expected messages to appear");
  const message = await onMessage;

  const urlNode = message.node.querySelector(".url");
  is(
    urlNode.textContent,
    XHR_TEST_URL,
    "The network call is displayed with the expected URL"
  );
});
