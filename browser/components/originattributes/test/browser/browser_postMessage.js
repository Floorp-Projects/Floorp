/**
 * Bug 1492607 - Test for assuring that postMessage cannot go across OAs.
 */

const FPD_ONE = "http://example.com";
const FPD_TWO = "http://example.org";

const TEST_BASE = "/browser/browser/components/originattributes/test/browser/";

add_setup(async function () {
  // Make sure first party isolation is enabled.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.firstparty.isolate", true],
      ["dom.security.https_first", false],
    ],
  });
});

async function runTestWithOptions(
  aDifferentFPD,
  aStarTargetOrigin,
  aBlockAcrossFPD
) {
  let testPageURL = aDifferentFPD
    ? FPD_ONE + TEST_BASE + "file_postMessage.html"
    : FPD_TWO + TEST_BASE + "file_postMessage.html";

  // Deciding the targetOrigin according to the test setting.
  let targetOrigin;
  if (aStarTargetOrigin) {
    targetOrigin = "*";
  } else {
    targetOrigin = aDifferentFPD ? FPD_ONE : FPD_TWO;
  }
  let senderURL =
    FPD_TWO + TEST_BASE + `file_postMessageSender.html?${targetOrigin}`;

  // Open a tab to listen messages.
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, testPageURL);

  // Use window.open() in the tab to open the sender tab. The sender tab
  // will send a message through postMessage to window.opener.
  let senderTabPromise = BrowserTestUtils.waitForNewTab(
    gBrowser,
    senderURL,
    true
  );
  SpecialPowers.spawn(tab.linkedBrowser, [senderURL], aSenderPath => {
    content.open(aSenderPath, "_blank");
  });

  // Wait and get the tab of the sender tab.
  let senderTab = await senderTabPromise;

  // The postMessage should be blocked when the first parties are different with
  // the following two cases. First, it is using a non-star target origin.
  // Second, it is using the star target origin and the pref
  // 'privacy.firstparty.isolate.block_post_message' is true.
  let shouldBlock = aDifferentFPD && (!aStarTargetOrigin || aBlockAcrossFPD);

  await SpecialPowers.spawn(tab.linkedBrowser, [shouldBlock], async aValue => {
    await new Promise(resolve => {
      content.addEventListener("message", async function eventHandler(aEvent) {
        if (aEvent.data === "Self") {
          let display = content.document.getElementById("display");
          if (aValue) {
            Assert.equal(
              display.innerHTML,
              "",
              "It should not get a message from other OA."
            );
          } else {
            await ContentTaskUtils.waitForCondition(
              () => display.innerHTML == "Message",
              "Wait for message to arrive"
            );
            Assert.equal(
              display.innerHTML,
              "Message",
              "It should get a message from the same OA."
            );
          }

          content.removeEventListener("message", eventHandler);
          resolve();
        }
      });

      // Trigger the content to send a postMessage to itself.
      content.document.getElementById("button").click();
    });
  });

  BrowserTestUtils.removeTab(tab);
  BrowserTestUtils.removeTab(senderTab);
}

add_task(async function runTests() {
  for (let useDifferentFPD of [true, false]) {
    for (let useStarTargetOrigin of [true, false]) {
      for (let enableBlocking of [true, false]) {
        if (enableBlocking) {
          await SpecialPowers.pushPrefEnv({
            set: [["privacy.firstparty.isolate.block_post_message", true]],
          });
        }

        await runTestWithOptions(
          useDifferentFPD,
          useStarTargetOrigin,
          enableBlocking
        );

        if (enableBlocking) {
          await SpecialPowers.popPrefEnv();
        }
      }
    }
  }
});
