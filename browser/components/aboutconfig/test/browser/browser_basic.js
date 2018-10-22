/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function test_load_title() {
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: "chrome://browser/content/aboutconfig/aboutconfig.html",
  }, browser => {
    info("about:config loaded");
    return ContentTask.spawn(browser, null, async () => {
      await content.document.l10n.ready;
      Assert.equal(content.document.title, "about:config");
    });
  });
});
