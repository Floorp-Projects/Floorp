/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test edge cases for engagement.

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/browser/components/urlbar/tests/ext/browser/head.js",
  this
);

add_setup(async function () {
  await setup();
});

/**
 * UrlbarProvider that does not add any result.
 */
class NoResponseTestProvider extends UrlbarTestUtils.TestProvider {
  constructor() {
    super({ name: "TestProviderNoResponse ", results: [] });
    this.#deferred = Promise.withResolvers();
  }

  get type() {
    return UrlbarUtils.PROVIDER_TYPE.HEURISTIC;
  }

  async startQuery(context, addCallback) {
    await this.#deferred.promise;
  }

  done() {
    this.#deferred.resolve();
  }

  #deferred = null;
}
const noResponseProvider = new NoResponseTestProvider();

/**
 * UrlbarProvider that adds a heuristic result immediately as usual.
 */
class AnotherHeuristicProvider extends UrlbarTestUtils.TestProvider {
  constructor({ results }) {
    super({ name: "TestProviderAnotherHeuristic ", results });
    this.#deferred = Promise.withResolvers();
  }

  get type() {
    return UrlbarUtils.PROVIDER_TYPE.HEURISTIC;
  }

  async startQuery(context, addCallback) {
    for (const result of this._results) {
      addCallback(this, result);
    }

    this.#deferred.resolve(context);
  }

  onQueryStarted() {
    return this.#deferred.promise;
  }

  #deferred = null;
}
const anotherHeuristicProvider = new AnotherHeuristicProvider({
  results: [
    Object.assign(
      new UrlbarResult(
        UrlbarUtils.RESULT_TYPE.URL,
        UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        { url: "https://example.com/immediate" }
      ),
      { heuristic: true }
    ),
  ],
});

add_task(async function engagement_before_showing_results() {
  await SpecialPowers.pushPrefEnv({
    // Avoid showing search tip.
    set: [["browser.urlbar.tipShownCount.searchTip_onboard", 999]],
  });

  // Increase chunk delays to delay the call to notifyResults.
  let originalChunkTimeout = UrlbarProvidersManager.CHUNK_RESULTS_DELAY_MS;
  UrlbarProvidersManager.CHUNK_RESULTS_DELAY_MS = 1000000;

  // Add a provider that waits forever in startQuery() to avoid fireing
  // heuristicProviderTimer.
  UrlbarProvidersManager.registerProvider(noResponseProvider);

  // Add a provider that add a result immediately as usual.
  UrlbarProvidersManager.registerProvider(anotherHeuristicProvider);

  const cleanup = () => {
    UrlbarProvidersManager.unregisterProvider(noResponseProvider);
    UrlbarProvidersManager.unregisterProvider(anotherHeuristicProvider);
    UrlbarProvidersManager.CHUNK_RESULTS_DELAY_MS = originalChunkTimeout;
  };
  registerCleanupFunction(cleanup);

  await doTest(async browser => {
    // Try to show the results.
    await UrlbarTestUtils.inputIntoURLBar(window, "exam");

    // Wait until starting the query and filling expected results.
    const context = await anotherHeuristicProvider.onQueryStarted();
    const query = UrlbarProvidersManager.queries.get(context);
    await BrowserTestUtils.waitForCondition(
      () =>
        query.unsortedResults.some(
          r => r.providerName === "HeuristicFallback"
        ) &&
        query.unsortedResults.some(
          r => r.providerName === anotherHeuristicProvider.name
        )
    );

    // Type Enter key before showing any results.
    await doEnter();

    assertEngagementTelemetry([
      {
        selected_result: "input_field",
        selected_result_subtype: "",
        provider: undefined,
        results: "",
        groups: "",
      },
    ]);

    // Clear the pending query.
    noResponseProvider.done();
  });

  cleanup();
  await SpecialPowers.popPrefEnv();
});

add_task(async function engagement_after_closing_results() {
  const TRIGGERS = [
    () => EventUtils.synthesizeKey("KEY_Escape"),
    () => {
      // We intentionally turn off this a11y check, because the following click
      // is sent to test the telemetry behavior using an alternative way of the
      // urlbar dismissal, where other ways are accessible (and tested above),
      // therefore this test can be ignored.
      AccessibilityUtils.setEnv({
        mustHaveAccessibleRule: false,
      });
      EventUtils.synthesizeMouseAtCenter(
        document.getElementById("customizableui-special-spring2"),
        {}
      );
      AccessibilityUtils.resetEnv();
    },
  ];

  for (const trigger of TRIGGERS) {
    await doTest(async browser => {
      await openPopup("test");
      await UrlbarTestUtils.promisePopupClose(window, () => {
        trigger();
      });
      Assert.equal(
        gURLBar.value,
        "test",
        "The inputted text remains even if closing the results"
      );
      // The tested trigger should not record abandonment event.
      assertAbandonmentTelemetry([]);

      // Endgagement.
      await doEnter();

      assertEngagementTelemetry([
        {
          selected_result: "input_field",
          selected_result_subtype: "",
          provider: undefined,
          results: "",
          groups: "",
        },
      ]);
    });
  }
});

add_task(async function enter_to_reload_current_url() {
  await doTest(async browser => {
    // Open a URL once.
    await openPopup("https://example.com");
    await doEnter();

    // Focus the urlbar.
    EventUtils.synthesizeMouseAtCenter(gURLBar.inputField, {});
    await BrowserTestUtils.waitForCondition(
      () => window.document.activeElement === gURLBar.inputField
    );
    await UrlbarTestUtils.promiseSearchComplete(window);

    // Press Enter key to reload the page without selecting any suggestions.
    await doEnter();

    assertEngagementTelemetry([
      {
        selected_result: "url",
        selected_result_subtype: "",
        provider: "HeuristicFallback",
        results: "url",
        groups: "heuristic",
      },
      {
        selected_result: "input_field",
        selected_result_subtype: "",
        provider: undefined,
        results: "action",
        groups: "suggested_index",
      },
    ]);
  });
});
