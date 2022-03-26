/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

function test_runner(test) {
  let testTask = async () => {
    // Before each
    const sandbox = sinon.createSandbox();
    try {
      await test({
        sandbox,
        pktPanelMessaging: testGlobal.window.pktPanelMessaging,
      });
    } finally {
      // After each
      sandbox.restore();
    }
  };

  // Copy the name of the test function to identify the test
  Object.defineProperty(testTask, "name", { value: test.name });
  add_task(testTask);
}

test_runner(async function test_clickHelper({ sandbox, pktPanelMessaging }) {
  // Create a button to test the click helper with.
  const button = document.createElement("button");
  button.setAttribute("href", "http://example.com");

  // Setup a stub for the click itself.
  sandbox.stub(pktPanelMessaging, "sendMessage");

  // Create the click helper and trigger the click.
  pktPanelMessaging.clickHelper(button, { source: "test-click", position: 2 });
  button.click();

  Assert.ok(
    pktPanelMessaging.sendMessage.calledOnce,
    "Should fire sendMessage once with clickHelper click"
  );
  Assert.ok(
    pktPanelMessaging.sendMessage.calledWith("PKT_openTabWithUrl", {
      url: "http://example.com",
      source: "test-click",
      position: 2,
    }),
    "Should send expected values to sendMessage with clickHelper click"
  );
});
