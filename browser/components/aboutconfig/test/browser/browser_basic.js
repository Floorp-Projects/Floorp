/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const PAGE_URL = "chrome://browser/content/aboutconfig/aboutconfig.html";

add_task(async function test_load_title() {
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: PAGE_URL,
  }, browser => {
    info("about:config loaded");
    return ContentTask.spawn(browser, null, async () => {
      await content.document.l10n.ready;
      Assert.equal(content.document.title, "about:config");
    });
  });
});

add_task(async function test_load_settings() {
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: PAGE_URL,
  }, browser => {
    return ContentTask.spawn(browser, null, () => {
      let list = [...content.document.getElementById("list")
                                     .getElementsByTagName("li")];
      function findPref(name) {
        return list.some(e => e.textContent.trim().startsWith(name + " "));
      }
      Assert.ok(findPref("plugins.testmode"));
      Assert.ok(findPref("dom.vr.enabled"));
      Assert.ok(findPref("accessibility.AOM.enabled"));
    });
  });
});
