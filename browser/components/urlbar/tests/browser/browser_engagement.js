/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests the UrlbarProvider.onEngagement() method.

"use strict";

add_task(async function abandonment() {
  await doTest({
    expectedEndState: "abandonment",
    endEngagement: async () => {
      await UrlbarTestUtils.promisePopupClose(window, () => gURLBar.blur());
    },
  });
});

add_task(async function engagement() {
  await BrowserTestUtils.withNewTab("about:blank", async () => {
    await doTest({
      expectedEndState: "engagement",
      endEngagement: async () => {
        let result, element;
        await UrlbarTestUtils.promisePopupClose(window, () => {
          EventUtils.synthesizeKey("KEY_ArrowDown");
          result = gURLBar.view.selectedResult;
          element = gURLBar.view.selectedElement;
          EventUtils.synthesizeKey("KEY_Enter");
        });
        return { result, element };
      },
      expectedEndDetails: {
        selIndex: 0,
        selType: "history",
        provider: "",
        searchSource: "urlbar",
        isSessionOngoing: false,
      },
    });
  });
});

add_task(async function privateWindow_abandonment() {
  let win = await BrowserTestUtils.openNewBrowserWindow({ private: true });
  await doTest({
    win,
    expectedEndState: "abandonment",
    expectedIsPrivate: true,
    endEngagement: async () => {
      await UrlbarTestUtils.promisePopupClose(win, () => win.gURLBar.blur());
    },
  });
  await BrowserTestUtils.closeWindow(win);
});

add_task(async function privateWindow_engagement() {
  let win = await BrowserTestUtils.openNewBrowserWindow({ private: true });
  await doTest({
    win,
    expectedEndState: "engagement",
    expectedIsPrivate: true,
    endEngagement: async () => {
      let result, element;
      await UrlbarTestUtils.promisePopupClose(win, () => {
        EventUtils.synthesizeKey("KEY_ArrowDown", {}, win);
        result = win.gURLBar.view.selectedResult;
        element = win.gURLBar.view.selectedElement;
        EventUtils.synthesizeKey("KEY_Enter", {}, win);
      });
      return { result, element };
    },
    expectedEndDetails: {
      selIndex: 0,
      selType: "history",
      provider: "",
      searchSource: "urlbar",
      isSessionOngoing: false,
    },
  });
  await BrowserTestUtils.closeWindow(win);
});

/**
 * Performs an engagement test.
 *
 * @param {object} options
 *   Options object.
 * @param {string} options.expectedEndState
 *   The expected state at the end of the engagement.
 * @param {Function} options.endEngagement
 *   A function that should end the engagement. If the expected end state is
 *   "engagement", the function should return `{ result, element }` with the
 *   expected engaged result and element.
 * @param {window} [options.win]
 *   The window to perform the test in.
 * @param {boolean} [options.expectedIsPrivate]
 *   Whether the engagement and query context are expected to be private.
 * @param {object} [options.expectedEndDetails]
 *   The expected `details` at the end of the engagement.  `searchString` is
 *   automatically included since it's always present.  If `provider` is
 *   expected, then include it and set it to any value; this function will
 *   replace it with the name of the test provider.
 */
async function doTest({
  expectedEndState,
  endEngagement,
  win = window,
  expectedIsPrivate = false,
  expectedEndDetails = {},
}) {
  let provider = new TestProvider();
  UrlbarProvidersManager.registerProvider(provider);

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window: win,
    value: "test",
    fireInputEvent: true,
  });

  let endPromise = provider.promiseEngagement();
  let { result, element } = (await endEngagement()) ?? {};

  let [state, queryContext, controller, details] = await endPromise;

  Assert.ok(
    ["engagement", "abandonment"].includes(state),
    "State should be either 'engagement' or 'abandonment'"
  );
  Assert.equal(controller.input.isPrivate, expectedIsPrivate, "End isPrivate");
  Assert.equal(state, expectedEndState, "End state");
  Assert.ok(queryContext, "End queryContext");
  Assert.equal(
    queryContext.isPrivate,
    expectedIsPrivate,
    "End queryContext.isPrivate"
  );

  let detailsDefaults = {
    searchString: "test",
    searchSource: "urlbar",
    provider: undefined,
    selIndex: -1,
  };
  if ("provider" in expectedEndDetails) {
    detailsDefaults.provider = provider.name;
    delete expectedEndDetails.provider;
  }

  if (expectedEndState == "engagement") {
    Assert.ok(
      result,
      "endEngagement() should have returned the expected engaged result"
    );
    Assert.ok(
      element,
      "endEngagement() should have returned the expected engaged element"
    );
    expectedEndDetails.result = result;
    expectedEndDetails.element = element;

    Assert.deepEqual(
      details,
      Object.assign(detailsDefaults, expectedEndDetails),
      "End details"
    );
  }

  UrlbarProvidersManager.unregisterProvider(provider);
}

/**
 * Test provider that resolves promises when onEngagement is called.
 */
class TestProvider extends UrlbarTestUtils.TestProvider {
  _resolves = [];

  constructor() {
    super({
      priority: Infinity,
      results: [
        new UrlbarResult(
          UrlbarUtils.RESULT_TYPE.URL,
          UrlbarUtils.RESULT_SOURCE.HISTORY,
          { url: "http://example.com/" }
        ),
      ],
    });
  }

  onEngagement(queryContext, controller, details) {
    let resolve = this._resolves.shift();
    if (resolve) {
      resolve(["engagement", queryContext, controller, details]);
    }
  }

  onAbandonment(queryContext, controller) {
    let resolve = this._resolves.shift();
    if (resolve) {
      resolve(["abandonment", queryContext, controller, undefined]);
    }
  }

  promiseEngagement() {
    return new Promise(resolve => this._resolves.push(resolve));
  }
}
