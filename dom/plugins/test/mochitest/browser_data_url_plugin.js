var rootDir = getRootDirectory(gTestPath);
const gTestRoot = rootDir.replace(
  "chrome://mochitests/content/",
  "http://127.0.0.1:8888/"
);

add_task(async function() {
  await SpecialPowers.pushPrefEnv({
    set: [["security.data_uri.unique_opaque_origin", true]],
  });
  is(
    navigator.plugins.length,
    0,
    "plugins should not be available to chrome-privilege pages"
  );

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: gTestRoot + "plugin_data_url_test.html" },
    async function(browser) {
      await ContentTask.spawn(browser, null, async function() {
        ok(
          content.window.navigator.plugins.length > 0,
          "plugins should be available to HTTP-loaded pages"
        );
        let dataFrameWin = content.document.getElementById("dataFrame")
          .contentWindow;
        is(
          dataFrameWin.navigator.plugins.length,
          0,
          "plugins should not be available to data: URI in iframe on a site"
        );
      });
    }
  );
});
