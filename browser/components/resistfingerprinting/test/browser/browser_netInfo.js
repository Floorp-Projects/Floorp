/**
 * Bug 1372072 - A test case for check whether network information API has been
 *   spoofed correctly when 'privacy.resistFingerprinting' is true;
 */

async function testWindow() {
  // Open a tab to test network information in a content.
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_PATH + "file_dummy.html"
  );

  await SpecialPowers.spawn(tab.linkedBrowser, [], async function() {
    ok("connection" in content.navigator, "navigator.connection should exist");

    is(
      content.navigator.connection.type,
      "unknown",
      "The connection type is spoofed correctly"
    );
  });

  BrowserTestUtils.removeTab(tab);
}

async function testWorker() {
  // Open a tab to test network information in a worker.
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_PATH + "file_dummy.html"
  );

  await SpecialPowers.spawn(tab.linkedBrowser, [], async function() {
    await new Promise(resolve => {
      let worker = new content.Worker("file_workerNetInfo.js");

      worker.onmessage = function(e) {
        if (e.data.type == "status") {
          ok(e.data.status, e.data.msg);
        } else if (e.data.type == "finish") {
          resolve();
        } else {
          ok(false, "Unknown message type");
          resolve();
        }
      };
      worker.postMessage({ type: "runTests" });
    });
  });

  BrowserTestUtils.removeTab(tab);
}

add_task(async function runTest() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.resistFingerprinting", true],
      ["dom.netinfo.enabled", true],
    ],
  });

  await testWindow();
  await testWorker();
});
