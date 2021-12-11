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
        await UrlbarTestUtils.promisePopupClose(window, () => {
          EventUtils.synthesizeKey("KEY_ArrowDown");
          EventUtils.synthesizeKey("KEY_Enter");
        });
      },
      expectedEndDetails: {
        selIndex: 0,
        selType: "history",
        provider: "",
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
      await UrlbarTestUtils.promisePopupClose(win, () => {
        EventUtils.synthesizeKey("KEY_ArrowDown", {}, win);
        EventUtils.synthesizeKey("KEY_Enter", {}, win);
      });
    },
    expectedEndDetails: {
      selIndex: 0,
      selType: "history",
      provider: "",
    },
  });
  await BrowserTestUtils.closeWindow(win);
});

/**
 * Performs an engagement test.
 *
 * @param {string} expectedEndState
 *   The expected state at the end of the engagement.
 * @param {function} endEngagement
 *   A function that should end the engagement.
 * @param {window} [win]
 *   The window to perform the test in.
 * @param {boolean} [expectedIsPrivate]
 *   Whether the engagement and query context are expected to be private.
 * @param {object} [expectedEndDetails]
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

  let startPromise = provider.promiseEngagement();
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window: win,
    value: "test",
    fireInputEvent: true,
  });

  let [isPrivate, state, queryContext, details] = await startPromise;
  Assert.equal(isPrivate, expectedIsPrivate, "Start isPrivate");
  Assert.equal(state, "start", "Start state");

  // `queryContext` isn't always defined for `start`, and `onEngagement`
  // shouldn't rely on it being defined on start, but there's no good reason to
  // assert that it's not defined here.

  // Similarly, `details` is never defined for `start`, but there's no good
  // reason to assert that it's not defined.

  let endPromise = provider.promiseEngagement();
  await endEngagement();

  [isPrivate, state, queryContext, details] = await endPromise;
  Assert.equal(isPrivate, expectedIsPrivate, "End isPrivate");
  Assert.equal(state, expectedEndState, "End state");
  Assert.ok(queryContext, "End queryContext");
  Assert.equal(
    queryContext.isPrivate,
    expectedIsPrivate,
    "End queryContext.isPrivate"
  );

  let detailsDefaults = { searchString: "test" };
  if ("provider" in expectedEndDetails) {
    detailsDefaults.provider = provider.name;
    delete expectedEndDetails.provider;
  }
  Assert.deepEqual(
    details,
    Object.assign(detailsDefaults, expectedEndDetails),
    "End details"
  );

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

  onEngagement(...args) {
    let resolve = this._resolves.shift();
    if (resolve) {
      resolve(args);
    }
  }

  promiseEngagement() {
    return new Promise(resolve => this._resolves.push(resolve));
  }
}
