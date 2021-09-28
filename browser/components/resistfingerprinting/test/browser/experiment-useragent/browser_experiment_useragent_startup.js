add_task(async function firefoxVersionExperimentOnStartup() {
  const BASE =
    "http://mochi.test:8888/browser/browser/components/resistfingerprinting/test/browser/";
  const TEST_TARGET_URL = `${BASE}browser_navigator_header.sjs?`;
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_TARGET_URL
  );

  let result = await SpecialPowers.spawn(tab.linkedBrowser, [], function() {
    return content.document.body.textContent;
  });

  ok(
    result.includes("rv:100.0"),
    "User-Agent string does include the rv:100.0 segment"
  );

  ok(
    result.includes("Firefox/100.0"),
    "User-Agent string does include the Firefox/100.0 segment"
  );

  BrowserTestUtils.removeTab(tab);
});
