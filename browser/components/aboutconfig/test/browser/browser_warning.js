/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const PAGE_URL = "chrome://browser/content/aboutconfig/aboutconfig.html";

add_task(async function test_load_warningpage() {
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: PAGE_URL,
  }, browser => {
    info("about:config loaded");
    return ContentTask.spawn(browser, null, async () => {
      // Test that the warning page is presented:
      Assert.equal(content.document.getElementsByTagName("button").length, 1);
      Assert.equal(content.document.getElementById("search"), undefined);
      Assert.equal(content.document.getElementById("prefs"), undefined);
    });
  });
});
