const { BrowserTestUtils } = ChromeUtils.import(
  "resource://testing-common/BrowserTestUtils.jsm"
);
const { RemoteSettings } = ChromeUtils.import(
  "resource://services-settings/remote-settings.js"
);
const { ASRouter } = ChromeUtils.import(
  "resource://activity-stream/lib/ASRouter.jsm"
);
const { RemoteSettingsExperimentLoader } = ChromeUtils.import(
  "resource://nimbus/lib/RemoteSettingsExperimentLoader.jsm"
);
const { ExperimentAPI } = ChromeUtils.import(
  "resource://nimbus/ExperimentAPI.jsm"
);
const { ExperimentFakes, ExperimentTestUtils } = ChromeUtils.import(
  "resource://testing-common/NimbusTestUtils.jsm"
);
const { ExperimentManager } = ChromeUtils.import(
  "resource://nimbus/lib/ExperimentManager.jsm"
);
const { TelemetryFeed } = ChromeUtils.import(
  "resource://activity-stream/lib/TelemetryFeed.jsm"
);
const { TelemetryTestUtils } = ChromeUtils.import(
  "resource://testing-common/TelemetryTestUtils.jsm"
);

const MESSAGE_CONTENT = {
  id: "xman_test_message",
  groups: [],
  content: {
    text: "This is a test CFR",
    addon: {
      id: "954390",
      icon:
        "chrome://activity-stream/content/data/content/assets/cfr_fb_container.png",
      title: "Facebook Container",
      users: 1455872,
      author: "Mozilla",
      rating: 4.5,
      amo_url: "https://addons.mozilla.org/firefox/addon/facebook-container/",
    },
    buttons: {
      primary: {
        label: {
          string_id: "cfr-doorhanger-extension-ok-button",
        },
        action: {
          data: {
            url: "about:blank",
          },
          type: "INSTALL_ADDON_FROM_URL",
        },
      },
      secondary: [
        {
          label: {
            string_id: "cfr-doorhanger-extension-cancel-button",
          },
          action: {
            type: "CANCEL",
          },
        },
        {
          label: {
            string_id: "cfr-doorhanger-extension-never-show-recommendation",
          },
        },
        {
          label: {
            string_id: "cfr-doorhanger-extension-manage-settings-button",
          },
          action: {
            data: {
              origin: "CFR",
              category: "general-cfraddons",
            },
            type: "OPEN_PREFERENCES_PAGE",
          },
        },
      ],
    },
    category: "cfrAddons",
    layout: "short_message",
    bucket_id: "CFR_M1",
    info_icon: {
      label: {
        string_id: "cfr-doorhanger-extension-sumo-link",
      },
      sumo_path: "extensionrecommendations",
    },
    heading_text: "Welcome to the experiment",
    notification_text: {
      string_id: "cfr-doorhanger-extension-notification2",
    },
  },
  trigger: {
    id: "openURL",
    params: [
      "www.facebook.com",
      "facebook.com",
      "www.instagram.com",
      "instagram.com",
      "www.whatsapp.com",
      "whatsapp.com",
      "web.whatsapp.com",
      "www.messenger.com",
      "messenger.com",
    ],
  },
  template: "cfr_doorhanger",
  frequency: {
    lifetime: 3,
  },
  targeting: "true",
};

const getCFRExperiment = async () => {
  let recipe = ExperimentFakes.recipe(
    // In tests by default studies/experiments are turned off. We turn them on
    // to run the test and rollback at the end. Cleanup causes unenrollment so
    // for cases where the test runs multiple times we need unique ids.
    `test_xman_cfr_${Date.now()}`,
    {
      id: "xman_test_message",
      bucketConfig: {
        count: 100,
        start: 0,
        total: 100,
        namespace: "mochitest",
        randomizationUnit: "normandy_id",
      },
    }
  );
  recipe.branches[0].features[0].featureId = "cfr";
  recipe.branches[0].features[0].value = MESSAGE_CONTENT;
  recipe.branches[1].features[0].featureId = "cfr";
  recipe.branches[1].features[0].value = MESSAGE_CONTENT;
  recipe.featureIds = ["cfr"];
  await ExperimentTestUtils.validateExperiment(recipe);
  return recipe;
};

const getLegacyCFRExperiment = async () => {
  let recipe = ExperimentFakes.recipe(`test_xman_cfr_${Date.now()}`, {
    id: "xman_test_message",
    bucketConfig: {
      count: 100,
      start: 0,
      total: 100,
      namespace: "mochitest",
      randomizationUnit: "normandy_id",
    },
  });

  delete recipe.branches[0].features;
  delete recipe.branches[1].features;
  recipe.branches[0].feature = {
    featureId: "cfr",
    value: MESSAGE_CONTENT,
  };
  recipe.branches[1].feature = {
    featureId: "cfr",
    value: MESSAGE_CONTENT,
  };
  return recipe;
};

const client = RemoteSettings("nimbus-desktop-experiments");

// no `add_task` because we want to run this setup before each test not before
// the entire test suite.
async function setup(experiment) {
  // Store the experiment in RS local db to bypass synchronization.
  await client.db.importChanges({}, Date.now(), [experiment], { clear: true });
  await SpecialPowers.pushPrefEnv({
    set: [
      ["app.shield.optoutstudies.enabled", true],
      ["datareporting.healthreport.uploadEnabled", true],
      [
        "browser.newtabpage.activity-stream.asrouter.providers.messaging-experiments",
        `{"id":"messaging-experiments","enabled":true,"type":"remote-experiments","updateCycleInMs":0}`,
      ],
    ],
  });
}

async function cleanup() {
  await client.db.clear();
  await SpecialPowers.popPrefEnv();
  // Reload the provider
  await ASRouter._updateMessageProviders();
}

add_task(async function test_loading_experimentsAPI() {
  let experiment = await getCFRExperiment();
  await setup(experiment);
  // Fetch the new recipe from RS
  await RemoteSettingsExperimentLoader.updateRecipes();
  await BrowserTestUtils.waitForCondition(
    () => ExperimentAPI.getExperiment({ featureId: "cfr" }),
    "ExperimentAPI should return an experiment"
  );

  const telemetryFeedInstance = new TelemetryFeed();
  Assert.ok(
    telemetryFeedInstance.isInCFRCohort,
    "Telemetry should return true"
  );

  // Reload the provider
  await ASRouter._updateMessageProviders();
  // Wait to load the messages from the messaging-experiments provider
  await ASRouter.loadMessagesFromAllProviders();

  Assert.ok(
    ASRouter.state.messages.find(m => m.id === "xman_test_message"),
    "Experiment message found in ASRouter state"
  );

  await cleanup();
});

add_task(async function test_loading_experimentsAPI_legacy() {
  let experiment = await getLegacyCFRExperiment();
  await setup(experiment);
  // Fetch the new recipe from RS
  await RemoteSettingsExperimentLoader.updateRecipes();
  await BrowserTestUtils.waitForCondition(
    () => ExperimentAPI.getExperiment({ featureId: "cfr" }),
    "ExperimentAPI should return an experiment"
  );

  const telemetryFeedInstance = new TelemetryFeed();
  Assert.ok(
    telemetryFeedInstance.isInCFRCohort,
    "Telemetry should return true"
  );

  // Reload the provider
  await ASRouter._updateMessageProviders();
  // Wait to load the messages from the messaging-experiments provider
  await ASRouter.loadMessagesFromAllProviders();

  Assert.ok(
    ASRouter.state.messages.find(m => m.id === "xman_test_message"),
    "Experiment message found in ASRouter state"
  );

  await cleanup();
});

add_task(async function test_exposure_ping() {
  // Reset this check to allow sending multiple exposure pings in tests
  NimbusFeatures.cfr._didSendExposureEvent = false;
  let experiment = await getCFRExperiment();
  await setup(experiment);
  Services.telemetry.clearScalars();
  // Fetch the new recipe from RS
  await RemoteSettingsExperimentLoader.updateRecipes();
  await BrowserTestUtils.waitForCondition(
    () => ExperimentAPI.getExperiment({ featureId: "cfr" }),
    "ExperimentAPI should return an experiment"
  );

  // Reload the provider
  await ASRouter._updateMessageProviders();
  // Wait to load the messages from the messaging-experiments provider
  await ASRouter.loadMessagesFromAllProviders();

  Assert.ok(
    ASRouter.state.messages.find(m => m.id === "xman_test_message"),
    "Experiment message found in ASRouter state"
  );

  const exposureSpy = sinon.spy(ExperimentAPI, "recordExposureEvent");

  await ASRouter.sendTriggerMessage({
    tabId: 1,
    browser: gBrowser.selectedBrowser,
    id: "openURL",
    param: { host: "messenger.com" },
  });

  Assert.ok(exposureSpy.callCount === 1, "Should send exposure ping");
  const scalars = TelemetryTestUtils.getProcessScalars("parent", true, true);
  TelemetryTestUtils.assertKeyedScalar(
    scalars,
    "telemetry.event_counts",
    "normandy#expose#nimbus_experiment",
    1
  );

  exposureSpy.restore();
  await cleanup();
});

add_task(async function test_exposure_ping_legacy() {
  // Reset this check to allow sending multiple exposure pings in tests
  NimbusFeatures.cfr._didSendExposureEvent = false;
  let experiment = await getLegacyCFRExperiment();
  await setup(experiment);
  Services.telemetry.clearScalars();
  // Fetch the new recipe from RS
  await RemoteSettingsExperimentLoader.updateRecipes();
  await BrowserTestUtils.waitForCondition(
    () => ExperimentAPI.getExperiment({ featureId: "cfr" }),
    "ExperimentAPI should return an experiment"
  );

  // Reload the provider
  await ASRouter._updateMessageProviders();
  // Wait to load the messages from the messaging-experiments provider
  await ASRouter.loadMessagesFromAllProviders();

  Assert.ok(
    ASRouter.state.messages.find(m => m.id === "xman_test_message"),
    "Experiment message found in ASRouter state"
  );

  const exposureSpy = sinon.spy(ExperimentAPI, "recordExposureEvent");

  await ASRouter.sendTriggerMessage({
    tabId: 1,
    browser: gBrowser.selectedBrowser,
    id: "openURL",
    param: { host: "messenger.com" },
  });

  Assert.ok(exposureSpy.callCount === 1, "Should send exposure ping");
  const scalars = TelemetryTestUtils.getProcessScalars("parent", true, true);
  TelemetryTestUtils.assertKeyedScalar(
    scalars,
    "telemetry.event_counts",
    "normandy#expose#nimbus_experiment",
    1
  );

  exposureSpy.restore();
  await cleanup();
});

add_task(async function test_forceEnrollUpdatesMessages() {
  const experiment = await getCFRExperiment();

  await setup(experiment);
  await SpecialPowers.pushPrefEnv({
    set: [["nimbus.debug", true]],
  });

  Assert.equal(
    ASRouter.state.messages.filter(m => m.id === "xman_test_message").length,
    0,
    "Experiment message should not be found until we opt in"
  );

  await RemoteSettingsExperimentLoader.optInToExperiment({
    slug: experiment.slug,
    branch: experiment.branches[0].slug,
  });

  await BrowserTestUtils.waitForCondition(
    () =>
      !!ASRouter.state.messages.filter(m => m.id === "xman_test_message")
        .length,
    "waiting for ASRouter to update messages"
  );

  Assert.equal(
    ASRouter.state.messages.filter(m => m.id === "xman_test_message").length,
    1,
    "Experiment message should be found after opt in"
  );

  await ExperimentManager.unenroll(`optin-${experiment.slug}`, "cleanup");
  await SpecialPowers.popPrefEnv();
  await cleanup();
});

add_task(async function test_emptyMessage() {
  const experiment = ExperimentFakes.recipe("empty", {
    branches: [
      {
        slug: "a",
        ratio: 1,
        features: [
          {
            featureId: "cfr",
            value: {},
          },
        ],
      },
    ],
    bucketConfig: {
      start: 0,
      count: 100,
      total: 100,
      namespace: "mochitest",
      randomizationUnit: "normandy_id",
    },
  });

  await setup(experiment);
  await RemoteSettingsExperimentLoader.updateRecipes();
  await BrowserTestUtils.waitForCondition(
    () => ExperimentAPI.getExperiment({ featureId: "cfr" }),
    "ExperimentAPI should return an experiment"
  );

  await ASRouter._updateMessageProviders();

  const experimentsProvider = ASRouter.state.providers.find(
    p => p.id === "messaging-experiments"
  );

  // Clear all messages
  ASRouter.setState(state => ({
    messages: [],
  }));

  await ASRouter.loadMessagesFromAllProviders([experimentsProvider]);

  Assert.deepEqual(
    ASRouter.state.messages,
    [],
    "ASRouter should have loaded zero messages"
  );

  await cleanup();
});
