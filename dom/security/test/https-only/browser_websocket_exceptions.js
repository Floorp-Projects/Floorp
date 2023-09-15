"use strict";

const TEST_PATH_HTTP = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "http://localhost:9898"
);

let WEBSOCKET_DOC_URL = `${TEST_PATH_HTTP}file_websocket_exceptions.html`;

add_task(async function () {
  // Here is a sequence of how this test works:
  // 1. Dynamically inject a localhost iframe
  // 2. Add an exemption for localhost
  // 3. Fire up Websocket
  // Generally local IP addresses are exempt from https-only, but if we do not add
  // an exemption for localhost, then the TriggeringPrincipal of the WebSocket is
  // `not` exempt and we would upgrade ws to wss.

  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.security.https_only_mode", true],
      ["network.proxy.allow_hijacking_localhost", true],
    ],
  });

  await BrowserTestUtils.withNewTab("about:blank", async function (browser) {
    let loaded = BrowserTestUtils.browserLoaded(browser);

    BrowserTestUtils.startLoadingURIString(browser, WEBSOCKET_DOC_URL);
    await loaded;

    await SpecialPowers.spawn(browser, [], async function () {
      // Part 1:
      let myIframe = content.document.createElement("iframe");
      content.document.body.appendChild(myIframe);
      myIframe.src =
        "http://localhost:9898/browser/dom/security/test/https-only/file_websocket_exceptions_iframe.html";

      myIframe.onload = async function () {
        // Part 2:
        await SpecialPowers.pushPermissions([
          {
            type: "https-only-load-insecure",
            allow: true,
            context: "http://localhost:9898",
          },
        ]);
        // Part 3.
        myIframe.contentWindow.postMessage({ myMessage: "runWebSocket" }, "*");
      };

      const promise = new Promise(resolve => {
        content.addEventListener("WebSocketEnded", resolve, {
          once: true,
        });
      });

      const { detail } = await promise;

      is(detail.state, "onopen", "sanity: websocket loaded");
      ok(
        detail.url.startsWith("ws://example.com/tests"),
        "exempt websocket should not be upgraded to wss://"
      );
    });
  });
  await SpecialPowers.popPermissions();
});
