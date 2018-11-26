/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const PAGE_URL = "chrome://browser/content/aboutconfig/aboutconfig.html";

add_task(async function test_delete_user_pref() {
  Services.prefs.setBoolPref("userAddedPref", true);
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: PAGE_URL,
  }, async browser => {
    await ContentTask.spawn(browser, null, () => {
      let list = [...content.document.getElementById("prefs")
        .getElementsByTagName("tr")];
      function getRow(name) {
        return list.find(row => row.querySelector("td").textContent == name);
      }
      Assert.ok(getRow("userAddedPref"));
      getRow("userAddedPref").lastChild.lastChild.click();
      list = [...content.document.getElementById("prefs")
        .getElementsByTagName("tr")];
      Assert.ok(!getRow("userAddedPref"));
      Assert.ok(!Services.prefs.getChildList("").includes("userAddedPref"));

      // Search for nothing to test gPrefArray
      let search = content.document.getElementById("search");
      search.focus();
    });

    EventUtils.sendKey("return");
    await ContentTask.spawn(browser, null, () => {
      let list = [...content.document.getElementById("prefs")
        .getElementsByTagName("tr")];
      function getRow(name) {
        return list.find(row => row.querySelector("td").textContent == name);
      }
      Assert.ok(!getRow("userAddedPref"));
    });
  });
});

add_task(async function test_reset_user_pref() {
  await SpecialPowers.pushPrefEnv({"set": [["browser.autofocus", false]]});

  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: PAGE_URL,
  }, async browser => {
    await ContentTask.spawn(browser, null, () => {
      ChromeUtils.import("resource://gre/modules/Preferences.jsm");

      function getRow(name) {
        return list.find(row => row.querySelector("td").textContent == name);
      }
      function getValue(name) {
        return getRow(name).querySelector("td.cell-value").textContent;
      }
      let testPref = "browser.autofocus";
      // Click reset.
      let list = [...content.document.getElementById("prefs")
        .getElementsByTagName("tr")];
      let row = getRow(testPref);
      row.lastChild.lastChild.click();
      // Check new layout and reset.
      list = [...content.document.getElementById("prefs")
        .getElementsByTagName("tr")];
      Assert.ok(!row.classList.contains("has-user-value"));
      Assert.equal(row.childNodes[2].childNodes.length, 0);
      Assert.ok(!Services.prefs.prefHasUserValue(testPref));
      Assert.equal(getValue(testPref), "" + Preferences.get(testPref));

      // Search for nothing to test gPrefArray
      let search = content.document.getElementById("search");
      search.focus();
    });

    EventUtils.sendKey("return");
    await ContentTask.spawn(browser, null, () => {
      function getRow(name) {
        return list.find(row => row.querySelector("td").textContent == name);
      }
      function getValue(name) {
        return getRow(name).querySelector("td.cell-value").textContent;
      }
      let testPref = "browser.autofocus";
      // Check new layout and reset.
      let list = [...content.document.getElementById("prefs")
        .getElementsByTagName("tr")];
      let row = getRow(testPref);
      Assert.ok(!row.classList.contains("has-user-value"));
      Assert.equal(row.childNodes[2].childNodes.length, 0);
      Assert.equal(getValue(testPref), "" + Preferences.get(testPref));
    });
  });
});
