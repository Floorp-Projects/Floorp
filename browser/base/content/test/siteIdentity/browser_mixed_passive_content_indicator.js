const TEST_URL = getRootDirectory(gTestPath).replace("chrome://mochitests/content", "https://example.com") + "simple_mixed_passive.html";

add_task(async function test_mixed_passive_content_indicator() {
  await BrowserTestUtils.withNewTab(TEST_URL, function() {
    is(document.getElementById("identity-box").className,
       "unknownIdentity mixedDisplayContent",
       "identity box has class name for mixed content");
  });
});
