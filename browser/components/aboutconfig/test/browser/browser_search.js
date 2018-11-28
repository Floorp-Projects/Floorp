/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

ChromeUtils.import("resource://gre/modules/Preferences.jsm", this);

const PAGE_URL = "chrome://browser/content/aboutconfig/aboutconfig.html";

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["test.aboutconfig.a", "test value 1"],
      ["test.aboutconfig.ab", "test value 2"],
      ["test.aboutconfig.b", "test value 3"],
    ],
  });
});

add_task(async function test_search() {
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: PAGE_URL,
  }, async browser => {
    let prefArray = Services.prefs.getChildList("");

    // Test page loaded with correct number of prefs.
    await ContentTask.spawn(browser, prefArray, aPrefArray => {
      Assert.equal(content.document.getElementById("prefs").childElementCount,
                   aPrefArray.length);

      // Test page search of "button" returns correct number of preferences.
      let search = content.document.getElementById("search");
      search.value = "button   ";
      search.focus();
    });

    EventUtils.sendKey("return");
    await ContentTask.spawn(browser, prefArray, aPrefArray => {
      let filteredPrefArray =
          aPrefArray.filter(pref => pref.includes("button"));
      Assert.equal(content.document.getElementById("prefs").childElementCount,
                   filteredPrefArray.length);

      // Test empty search returns all preferences.
      let search = content.document.getElementById("search");
      search.value = "";
      search.focus();
    });

    EventUtils.sendKey("return");
    await ContentTask.spawn(browser, prefArray, aPrefArray => {
      Assert.equal(content.document.getElementById("prefs").childElementCount,
                   aPrefArray.length);

      // Test invalid search returns no preferences.
      let search = content.document.getElementById("search");
      search.value = "aJunkValueasdf";
      search.focus();
    });

    EventUtils.sendKey("return");
    await ContentTask.spawn(browser, prefArray, aPrefArray => {
      Assert.equal(content.document.getElementById("prefs").childElementCount,
                   0);

      // Test added preferences search returns 2 preferences.
      let search = content.document.getElementById("search");
      search.value = "test.aboutconfig.a";
      search.focus();
    });

    EventUtils.sendKey("return");
    await ContentTask.spawn(browser, prefArray, aPrefArray => {
      Assert.equal(content.document.getElementById("prefs").childElementCount,
                   2);
    });
  });
});
