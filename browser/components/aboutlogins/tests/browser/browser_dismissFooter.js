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
  await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: "about:logins",
  });
  let getState = UIState.get;
  registerCleanupFunction(() => {
    BrowserTestUtils.removeTab(gBrowser.selectedTab);
    UIState.get = getState;
    SpecialPowers.clearUserPref("signon.management.page.hideMobileFooter");
    Services.logins.removeAllLogins();
  });
});

add_task(async function test_open_links() {
  // urlFinal is what the full link should be. urlBase is what gets Footer_Menu
  // appended to it.
  const linkArray = [
    {
      urlFinal: "https://example.com/android?utm_creative=Footer_Menu",
      urlBase: "https://example.com/android?utm_creative=",
      pref: "signon.management.page.mobileAndroidURL",
      selector: ".image-play-store",
    },
    {
      urlFinal: "https://example.com/apple?utm_creative=Footer_Menu",
      urlBase: "https://example.com/apple?utm_creative=",
      pref: "signon.management.page.mobileAppleURL",
      selector: ".image-app-store",
    },
  ];

  for (const { urlFinal, urlBase, pref, selector } of linkArray) {
    info("Test on " + urlFinal);
    const TEST_EMAIL = "test@example.com";
    const TEST_AVATAR_URL =
      "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAUAAAAFCAYAAACNbyblAAAAHElEQVQI12P4//8/w38GIAXDIBKE0DHxgljNBAAO9TXL0Y4OHwAAAABJRU5ErkJggg==";
    mockState({
      status: UIState.STATUS_SIGNED_IN,
      email: TEST_EMAIL,
      avatarURL: TEST_AVATAR_URL,
    });

    await SpecialPowers.pushPrefEnv({
      set: [[pref, urlBase], ["services.sync.engine.passwords", true]],
    });

    Services.obs.notifyObservers(null, UIState.ON_UPDATE);

    let promiseNewTab = BrowserTestUtils.waitForNewTab(gBrowser, urlFinal);
    let browser = gBrowser.selectedBrowser;

    await ContentTask.spawn(browser, selector, async buttonClass => {
      let footer = Cu.waiveXrays(
        content.document
          .querySelector("login-item")
          .shadowRoot.querySelector("login-footer")
      );
      ok(!footer.hidden, "Footer is visible");
      let button = footer.shadowRoot.querySelector(buttonClass);

      button.click();
    });

    info("waiting for new tab to get opened");
    let newTab = await promiseNewTab;
    ok(true, "New tab opened to " + urlFinal);

    BrowserTestUtils.removeTab(newTab);
  }
});

add_task(async function dismissFooter() {
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

  await ContentTask.spawn(browser, null, async () => {
    let footer = Cu.waiveXrays(
      content.document
        .querySelector("login-item")
        .shadowRoot.querySelector("login-footer")
    );
    let dismissButton = footer.shadowRoot.querySelector(".close");

    dismissButton.click();
    await ContentTaskUtils.waitForCondition(
      () => ContentTaskUtils.is_hidden(footer),
      "Waiting for the footer to be hidden"
    );
    ok(
      ContentTaskUtils.is_hidden(footer),
      "Footer should be hidden after clicking dismiss button"
    );
  });
});
