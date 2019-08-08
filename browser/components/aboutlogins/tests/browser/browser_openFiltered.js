/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [["signon.management.overrideURI", "about:logins?filter=%DOMAIN%"]],
  });

  let storageChangedPromised = TestUtils.topicObserved(
    "passwordmgr-storage-changed",
    (_, data) => data == "addLogin"
  );
  TEST_LOGIN1 = Services.logins.addLogin(TEST_LOGIN1);
  await storageChangedPromised;
  storageChangedPromised = TestUtils.topicObserved(
    "passwordmgr-storage-changed",
    (_, data) => data == "addLogin"
  );
  TEST_LOGIN2 = Services.logins.addLogin(TEST_LOGIN2);
  await storageChangedPromised;
  let tabOpenedPromise = BrowserTestUtils.waitForNewTab(
    gBrowser,
    "about:logins?filter=" + encodeURIComponent(TEST_LOGIN1.origin),
    true
  );
  LoginHelper.openPasswordManager(window, {
    filterString: TEST_LOGIN1.origin,
    entryPoint: "preferences",
  });
  await tabOpenedPromise;
  registerCleanupFunction(() => {
    BrowserTestUtils.removeTab(gBrowser.selectedTab);
    Services.logins.removeAllLogins();
  });
});

add_task(async function test_query_parameter_filter() {
  let browser = gBrowser.selectedBrowser;
  let vanillaLogins = [
    LoginHelper.loginToVanillaObject(TEST_LOGIN1),
    LoginHelper.loginToVanillaObject(TEST_LOGIN2),
  ];
  await ContentTask.spawn(browser, vanillaLogins, async logins => {
    let loginList = Cu.waiveXrays(content.document.querySelector("login-list"));
    await ContentTaskUtils.waitForCondition(() => {
      return loginList._loginGuidsSortedOrder.length == 2;
    }, "Waiting for logins to be cached");

    let loginItem = Cu.waiveXrays(content.document.querySelector("login-item"));
    await ContentTaskUtils.waitForCondition(
      () => loginItem._login.guid == logins[0].guid,
      "Waiting for TEST_LOGIN1 to be selected for the login-item view"
    );

    let loginFilter = content.document.querySelector("login-filter");
    let xRayLoginFilter = Cu.waiveXrays(loginFilter);
    is(
      xRayLoginFilter.value,
      logins[0].origin,
      "The filter should be prepopulated"
    );
    is(
      content.document.activeElement,
      loginFilter,
      "login-filter should be focused"
    );
    is(
      loginFilter.shadowRoot.activeElement,
      loginFilter.shadowRoot.querySelector(".filter"),
      "the actual input inside of login-filter should be focused"
    );

    let hiddenLoginListItems = loginList.shadowRoot.querySelectorAll(
      ".login-list-item[hidden]"
    );
    let visibleLoginListItems = loginList.shadowRoot.querySelectorAll(
      ".login-list-item:not(#new-login-list-item):not([hidden])"
    );
    is(visibleLoginListItems.length, 1, "The one login should be visible");
    is(
      visibleLoginListItems[0].dataset.guid,
      logins[0].guid,
      "TEST_LOGIN1 should be visible"
    );
    is(hiddenLoginListItems.length, 1, "One login should be hidden");
    is(
      hiddenLoginListItems[0].dataset.guid,
      logins[1].guid,
      "TEST_LOGIN2 should be hidden"
    );
  });
});
