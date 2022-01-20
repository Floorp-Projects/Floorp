const TEST_URI1 =
  "http://mochi.test:8888/browser/dom/tests/browser/" +
  "create_webrtc_peer_connection.html";

const TEST_URI2 =
  "https://example.com/browser/dom/tests/browser/" +
  "create_webrtc_peer_connection.html";

add_task(async () => {
  await BrowserTestUtils.withNewTab(TEST_URI1, async browser => {
    const windowGlobal = browser.browsingContext.currentWindowGlobal;
    Assert.ok(windowGlobal);

    Assert.strictEqual(
      windowGlobal.hasActivePeerConnections(),
      false,
      "No active connections at the beginning"
    );

    await SpecialPowers.spawn(browser, [], async () => {
      content.postMessage("push-peer-connection", "*");
      return new Promise(resolve =>
        content.addEventListener("message", function onMessage(event) {
          if (event.data == "ack") {
            content.removeEventListener(event.type, onMessage);
            resolve();
          }
        })
      );
    });

    Assert.strictEqual(
      windowGlobal.hasActivePeerConnections(),
      true,
      "One connection in the top window"
    );

    await SpecialPowers.spawn(browser, [], async () => {
      content.postMessage("pop-peer-connection", "*");
      return new Promise(resolve =>
        content.addEventListener("message", function onMessage(event) {
          if (event.data == "ack") {
            content.removeEventListener(event.type, onMessage);
            resolve();
          }
        })
      );
    });

    Assert.strictEqual(
      windowGlobal.hasActivePeerConnections(),
      false,
      "All connections have been closed"
    );

    await SpecialPowers.spawn(
      browser,
      [TEST_URI1, TEST_URI2],
      async (TEST_URI1, TEST_URI2) => {
        // Create a promise that is fulfilled when the "ack" message is received
        // |targetCount| times.
        const createWaitForAckPromise = (eventTarget, targetCount) => {
          let counter = 0;
          return new Promise(resolve => {
            eventTarget.addEventListener("message", function onMsg(event) {
              if (event.data == "ack") {
                ++counter;
                if (counter == targetCount) {
                  eventTarget.removeEventListener(event.type, onMsg);
                  resolve();
                }
              }
            });
          });
        };

        const addFrame = (id, url) => {
          const iframe = content.document.createElement("iframe");
          iframe.id = id;
          iframe.src = url;
          content.document.body.appendChild(iframe);
          return iframe;
        };

        // Create two iframes hosting a same-origin page and a cross-origin page
        const iframe1 = addFrame("iframe-same-origin", TEST_URI1);
        const iframe2 = addFrame("iframe-cross-origin", TEST_URI2);
        await ContentTaskUtils.waitForEvent(iframe1, "load");
        await ContentTaskUtils.waitForEvent(iframe2, "load");

        // Make sure the counter is not messed up after successive push/pop
        // messages
        const kLoopCount = 100;
        for (let i = 0; i < kLoopCount; ++i) {
          content.postMessage("push-peer-connection", "*");
          iframe1.contentWindow.postMessage("push-peer-connection", "*");
          iframe2.contentWindow.postMessage("push-peer-connection", "*");
          iframe1.contentWindow.postMessage("pop-peer-connection", "*");
          iframe2.contentWindow.postMessage("pop-peer-connection", "*");
          content.postMessage("pop-peer-connection", "*");
        }
        iframe2.contentWindow.postMessage("push-peer-connection", "*");

        return createWaitForAckPromise(content, kLoopCount * 6 + 1);
      }
    );

    Assert.strictEqual(
      windowGlobal.hasActivePeerConnections(),
      true,
      "#iframe-cross-origin still has an active connection"
    );

    await SpecialPowers.spawn(browser, [], async () => {
      content.document
        .getElementById("iframe-cross-origin")
        .contentWindow.postMessage("pop-peer-connection", "*");
      return new Promise(resolve =>
        content.addEventListener("message", function onMessage(event) {
          if (event.data == "ack") {
            content.removeEventListener(event.type, onMessage);
            resolve();
          }
        })
      );
    });

    Assert.strictEqual(
      windowGlobal.hasActivePeerConnections(),
      false,
      "All connections have been closed"
    );
  });
});
