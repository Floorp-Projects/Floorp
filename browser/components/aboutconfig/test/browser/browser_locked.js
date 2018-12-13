/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const PAGE_URL = "chrome://browser/content/aboutconfig/aboutconfig.html";

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["test.aboutconfig.a", "some value"],
    ],
  });
});

add_task(async function test_locked() {
  registerCleanupFunction(() => {
    Services.prefs.unlockPref("browser.search.searchEnginesURL");
    Services.prefs.unlockPref("test.aboutconfig.a");
    Services.prefs.unlockPref("accessibility.AOM.enabled");
  });

  Services.prefs.lockPref("browser.search.searchEnginesURL");
  Services.prefs.lockPref("test.aboutconfig.a");
  Services.prefs.lockPref("accessibility.AOM.enabled");
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: PAGE_URL,
  }, async browser => {
    await content.document.querySelector("button").click();

    await ContentTask.spawn(browser, null, () => {
      let list = [...content.document.getElementById("prefs")
        .getElementsByTagName("tr")];
      function getRow(name) {
        return list.find(row => row.querySelector("td").textContent == name);
      }
      function getValue(name) {
          return getRow(name).querySelector("td.cell-value").textContent;
      }
      function getButton(name) {
        return getRow(name).querySelector("button");
      }

      // Test locked default string pref.
      let lockedPref = getRow("browser.search.searchEnginesURL");
      Assert.ok(lockedPref.classList.contains("locked"));
      Assert.equal(getValue("browser.search.searchEnginesURL"), "https://addons.mozilla.org/%LOCALE%/firefox/search-engines/");
      Assert.ok(getButton("browser.search.searchEnginesURL").classList.contains("button-edit"));
      Assert.equal(getButton("browser.search.searchEnginesURL").disabled, true);

      // Test locked default boolean pref.
      lockedPref = getRow("accessibility.AOM.enabled");
      Assert.ok(lockedPref.classList.contains("locked"));
      Assert.equal(getValue("accessibility.AOM.enabled"), "false");
      Assert.ok(getButton("accessibility.AOM.enabled").classList.contains("button-toggle"));
      Assert.equal(getButton("accessibility.AOM.enabled").disabled, true);

      // Test locked user added pref.
      lockedPref = getRow("test.aboutconfig.a");
      Assert.ok(lockedPref.classList.contains("locked"));
      Assert.equal(getValue("test.aboutconfig.a"), "");
      Assert.ok(getButton("test.aboutconfig.a").classList.contains("button-edit"));
      Assert.equal(getButton("test.aboutconfig.a").disabled, true);

      // Test pref not locked
      let unlockedPref = getRow("accessibility.indicator.enabled");
      Assert.equal(unlockedPref.classList.contains("locked"), false);
      Assert.equal(getValue("accessibility.indicator.enabled"), "false");
      Assert.ok(getButton("accessibility.indicator.enabled").classList.contains("button-toggle"));
      Assert.equal(getButton("accessibility.indicator.enabled").disabled, false);
    });
  });
});
