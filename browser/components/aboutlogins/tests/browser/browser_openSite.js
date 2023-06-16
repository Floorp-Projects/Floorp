/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_setup(async function () {
  TEST_LOGIN1 = await addLogin(TEST_LOGIN1);
  await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: "about:logins",
  });
  registerCleanupFunction(() => {
    BrowserTestUtils.removeTab(gBrowser.selectedTab);
    Services.logins.removeAllUserFacingLogins();
  });
});

add_task(async function test_launch_login_item() {
  let promiseNewTab = BrowserTestUtils.waitForNewTab(
    gBrowser,
    TEST_LOGIN1.origin + "/"
  );

  let browser = gBrowser.selectedBrowser;

  await SpecialPowers.spawn(browser, [], async () => {
    let loginItem = Cu.waiveXrays(content.document.querySelector("login-item"));
    let originInput = loginItem.shadowRoot.querySelector("a[name='origin']");
    let EventUtils = ContentTaskUtils.getEventUtils(content);
    // Use synthesizeMouseAtCenter to generate an event that more closely resembles the
    // properties of the event object that will be seen when the user clicks the element
    // (.click() sets originalTarget while synthesizeMouse has originalTarget as a Restricted object).
    await EventUtils.synthesizeMouseAtCenter(originInput, {}, content);
  });

  info("waiting for new tab to get opened");
  let newTab = await promiseNewTab;
  Assert.ok(true, "New tab opened to " + TEST_LOGIN1.origin);
  BrowserTestUtils.removeTab(newTab);

  if (!OSKeyStoreTestUtils.canTestOSKeyStoreLogin()) {
    return;
  }

  promiseNewTab = BrowserTestUtils.waitForNewTab(
    gBrowser,
    TEST_LOGIN1.origin + "/"
  );
  let reauthObserved = OSKeyStoreTestUtils.waitForOSKeyStoreLogin(true);
  await SpecialPowers.spawn(browser, [], async () => {
    let loginItem = Cu.waiveXrays(content.document.querySelector("login-item"));
    loginItem._editButton.click();
  });
  await reauthObserved;
  await SpecialPowers.spawn(browser, [], async () => {
    let loginItem = Cu.waiveXrays(content.document.querySelector("login-item"));
    loginItem._usernameInput.value += "-changed";

    Assert.ok(
      content.document.querySelector("confirmation-dialog").hidden,
      "discard-changes confirmation-dialog should be hidden before opening the site"
    );

    let originInput = loginItem.shadowRoot.querySelector("a[name='origin']");
    let EventUtils = ContentTaskUtils.getEventUtils(content);
    // Use synthesizeMouseAtCenter to generate an event that more closely resembles the
    // properties of the event object that will be seen when the user clicks the element
    // (.click() sets originalTarget while synthesizeMouse has originalTarget as a Restricted object).
    await EventUtils.synthesizeMouseAtCenter(originInput, {}, content);
  });

  info("waiting for new tab to get opened");
  newTab = await promiseNewTab;
  Assert.ok(true, "New tab opened to " + TEST_LOGIN1.origin);

  let modifiedLogin = TEST_LOGIN1.clone();
  modifiedLogin.timeLastUsed = 9000;
  let storageChangedPromised = TestUtils.topicObserved(
    "passwordmgr-storage-changed",
    (_, data) => data == "modifyLogin"
  );
  Services.logins.modifyLogin(TEST_LOGIN1, modifiedLogin);
  await storageChangedPromised;

  BrowserTestUtils.removeTab(newTab);

  await SpecialPowers.spawn(browser, [], async () => {
    await ContentTaskUtils.waitForCondition(() => {
      return !content.document.querySelector("confirmation-dialog").hidden;
    }, "waiting for confirmation-dialog to appear");
    Assert.ok(
      !content.document.querySelector("confirmation-dialog").hidden,
      "discard-changes confirmation-dialog should be visible after logging in to a site with a modified login present in the form"
    );
  });
});
