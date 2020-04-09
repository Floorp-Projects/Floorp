/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function setup() {
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
  await SpecialPowers.spawn(browser, [vanillaLogins], async logins => {
    let loginList = Cu.waiveXrays(content.document.querySelector("login-list"));
    await ContentTaskUtils.waitForCondition(() => {
      return loginList._loginGuidsSortedOrder.length == 2;
    }, "Waiting for logins to be cached");

    let loginItem = Cu.waiveXrays(content.document.querySelector("login-item"));
    await ContentTaskUtils.waitForCondition(
      () => loginItem._login.guid == logins[0].guid,
      "Waiting for TEST_LOGIN1 to be selected for the login-item view"
    );

    ok(
      ContentTaskUtils.is_visible(loginItem),
      "login-item should be visible when a login is selected"
    );
    let loginIntro = content.document.querySelector("login-intro");
    ok(
      ContentTaskUtils.is_hidden(loginIntro),
      "login-intro should be hidden when a login is selected"
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
      ".login-list-item:not([hidden])"
    );
    is(visibleLoginListItems.length, 1, "The one login should be visible");
    is(
      visibleLoginListItems[0].dataset.guid,
      logins[0].guid,
      "TEST_LOGIN1 should be visible"
    );
    is(
      hiddenLoginListItems.length,
      2,
      "One saved login and one blank login should be hidden"
    );
    is(
      hiddenLoginListItems[0].id,
      "new-login-list-item",
      "#new-login-list-item should be hidden"
    );
    is(
      hiddenLoginListItems[1].dataset.guid,
      logins[1].guid,
      "TEST_LOGIN2 should be hidden"
    );
  });
});

add_task(async function test_query_parameter_filter_no_logins_for_site() {
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
  const HOSTNAME_WITH_NO_LOGINS = "xxx-no-logins-for-site-xxx";
  let tabOpenedPromise = BrowserTestUtils.waitForNewTab(
    gBrowser,
    "about:logins?filter=" + encodeURIComponent(HOSTNAME_WITH_NO_LOGINS),
    true
  );
  LoginHelper.openPasswordManager(window, {
    filterString: HOSTNAME_WITH_NO_LOGINS,
    entryPoint: "preferences",
  });
  await tabOpenedPromise;

  let browser = gBrowser.selectedBrowser;
  await SpecialPowers.spawn(browser, [], async () => {
    let loginList = Cu.waiveXrays(content.document.querySelector("login-list"));
    await ContentTaskUtils.waitForCondition(() => {
      return loginList._loginGuidsSortedOrder.length == 2;
    }, "Waiting for logins to be cached");
    is(
      loginList._loginGuidsSortedOrder.length,
      2,
      "login list should have two logins stored"
    );

    ok(
      ContentTaskUtils.is_hidden(loginList._list),
      "the login list should be hidden when there is a search with no results"
    );
    let intro = loginList.shadowRoot.querySelector(".intro");
    ok(
      ContentTaskUtils.is_hidden(intro),
      "the intro should be hidden when there is a search with no results"
    );
    let emptySearchMessage = loginList.shadowRoot.querySelector(
      ".empty-search-message"
    );
    ok(
      ContentTaskUtils.is_visible(emptySearchMessage),
      "the empty search message should be visible when there is a search with no results"
    );

    let visibleLoginListItems = loginList.shadowRoot.querySelectorAll(
      ".login-list-item:not([hidden])"
    );
    is(visibleLoginListItems.length, 0, "No login should be visible");

    ok(
      !loginList._createLoginButton.disabled,
      "create button should be enabled"
    );

    let loginItem = content.document.querySelector("login-item");
    ok(!loginItem.dataset.isNewLogin, "should not be in create mode");
    ok(!loginItem.dataset.editing, "should not be in edit mode");
    ok(
      ContentTaskUtils.is_hidden(loginItem),
      "login-item should be hidden when a login is not selected and we're not in create mode"
    );
    let loginIntro = content.document.querySelector("login-intro");
    ok(
      ContentTaskUtils.is_hidden(loginIntro),
      "login-intro should be hidden when a login is not selected and we're not in create mode"
    );

    loginList._createLoginButton.click();

    ok(loginItem.dataset.isNewLogin, "should be in create mode");
    ok(loginItem.dataset.editing, "should be in edit mode");
    ok(
      ContentTaskUtils.is_visible(loginItem),
      "login-item should be visible in create mode"
    );
    ok(
      ContentTaskUtils.is_hidden(loginIntro),
      "login-intro should be hidden in create mode"
    );
  });
});

add_task(async function test_query_parameter_filter_no_login_until_backspace() {
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
  let tabOpenedPromise = BrowserTestUtils.waitForNewTab(
    gBrowser,
    "about:logins?filter=" + encodeURIComponent(TEST_LOGIN1.origin) + "x",
    true
  );
  LoginHelper.openPasswordManager(window, {
    filterString: TEST_LOGIN1.origin + "x",
    entryPoint: "preferences",
  });
  await tabOpenedPromise;

  let browser = gBrowser.selectedBrowser;
  await SpecialPowers.spawn(browser, [], async () => {
    let loginList = Cu.waiveXrays(content.document.querySelector("login-list"));
    await ContentTaskUtils.waitForCondition(() => {
      return loginList._loginGuidsSortedOrder.length == 2;
    }, "Waiting for logins to be cached");
    is(
      loginList._loginGuidsSortedOrder.length,
      2,
      "login list should have two logins stored"
    );

    ok(
      ContentTaskUtils.is_hidden(loginList._list),
      "the login list should be hidden when there is a search with no results"
    );

    // Backspace the trailing 'x' to get matching logins
    const EventUtils = ContentTaskUtils.getEventUtils(content);
    EventUtils.sendChar("KEY_Backspace", content);

    let intro = loginList.shadowRoot.querySelector(".intro");
    ok(
      ContentTaskUtils.is_hidden(intro),
      "the intro should be hidden when there is no selection"
    );
    let emptySearchMessage = loginList.shadowRoot.querySelector(
      ".empty-search-message"
    );
    ok(
      ContentTaskUtils.is_hidden(emptySearchMessage),
      "the empty search message should be hidden when there is matching logins"
    );

    let visibleLoginListItems = loginList.shadowRoot.querySelectorAll(
      ".login-list-item:not([hidden])"
    );
    is(
      visibleLoginListItems.length,
      1,
      "One login should be visible after backspacing"
    );

    ok(
      !loginList._createLoginButton.disabled,
      "create button should be enabled"
    );

    let loginItem = content.document.querySelector("login-item");
    ok(!loginItem.dataset.isNewLogin, "should not be in create mode");
    ok(!loginItem.dataset.editing, "should not be in edit mode");
    ok(
      ContentTaskUtils.is_hidden(loginItem),
      "login-item should be hidden when a login is not selected and we're not in create mode"
    );
    let loginIntro = content.document.querySelector("login-intro");
    ok(
      ContentTaskUtils.is_hidden(loginIntro),
      "login-intro should be hidden when a login is not selected and we're not in create mode"
    );

    loginList._createLoginButton.click();

    ok(loginItem.dataset.isNewLogin, "should be in create mode");
    ok(loginItem.dataset.editing, "should be in edit mode");
    ok(
      ContentTaskUtils.is_visible(loginItem),
      "login-item should be visible in create mode"
    );
    ok(
      ContentTaskUtils.is_hidden(loginIntro),
      "login-intro should be hidden in create mode"
    );
  });
});
