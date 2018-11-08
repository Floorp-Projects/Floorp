/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const PAGE_URL = "chrome://browser/content/aboutconfig/aboutconfig.html";

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["random.user.pref", "chrome://test/locale/testing.properties"],
    ],
  });
});

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

      // Test if page contains elements.
      Assert.ok(findPref("plugins.testmode"));
      Assert.ok(findPref("dom.vr.enabled"));
      Assert.ok(findPref("accessibility.AOM.enabled"));

      function containsLocalizedValue(value) {
        return list.some(e => e.textContent.trim() == value);
      }
      // Test to see if values are localized.
      Assert.ok(containsLocalizedValue("font.language.group || Default || String || x-western"));
      Assert.ok(containsLocalizedValue("intl.ellipsis || Default || String || \u2026"));
      Assert.ok(containsLocalizedValue("gecko.handlerService.schemes.mailto.1.uriTemplate || Default || String || https://mail.google.com/mail/?extsrc=mailto&url=%s"));

      // Test to see if user created value is not empty string when it matches /^chrome:\/\/.+\/locale\/.+\.properties/.
      Assert.ok(containsLocalizedValue("random.user.pref || Modified || String || chrome://test/locale/testing.properties"));
      // Test to see if empty string when value matches /^chrome:\/\/.+\/locale\/.+\.properties/ and an exception is thrown.
      Assert.ok(containsLocalizedValue("gecko.handlerService.schemes.irc.1.name || Default || String ||"));
    });
  });
});
