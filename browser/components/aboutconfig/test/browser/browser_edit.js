/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const PAGE_URL = "chrome://browser/content/aboutconfig/aboutconfig.html";

add_task(async function test_delete_user_pref() {
  Services.prefs.setBoolPref("userAddedPref", true);
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: PAGE_URL,
  }, browser => {
    return ContentTask.spawn(browser, null, () => {
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
    });
  });
});
