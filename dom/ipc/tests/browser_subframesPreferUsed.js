/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

ok(
  Services.appinfo.fissionAutostart,
  "this test requires fission to function!"
);

function documentURL(origin, html) {
  let params = new URLSearchParams();
  params.append("html", html.trim());
  return `${origin}/document-builder.sjs?${params.toString()}`;
}

async function singleTest(preferUsed) {
  info(`running test with preferUsed=${preferUsed}`);
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.ipc.processCount.webIsolated", 4],
      ["browser.tabs.remote.subframesPreferUsed", preferUsed],
    ],
  });

  const TEST_URL = documentURL(
    "https://example.com",
    `<iframe src=${JSON.stringify(
      documentURL("https://example.org", `<h1>iframe</h1>`)
    )}></iframe>`
  );

  await BrowserTestUtils.withNewTab(TEST_URL, async browser1 => {
    is(browser1.browsingContext.children.length, 1);
    let topProc1 = browser1.browsingContext.currentWindowGlobal.domProcess;
    let frameProc1 =
      browser1.browsingContext.children[0].currentWindowGlobal.domProcess;
    isnot(
      topProc1.childID,
      frameProc1.childID,
      "the frame should be in a separate process"
    );

    await BrowserTestUtils.withNewTab(TEST_URL, async browser2 => {
      is(browser2.browsingContext.children.length, 1);
      let topProc2 = browser2.browsingContext.currentWindowGlobal.domProcess;
      let frameProc2 =
        browser2.browsingContext.children[0].currentWindowGlobal.domProcess;
      isnot(
        topProc2.childID,
        frameProc2.childID,
        "the frame should be in a separate process"
      );

      // Compare processes used for the two tabs.
      isnot(
        topProc1.childID,
        topProc2.childID,
        "the toplevel windows should be loaded in separate processes"
      );
      if (preferUsed) {
        is(
          frameProc1.childID,
          frameProc2.childID,
          "the iframes should load in the same process with subframesPreferUsed"
        );
      } else {
        isnot(
          frameProc1.childID,
          frameProc2.childID,
          "the iframes should load in different processes without subframesPreferUsed"
        );
      }
    });
  });
}

add_task(async function test_preferUsed() {
  await singleTest(true);
});

add_task(async function test_noPreferUsed() {
  await singleTest(false);
});
