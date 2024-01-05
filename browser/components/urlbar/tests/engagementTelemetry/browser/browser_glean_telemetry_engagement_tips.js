/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for engagement telemetry for tips using Glean.

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/browser/components/urlbar/tests/browser-tips/head.js",
  this
);

add_setup(async function () {
  makeProfileResettable();

  Services.fog.setMetricsFeatureConfig(
    JSON.stringify({ "urlbar.engagement": false })
  );
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.quickactions.enabled", false]],
  });

  registerCleanupFunction(async function () {
    Services.fog.setMetricsFeatureConfig("{}");
    await SpecialPowers.popPrefEnv();
  });
});

add_task(async function selected_result_tip() {
  const testData = [
    {
      type: "searchTip_onboard",
      expected: "tip_onboard",
    },
    {
      type: "searchTip_persist",
      expected: "tip_persist",
    },
    {
      type: "searchTip_redirect",
      expected: "tip_redirect",
    },
    {
      type: "test",
      expected: "tip_unknown",
    },
  ];

  for (const { type, expected } of testData) {
    const deferred = Promise.withResolvers();
    const provider = new UrlbarTestUtils.TestProvider({
      results: [
        new UrlbarResult(
          UrlbarUtils.RESULT_TYPE.TIP,
          UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
          {
            type,
            helpUrl: "https://example.com/",
            titleL10n: { id: "urlbar-search-tips-confirm" },
            buttons: [
              {
                url: "https://example.com/",
                l10n: { id: "urlbar-search-tips-confirm" },
              },
            ],
          }
        ),
      ],
      priority: 1,
      onEngagement: () => {
        deferred.resolve();
      },
    });
    UrlbarProvidersManager.registerProvider(provider);

    await doTest(async browser => {
      await openPopup("example");
      await selectRowByType(type);
      EventUtils.synthesizeKey("VK_RETURN");
      await deferred.promise;

      assertEngagementTelemetry([
        {
          selected_result: expected,
          results: expected,
        },
      ]);
    });

    UrlbarProvidersManager.unregisterProvider(provider);
  }
});

add_task(async function selected_result_intervention_clear() {
  await doInterventionTest(
    SEARCH_STRINGS.CLEAR,
    "intervention_clear",
    "chrome://browser/content/sanitize.xhtml",
    [
      {
        selected_result: "intervention_clear",
        results: "search_engine,intervention_clear",
      },
    ]
  );
});

add_task(async function selected_result_intervention_refresh() {
  await doInterventionTest(
    SEARCH_STRINGS.REFRESH,
    "intervention_refresh",
    "chrome://global/content/resetProfile.xhtml",
    [
      {
        selected_result: "intervention_refresh",
        results: "search_engine,intervention_refresh",
      },
    ]
  );
});

add_task(async function selected_result_intervention_update() {
  // Updates are disabled for MSIX packages, this test is irrelevant for them.
  if (
    AppConstants.platform === "win" &&
    Services.sysinfo.getProperty("hasWinPackageId")
  ) {
    return;
  }
  await UpdateUtils.setAppUpdateAutoEnabled(false);
  await initUpdate({ queryString: "&noUpdates=1" });
  UrlbarProviderInterventions.checkForBrowserUpdate(true);
  await processUpdateSteps([
    {
      panelId: "checkingForUpdates",
      checkActiveUpdate: null,
      continueFile: CONTINUE_CHECK,
    },
    {
      panelId: "noUpdatesFound",
      checkActiveUpdate: null,
      continueFile: null,
    },
  ]);

  await doInterventionTest(
    SEARCH_STRINGS.UPDATE,
    "intervention_update_refresh",
    "chrome://global/content/resetProfile.xhtml",
    [
      {
        selected_result: "intervention_update",
        results: "search_engine,intervention_update",
      },
    ]
  );
});

async function doInterventionTest(keyword, type, dialog, expectedTelemetry) {
  await doTest(async browser => {
    await openPopup(keyword);
    await selectRowByType(type);
    const onDialog = BrowserTestUtils.promiseAlertDialog("cancel", dialog, {
      isSubDialog: true,
    });
    EventUtils.synthesizeKey("VK_RETURN");
    await onDialog;

    assertEngagementTelemetry(expectedTelemetry);
  });
}
