/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function setup() {
  await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: "about:logins",
  });
  registerCleanupFunction(() => {
    BrowserTestUtils.removeTab(gBrowser.selectedTab);
  });
});

add_task(async function test_no_logins_class() {
  await ContentTask.spawn(gBrowser.selectedBrowser, null, async () => {
    function isElementHidden(element) {
      return content.getComputedStyle(element).display === "none";
    }

    let loginList = content.document.querySelector("login-list");

    ok(
      content.document.documentElement.classList.contains("no-logins"),
      "root should be in no logins view"
    );
    ok(
      loginList.classList.contains("no-logins"),
      "login-list should be in no logins view"
    );

    let loginIntro = content.document.querySelector("login-intro");
    let loginItem = content.document.querySelector("login-item");
    let loginListIntro = loginList.shadowRoot.querySelector(".intro");
    let loginListList = loginList.shadowRoot.querySelector("ol");

    ok(
      !isElementHidden(loginIntro),
      "login-intro should be shown in no logins view"
    );
    ok(
      !isElementHidden(loginListIntro),
      "login-list intro should be shown in no logins view"
    );

    ok(
      isElementHidden(loginItem),
      "login-item should be hidden in no logins view"
    );
    ok(
      isElementHidden(loginListList),
      "login-list logins list should be hidden in no logins view"
    );
  });
});
