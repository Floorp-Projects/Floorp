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
const SIMPLIFIED_WELCOME_ENABLED_PREF = "browser.aboutwelcome.enabled";

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

  Services.prefs.setCharPref(BRANCH_PREF, "join-supercharge");
  // Set about:welcome to use trailhead flow
  Services.prefs.setBoolPref(SIMPLIFIED_WELCOME_ENABLED_PREF, false);

  // Reset trailhead so it loads the new branch.
  Services.prefs.clearUserPref("trailhead.firstrun.didSeeAboutWelcome");
  await ASRouter.setState({ trailheadInitialized: false });
  ASRouter._updateMessageProviders();
  await ASRouter.loadMessagesFromAllProviders();

  registerCleanupFunction(async () => {
    // Separate cleanup methods between mac and windows
    if (AppConstants.platform === "macosx") {
      const { path } = Services.dirsvc.get("GreD", Ci.nsIFile).parent.parent;
      const attributionSvc = Cc["@mozilla.org/mac-attribution;1"].getService(
        Ci.nsIMacAttributionService
      );
      attributionSvc.setReferrerUrl(path, "", true);
    }
    // Clear cache call is only possible in a testing environment
    let env = Cc["@mozilla.org/process/environment;1"].getService(
      Ci.nsIEnvironment
    );
    env.set("XPCSHELL_TEST_PROFILE_DIR", "testing");
    Services.prefs.clearUserPref(BRANCH_PREF);
    Services.prefs.clearUserPref(SIMPLIFIED_WELCOME_ENABLED_PREF);
    await AttributionCode.deleteFileAsync();
    AttributionCode._clearCache();
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

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:welcome" },
    async browser => {
      let modalText = await SpecialPowers.spawn(browser, [], async () => {
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
          await ContentTaskUtils.waitForCondition(
            () => content.document.querySelector(selector) !== null,
            `Should render ${selector}`
          );
        }

        return content.document.querySelector(".ReturnToAMOText").innerText;
      });
      // Make sure strings are properly shown
      Assert.equal(modalText, "Now let’s get you mochitest_name.");
    }
  );
});
