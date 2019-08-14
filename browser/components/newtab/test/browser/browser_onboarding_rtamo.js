const { ASRouter } = ChromeUtils.import(
  "resource://activity-stream/lib/ASRouter.jsm"
);
const { OnboardingMessageProvider } = ChromeUtils.import(
  "resource://activity-stream/lib/OnboardingMessageProvider.jsm"
);
const { AttributionCode } = ChromeUtils.import(
  "resource:///modules/AttributionCode.jsm"
);

const BRANCH_PREF = "trailhead.firstrun.branches";

async function setRTAMOOnboarding() {
  await ASRouter.forceAttribution({
    campaign: "non-fx-button",
    source: "addons.mozilla.org",
    content: "iridium@particlecore.github.io",
  });
  AttributionCode._clearCache();
  const data = await AttributionCode.getAttrDataAsync();
  Assert.equal(
    data.source,
    "addons.mozilla.org",
    "Attribution data should be set"
  );

  Services.prefs.setCharPref(BRANCH_PREF, "control");

  // Reset trailhead so it loads the new branch.
  Services.prefs.clearUserPref("trailhead.firstrun.didSeeAboutWelcome");
  await ASRouter.setState({ trailheadInitialized: false });
  await ASRouter.setupTrailhead();
  ASRouter._updateMessageProviders();
  await ASRouter.loadMessagesFromAllProviders();

  registerCleanupFunction(() => {
    Services.prefs.clearUserPref(BRANCH_PREF);
  });
}

add_task(async function setup() {
  // Store it in order to restore to the original value
  const { getAddonInfo } = OnboardingMessageProvider;
  // Prevent fetching the real addon url and making a network request
  OnboardingMessageProvider.getAddonInfo = () => ({
    name: "mochitest_name",
    iconURL: "mochitest_iconURL",
    url: "https://example.com",
  });

  registerCleanupFunction(() => {
    OnboardingMessageProvider.getAddonInfo = getAddonInfo;
  });
});

add_task(async () => {
  await setRTAMOOnboarding();

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:welcome",
    false
  );
  let browser = tab.linkedBrowser;

  await ContentTask.spawn(browser, {}, async () => {
    // Wait for Activity Stream to load
    await ContentTaskUtils.waitForCondition(
      () => content.document.querySelector(".activity-stream"),
      `Should render Activity Stream`
    );
    await ContentTaskUtils.waitForCondition(
      () => content.document.body.classList.contains("welcome"),
      "The modal setup should be completed"
    );
    await ContentTaskUtils.waitForCondition(
      () => content.document.body.classList.contains("hide-main"),
      "You shouldn't be able to see newtabpage content"
    );
    for (let selector of [
      // ReturnToAMO elements
      ".ReturnToAMOOverlay",
      ".ReturnToAMOContainer",
      ".ReturnToAMOAddonContents",
      ".ReturnToAMOIcon",
    ]) {
      ok(content.document.querySelector(selector), `Should render ${selector}`);
    }
    // Make sure strings are properly shown
    Assert.equal(
      content.document.querySelector(".ReturnToAMOText").innerText,
      "Now let’s get you mochitest_name."
    );
  });

  BrowserTestUtils.removeTab(tab);
});
