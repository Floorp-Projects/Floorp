/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for engagement telemetry for tips using Glean.

ChromeUtils.defineESModuleGetters(this, {
  PromiseUtils: "resource://gre/modules/PromiseUtils.sys.mjs",
});

add_setup(async function() {
  makeProfileResettable();

  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.searchEngagementTelemetry.enabled", true],
      ["browser.urlbar.quickactions.enabled", true],
      ["browser.urlbar.suggest.quickactions", true],
      ["browser.urlbar.quickactions.showInZeroPrefix", true],
    ],
  });
  registerCleanupFunction(async function() {
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
      type: "searchTip_redirect",
      expected: "tip_redirect",
    },
    {
      type: "test",
      expected: "tip_unknown",
    },
  ];

  for (const { type, expected } of testData) {
    const deferred = PromiseUtils.defer();
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
      await selectRow(type);
      EventUtils.synthesizeKey("VK_RETURN");
      await deferred.promise;

      assertGleanTelemetry([
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
        results: "search_engine,action,intervention_update",
      },
    ]
  );
});

function assertGleanTelemetry(expectedExtraList) {
  const telemetries = Glean.urlbar.engagement.testGetValue();
  Assert.equal(telemetries.length, expectedExtraList.length);

  for (let i = 0; i < telemetries.length; i++) {
    const telemetry = telemetries[i];
    Assert.equal(telemetry.category, "urlbar");
    Assert.equal(telemetry.name, "engagement");

    const expectedExtra = expectedExtraList[i];
    for (const key of Object.keys(expectedExtra)) {
      Assert.equal(
        telemetry.extra[key],
        expectedExtra[key],
        `${key} is correct`
      );
    }
  }
}

async function doInterventionTest(keyword, type, dialog, expectedTelemetry) {
  await doTest(async browser => {
    await openPopup(keyword);
    await selectRow(type);
    const onDialog = BrowserTestUtils.promiseAlertDialog("cancel", dialog, {
      isSubDialog: true,
    });
    EventUtils.synthesizeKey("VK_RETURN");
    await onDialog;

    assertGleanTelemetry(expectedTelemetry);
  });
}

async function doTest(testFn) {
  Services.fog.testResetFOG();

  await BrowserTestUtils.withNewTab(gBrowser, testFn);
}

async function openPopup(input) {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: input,
    fireInputEvent: true,
  });
}

async function selectRow(type) {
  for (let i = 0; i < UrlbarTestUtils.getResultCount(window); i++) {
    const detail = await UrlbarTestUtils.getDetailsOfResultAt(window, i);
    if (detail.result.payload.type === type) {
      UrlbarTestUtils.setSelectedRowIndex(window, i);
      return;
    }
  }
}
