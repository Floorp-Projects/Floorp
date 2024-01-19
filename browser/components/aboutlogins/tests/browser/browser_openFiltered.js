/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_setup(async function () {
  let storageChangedPromised = TestUtils.topicObserved(
    "passwordmgr-storage-changed",
    (_, data) => data == "addLogin"
  );
  TEST_LOGIN1 = await Services.logins.addLoginAsync(TEST_LOGIN1);
  await storageChangedPromised;
  storageChangedPromised = TestUtils.topicObserved(
    "passwordmgr-storage-changed",
    (_, data) => data == "addLogin"
  );
  TEST_LOGIN2 = await Services.logins.addLoginAsync(TEST_LOGIN2);
  await storageChangedPromised;
  let tabOpenedPromise = BrowserTestUtils.waitForNewTab(
    gBrowser,
    url =>
      url.includes(
        `about:logins?filter=${encodeURIComponent(TEST_LOGIN1.origin)}`
      ),
    true
  );
  LoginHelper.openPasswordManager(window, {
    filterString: TEST_LOGIN1.origin,
    entryPoint: "preferences",
  });
  await tabOpenedPromise;
  registerCleanupFunction(() => {
    BrowserTestUtils.removeTab(gBrowser.selectedTab);
    Services.logins.removeAllUserFacingLogins();
  });
});

add_task(async function test_query_parameter_filter() {
  let browser = gBrowser.selectedBrowser;
  let vanillaLogins = [
    LoginHelper.loginToVanillaObject(TEST_LOGIN1),
    LoginHelper.loginToVanillaObject(TEST_LOGIN2),
  ];
  await SpecialPowers.spawn(browser, [vanillaLogins], async logins => {
    const loginList = Cu.waiveXrays(
      content.document.querySelector("login-list")
    );
    await ContentTaskUtils.waitForCondition(() => {
      return loginList._loginGuidsSortedOrder.length == 2;
    }, "Waiting for logins to be cached");

    await ContentTaskUtils.waitForCondition(() => {
      const selectedLoginItem = Cu.waiveXrays(
        loginList.shadowRoot.querySelector(
          "login-list-item[aria-selected='true']"
        )
      );
      return selectedLoginItem.dataset.guid === logins[0].guid;
    }, "Waiting for TEST_LOGIN1 to be selected for the login-item view");

    const loginItem = Cu.waiveXrays(
      content.document.querySelector("login-item")
    );

    Assert.ok(
      ContentTaskUtils.isVisible(loginItem),
      "login-item should be visible when a login is selected"
    );
    const loginIntro = content.document.querySelector("login-intro");
    Assert.ok(
      ContentTaskUtils.isHidden(loginIntro),
      "login-intro should be hidden when a login is selected"
    );

    const loginFilter = loginList.shadowRoot.querySelector("login-filter");

    const xRayLoginFilter = Cu.waiveXrays(loginFilter);
    Assert.equal(
      xRayLoginFilter.value,
      logins[0].origin,
      "The filter should be prepopulated"
    );
    Assert.equal(
      loginList.shadowRoot.activeElement,
      loginFilter,
      "login-filter should be focused"
    );
    Assert.equal(
      loginFilter.shadowRoot.activeElement,
      loginFilter.shadowRoot.querySelector(".filter"),
      "the actual input inside of login-filter should be focused"
    );

    let hiddenLoginListItems =
      loginList.shadowRoot.querySelectorAll(".list-item[hidden]");

    let visibleLoginListItems = loginList.shadowRoot.querySelectorAll(
      ".list-item:not([hidden])"
    );

    Assert.equal(
      visibleLoginListItems.length,
      1,
      "The one login should be visible"
    );
    Assert.equal(
      visibleLoginListItems[0].dataset.guid,
      logins[0].guid,
      "TEST_LOGIN1 should be visible"
    );
    Assert.equal(
      hiddenLoginListItems.length,
      2,
      "One saved login and one blank login should be hidden"
    );
    Assert.equal(
      hiddenLoginListItems[0].tagName,
      "NEW-LIST-ITEM",
      "new-list-item should be hidden"
    );
    Assert.equal(
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
    url =>
      url.includes(
        `about:logins?filter=${encodeURIComponent(HOSTNAME_WITH_NO_LOGINS)}`
      ),
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
    Assert.equal(
      loginList._loginGuidsSortedOrder.length,
      2,
      "login list should have two logins stored"
    );

    Assert.ok(
      ContentTaskUtils.isHidden(loginList._list),
      "the login list should be hidden when there is a search with no results"
    );
    let intro = loginList.shadowRoot.querySelector(".intro");
    Assert.ok(
      ContentTaskUtils.isHidden(intro),
      "the intro should be hidden when there is a search with no results"
    );
    let emptySearchMessage = loginList.shadowRoot.querySelector(
      ".empty-search-message"
    );
    Assert.ok(
      ContentTaskUtils.isVisible(emptySearchMessage),
      "the empty search message should be visible when there is a search with no results"
    );

    let visibleLoginListItems = loginList.shadowRoot.querySelectorAll(
      "login-list-item:not([hidden])"
    );
    Assert.equal(visibleLoginListItems.length, 0, "No login should be visible");

    Assert.ok(
      !loginList._createLoginButton.disabled,
      "create button should be enabled"
    );

    let loginItem = content.document.querySelector("login-item");
    Assert.ok(!loginItem.dataset.isNewLogin, "should not be in create mode");
    Assert.ok(!loginItem.dataset.editing, "should not be in edit mode");
    Assert.ok(
      ContentTaskUtils.isHidden(loginItem),
      "login-item should be hidden when a login is not selected and we're not in create mode"
    );
    let loginIntro = content.document.querySelector("login-intro");
    Assert.ok(
      ContentTaskUtils.isHidden(loginIntro),
      "login-intro should be hidden when a login is not selected and we're not in create mode"
    );

    loginList._createLoginButton.click();

    Assert.ok(loginItem.dataset.isNewLogin, "should be in create mode");
    Assert.ok(loginItem.dataset.editing, "should be in edit mode");
    Assert.ok(
      ContentTaskUtils.isVisible(loginItem),
      "login-item should be visible in create mode"
    );
    Assert.ok(
      ContentTaskUtils.isHidden(loginIntro),
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
    Assert.equal(
      loginList._loginGuidsSortedOrder.length,
      2,
      "login list should have two logins stored"
    );

    Assert.ok(
      ContentTaskUtils.isHidden(loginList._list),
      "the login list should be hidden when there is a search with no results"
    );

    // Backspace the trailing 'x' to get matching logins
    const EventUtils = ContentTaskUtils.getEventUtils(content);
    EventUtils.sendChar("KEY_Backspace", content);

    let intro = loginList.shadowRoot.querySelector(".intro");
    Assert.ok(
      ContentTaskUtils.isHidden(intro),
      "the intro should be hidden when there is no selection"
    );
    let emptySearchMessage = loginList.shadowRoot.querySelector(
      ".empty-search-message"
    );
    Assert.ok(
      ContentTaskUtils.isHidden(emptySearchMessage),
      "the empty search message should be hidden when there is matching logins"
    );

    let visibleLoginListItems = loginList.shadowRoot.querySelectorAll(
      "login-list-item:not([hidden])"
    );
    Assert.equal(
      visibleLoginListItems.length,
      1,
      "One login should be visible after backspacing"
    );

    Assert.ok(
      !loginList._createLoginButton.disabled,
      "create button should be enabled"
    );

    let loginItem = content.document.querySelector("login-item");
    Assert.ok(!loginItem.dataset.isNewLogin, "should not be in create mode");
    Assert.ok(!loginItem.dataset.editing, "should not be in edit mode");
    Assert.ok(
      ContentTaskUtils.isHidden(loginItem),
      "login-item should be hidden when a login is not selected and we're not in create mode"
    );
    let loginIntro = content.document.querySelector("login-intro");
    Assert.ok(
      ContentTaskUtils.isHidden(loginIntro),
      "login-intro should be hidden when a login is not selected and we're not in create mode"
    );

    loginList._createLoginButton.click();

    Assert.ok(loginItem.dataset.isNewLogin, "should be in create mode");
    Assert.ok(loginItem.dataset.editing, "should be in edit mode");
    Assert.ok(
      ContentTaskUtils.isVisible(loginItem),
      "login-item should be visible in create mode"
    );
    Assert.ok(
      ContentTaskUtils.isHidden(loginIntro),
      "login-intro should be hidden in create mode"
    );
  });
});
