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
const { TelemetryFeed } = ChromeUtils.import(
  "resource://activity-stream/lib/TelemetryFeed.jsm"
);
const { TelemetryTestUtils } = ChromeUtils.import(
  "resource://testing-common/TelemetryTestUtils.jsm"
);

const MESSAGE_CONTENT = {
  id: "xman_test_message",
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
            url: null,
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
    enabled: true,
    value: MESSAGE_CONTENT,
  };
  recipe.branches[1].feature = {
    featureId: "cfr",
    enabled: true,
    value: MESSAGE_CONTENT,
  };
  return recipe;
};

const client = RemoteSettings("nimbus-desktop-experiments");

// no `add_task` because we want to run this setup before each test not before
// the entire test suite.
async function setup(experiment) {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["app.shield.optoutstudies.enabled", true],
      [
        "browser.newtabpage.activity-stream.asrouter.providers.messaging-experiments",
        `{"id":"messaging-experiments","enabled":true,"type":"remote-experiments","messageGroups":["cfr","spotlight","infobar","aboutwelcome"],"updateCycleInMs":0}`,
      ],
    ],
  });

  await client.db.importChanges({}, 42, [experiment], { clear: true });
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

add_task(async function test_featureless_experiment() {
  let experiment = await getCFRExperiment();
  // Remove the feature property from the branch
  experiment.branches.forEach(branch => delete branch.features);
  Assert.ok(ExperimentAPI._store.getAllActive().length === 0, "Empty store");
  await setup(experiment);
  // Fetch the new recipe from RS
  await RemoteSettingsExperimentLoader.updateRecipes();
  // Enrollment was successful; featureless experiments shouldn't break anything
  await BrowserTestUtils.waitForCondition(
    () => ExperimentAPI._store.getAllActive().length === 1,
    "ExperimentAPI should return an experiment"
  );

  await cleanup();
});
