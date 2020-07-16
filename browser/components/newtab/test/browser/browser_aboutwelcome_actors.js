"use strict";

const SEPARATE_ABOUT_WELCOME_PREF = "browser.aboutwelcome.enabled";
const DID_SEE_ABOUT_WELCOME_PREF = "trailhead.firstrun.didSeeAboutWelcome";
const ABOUT_WELCOME_OVERRIDE_CONTENT = "browser.aboutwelcome.overrideContent";

const { PrivateBrowsingUtils } = ChromeUtils.import(
  "resource://gre/modules/PrivateBrowsingUtils.jsm"
);
const { FxAccounts } = ChromeUtils.import(
  "resource://gre/modules/FxAccounts.jsm"
);

const SIMPLIFIED_WELCOME_CONTENT = {
  title: {
    string_id: "onboarding-welcome-header",
  },
  startButton: {
    label: {
      string_id: "onboarding-start-browsing-button-label",
    },
    message_id: "START_BROWSING_BUTTON",
    action: {
      type: "OPEN_AWESOME_BAR",
    },
  },
  cards: [
    {
      content: {
        title: {
          string_id: "onboarding-data-sync-title",
        },
        text: {
          string_id: "onboarding-data-sync-text2",
        },
        icon: "devices",
        primary_button: {
          label: {
            string_id: "onboarding-data-sync-button2",
          },
          action: {
            type: "OPEN_URL",
            addFlowParams: true,
            data: {
              args:
                "https://accounts.firefox.com/?service=sync&action=email&context=fx_desktop_v3&entrypoint=activity-stream-firstrun&style=trailhead",
              where: "tabshifted",
            },
          },
        },
      },
      id: "TRAILHEAD_CARD_2",
      order: 1,
      blockOnClick: false,
    },
    {
      content: {
        title: {
          string_id: "onboarding-firefox-monitor-title",
        },
        text: {
          string_id: "onboarding-firefox-monitor-text2",
        },
        icon: "ffmonitor",
        primary_button: {
          label: {
            string_id: "onboarding-firefox-monitor-button",
          },
          action: {
            type: "OPEN_URL",
            data: {
              args: "https://monitor.firefox.com/",
              where: "tabshifted",
            },
          },
        },
      },
      id: "TRAILHEAD_CARD_3",
      order: 2,
      blockOnClick: false,
    },
    {
      content: {
        title: {
          string_id: "onboarding-browse-privately-title",
        },
        text: {
          string_id: "onboarding-browse-privately-text",
        },
        icon: "private",
        primary_button: {
          label: {
            string_id: "onboarding-browse-privately-button",
          },
          action: {
            type: "OPEN_PRIVATE_BROWSER_WINDOW",
          },
        },
      },
      id: "TRAILHEAD_CARD_4",
      order: 3,
      blockOnClick: true,
    },
  ],
};

/**
 * Sets the aboutwelcome pref to enabled simplified welcome UI
 */
async function setAboutWelcomePref(value) {
  await pushPrefs([
    ABOUT_WELCOME_OVERRIDE_CONTENT,
    JSON.stringify(SIMPLIFIED_WELCOME_CONTENT),
  ]);
  return pushPrefs([SEPARATE_ABOUT_WELCOME_PREF, value]);
}

async function openAboutWelcome() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:welcome",
    true
  );
  registerCleanupFunction(() => {
    BrowserTestUtils.removeTab(tab);
  });
  return tab.linkedBrowser;
}

// Test events from AboutWelcomeUtils
async function test_set_message() {
  Services.prefs.setBoolPref(DID_SEE_ABOUT_WELCOME_PREF, false);
  await openAboutWelcome();
  Assert.equal(
    Services.prefs.getBoolPref(DID_SEE_ABOUT_WELCOME_PREF, false),
    true,
    "Pref was set"
  );
}

async function test_open_awesome_bar(browser, message) {
  await ContentTask.spawn(browser, message, async () => {
    await ContentTaskUtils.waitForCondition(
      () =>
        content.document.querySelector(
          "button[data-l10n-id=onboarding-start-browsing-button-label]"
        ),
      "Wait for start browsing button to load"
    );
    let button = content.document.querySelector(
      "button[data-l10n-id=onboarding-start-browsing-button-label]"
    );
    button.click();
  });
  Assert.ok(gURLBar.focused, "Focus should be on awesome bar");
}

async function test_open_private_browser(browser, message) {
  let newWindowPromise = BrowserTestUtils.waitForNewWindow();
  await ContentTask.spawn(browser, message, async () => {
    await ContentTaskUtils.waitForCondition(
      () =>
        content.document.querySelector(
          "button[data-l10n-id=onboarding-browse-privately-button]"
        ),
      "Wait for private browsing button to load"
    );
    let button = content.document.querySelector(
      "button[data-l10n-id=onboarding-browse-privately-button]"
    );
    button.click();
  });
  let win = await newWindowPromise;
  Assert.ok(PrivateBrowsingUtils.isWindowPrivate(win));
  await BrowserTestUtils.closeWindow(win);
}

// Test Fxaccounts MetricsFlowURI

add_task(async function setup() {
  const sandbox = sinon.createSandbox();
  sandbox.stub(FxAccounts.config, "promiseMetricsFlowURI").resolves("");

  await pushPrefs([
    ABOUT_WELCOME_OVERRIDE_CONTENT,
    JSON.stringify(SIMPLIFIED_WELCOME_CONTENT),
  ]);

  registerCleanupFunction(() => {
    sandbox.restore();
  });
});

test_newtab(
  {
    async before({ pushPrefs }) {
      await pushPrefs(["browser.aboutwelcome.enabled", true]);
    },
    test: async function test_startBrowsing() {
      await ContentTaskUtils.waitForCondition(
        () =>
          content.document.querySelector(
            "button[data-l10n-id=onboarding-start-browsing-button-label]"
          ),
        "Wait for start browsing button to load"
      );
    },
    after() {
      ok(
        FxAccounts.config.promiseMetricsFlowURI.callCount === 1,
        "Stub was called"
      );
      Assert.equal(
        FxAccounts.config.promiseMetricsFlowURI.firstCall.args[0],
        "aboutwelcome",
        "Called by AboutWelcomeParent"
      );
    },
  },
  "about:welcome"
);

add_task(async function test_onContentMessage() {
  await setAboutWelcomePref(true);
  let browser = await openAboutWelcome();

  //case "SET_WELCOME_MESSAGE_SEEN"
  await test_set_message();

  // //case "OPEN_AWESOME_BAR"
  await test_open_awesome_bar(browser, "Open awesome bar");

  //case "OPEN_PRIVATE_BROWSER_WINDOW"
  await test_open_private_browser(browser, "Open private window");
});
