/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

let syncService = {};
ChromeUtils.import("resource://services-sync/service.js", syncService);
const service = syncService.Service;
const { UIState } = ChromeUtils.import("resource://services-sync/UIState.jsm");

function mockState(state) {
  UIState.get = () => ({
    status: state.status,
    lastSync: new Date(),
    email: state.email,
    avatarURL: state.avatarURL,
  });
}

add_task(async function setup() {
  let aboutLoginsTab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: "about:logins",
  });
  let getState = UIState.get;
  registerCleanupFunction(() => {
    BrowserTestUtils.removeTab(aboutLoginsTab);
    UIState.get = getState;
  });
});

add_task(async function test_logged_out() {
  mockState({ status: UIState.STATUS_NOT_CONFIGURED });
  Services.obs.notifyObservers(null, UIState.ON_UPDATE);

  let browser = gBrowser.selectedBrowser;
  await SpecialPowers.spawn(browser, [], async () => {
    let fxAccountsButton = content.document.querySelector("fxaccounts-button");
    ok(fxAccountsButton, "fxAccountsButton should exist");
    fxAccountsButton = Cu.waiveXrays(fxAccountsButton);
    await ContentTaskUtils.waitForCondition(
      () => fxAccountsButton._loggedIn === false,
      "waiting for _loggedIn to strictly equal false"
    );
    is(fxAccountsButton._loggedIn, false, "state should reflect not logged in");
  });
});

add_task(async function test_login_syncing_enabled() {
  const TEST_EMAIL = "test@example.com";
  const TEST_AVATAR_URL =
    "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAUAAAAFCAYAAACNbyblAAAAHElEQVQI12P4//8/w38GIAXDIBKE0DHxgljNBAAO9TXL0Y4OHwAAAABJRU5ErkJggg==";
  mockState({
    status: UIState.STATUS_SIGNED_IN,
    email: TEST_EMAIL,
    avatarURL: TEST_AVATAR_URL,
  });
  await SpecialPowers.pushPrefEnv({
    set: [["services.sync.engine.passwords", true]],
  });
  Services.obs.notifyObservers(null, UIState.ON_UPDATE);

  let browser = gBrowser.selectedBrowser;
  await SpecialPowers.spawn(
    browser,
    [[TEST_EMAIL, TEST_AVATAR_URL]],
    async ([expectedEmail, expectedAvatarURL]) => {
      let fxAccountsButton = content.document.querySelector(
        "fxaccounts-button"
      );
      ok(fxAccountsButton, "fxAccountsButton should exist");
      fxAccountsButton = Cu.waiveXrays(fxAccountsButton);
      await ContentTaskUtils.waitForCondition(
        () => fxAccountsButton._email === expectedEmail,
        "waiting for _email to strictly equal expectedEmail"
      );
      is(fxAccountsButton._loggedIn, true, "state should reflect logged in");
      is(fxAccountsButton._email, expectedEmail, "state should have email set");
      is(
        fxAccountsButton._avatarURL,
        expectedAvatarURL,
        "state should have avatarURL set"
      );
    }
  );

  await SpecialPowers.popPrefEnv();
});
