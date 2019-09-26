/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that displaying results with resultSpan > 1 limits other results in
// the view.
add_task(async function oneTip() {
  let tipMatches = Array.from(matches);
  for (let i = 2; i < 15; i++) {
    tipMatches.push(
      new UrlbarResult(
        UrlbarUtils.RESULT_TYPE.URL,
        UrlbarUtils.RESULT_SOURCE.HISTORY,
        { url: `http://mozilla.org/${i}` }
      )
    );
  }

  let provider = new TipTestProvider(tipMatches);
  UrlbarProvidersManager.registerProvider(provider);

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    value: "test",
    window,
    waitForFocus: SimpleTest.waitForFocus,
  });
  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    8,
    "There should be 8 results in the view."
  );

  UrlbarProvidersManager.unregisterProvider(provider);
  gURLBar.view.close();
});

add_task(async function threeTips() {
  let tipMatches = Array.from(matches);
  for (let i = 1; i < 3; i++) {
    tipMatches.push(
      new UrlbarResult(
        UrlbarUtils.RESULT_TYPE.TIP,
        UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        {
          text: "This is a test intervention.",
          buttonText: "Done",
          data: "test",
          helpUrl: `about:about#${i}`,
        }
      )
    );
  }
  for (let i = 2; i < 15; i++) {
    tipMatches.push(
      new UrlbarResult(
        UrlbarUtils.RESULT_TYPE.URL,
        UrlbarUtils.RESULT_SOURCE.HISTORY,
        { url: `http://mozilla.org/${i}` }
      )
    );
  }

  let provider = new TipTestProvider(tipMatches);
  UrlbarProvidersManager.registerProvider(provider);

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    value: "test",
    window,
    waitForFocus: SimpleTest.waitForFocus,
  });
  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    4,
    "There should be 4 results in the view."
  );

  UrlbarProvidersManager.unregisterProvider(provider);
  gURLBar.view.close();
});

/**
 * A test provider.
 */
class TipTestProvider extends UrlbarProvider {
  constructor(matches) {
    super();
    this._matches = matches;
  }
  get name() {
    return "TipTestProvider";
  }
  get type() {
    return UrlbarUtils.PROVIDER_TYPE.PROFILE;
  }
  isActive(context) {
    return true;
  }
  isRestricting(context) {
    return true;
  }
  async startQuery(context, addCallback) {
    Assert.ok(true, "Tip provider was invoked");
    this._context = context;
    for (const match of this._matches) {
      addCallback(this, match);
    }
  }
  cancelQuery(context) {}
  pickResult(result, details) {}
}

const matches = [
  new UrlbarResult(
    UrlbarUtils.RESULT_TYPE.URL,
    UrlbarUtils.RESULT_SOURCE.HISTORY,
    { url: "http://mozilla.org/1" }
  ),
  new UrlbarResult(
    UrlbarUtils.RESULT_TYPE.TIP,
    UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
    {
      text: "This is a test intervention.",
      buttonText: "Done",
      data: "test",
      helpUrl: "about:about",
    }
  ),
];
