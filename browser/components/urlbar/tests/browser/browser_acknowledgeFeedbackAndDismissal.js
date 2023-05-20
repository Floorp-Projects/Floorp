/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests feedback and dismissal acknowledgments in the view.
 */

"use strict";

// The name of this command must be one that's recognized as not ending the
// urlbar session. See `isSessionOngoing` comments for details.
const FEEDBACK_COMMAND = "show_less_frequently";

let gTestProvider;

add_setup(async function () {
  gTestProvider = new TestProvider({
    results: [
      new UrlbarResult(
        UrlbarUtils.RESULT_TYPE.URL,
        UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        {
          url: "https://example.com/",
          isBlockable: true,
          blockL10n: {
            id: "urlbar-result-menu-dismiss-firefox-suggest",
          },
        }
      ),
    ],
  });

  gTestProvider.commandCount = {};
  UrlbarProvidersManager.registerProvider(gTestProvider);

  // Add a visit so that there's one result above the test result (the
  // heuristic) and one below (the visit) just to make sure removing the test
  // result doesn't mess up adjacent results.
  await PlacesUtils.history.clear();
  await PlacesUtils.bookmarks.eraseEverything();
  await UrlbarTestUtils.formHistory.clear();
  await PlacesTestUtils.addVisits("https://example.com/aaa");

  registerCleanupFunction(() => {
    UrlbarProvidersManager.unregisterProvider(gTestProvider);
  });
});

// Tests dismissal acknowledgment when the dismissed row is not selected.
add_task(async function acknowledgeDismissal_rowNotSelected() {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "test",
  });
  await doDismissTest({ shouldBeSelected: false });
});

// Tests dismissal acknowledgment when the dismissed row is selected.
add_task(async function acknowledgeDismissal_rowSelected() {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "test",
  });

  // Select the row.
  let resultIndex = await getTestResultIndex();
  while (gURLBar.view.selectedRowIndex != resultIndex) {
    this.EventUtils.synthesizeKey("KEY_ArrowDown");
  }

  await doDismissTest({ resultIndex, shouldBeSelected: true });
});

// Tests a feedback acknowledgment command immediately followed by a dismissal
// acknowledgment command. This makes sure that both feedback acknowledgment
// works and a subsequent dismissal command works while the urlbar session
// remains ongoing.
add_task(async function acknowledgeFeedbackAndDismissal() {
  // Trigger the suggestion.
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "test",
  });

  let resultIndex = await getTestResultIndex();
  let details = await UrlbarTestUtils.getDetailsOfResultAt(window, resultIndex);

  // Click the feedback command.
  await UrlbarTestUtils.openResultMenuAndClickItem(window, FEEDBACK_COMMAND, {
    resultIndex,
  });

  Assert.equal(
    gTestProvider.commandCount[FEEDBACK_COMMAND],
    1,
    "One feedback command should have happened"
  );
  gTestProvider.commandCount[FEEDBACK_COMMAND] = 0;

  Assert.ok(
    gURLBar.view.isOpen,
    "The view should remain open clicking the command"
  );
  Assert.ok(
    details.element.row.hasAttribute("feedback-acknowledgment"),
    "Row should have feedback acknowledgment after clicking command"
  );

  info("Doing dismissal");
  await doDismissTest({ resultIndex, shouldBeSelected: true });
});

/**
 * Does a dismissal test:
 *
 * 1. Clicks the dismiss command in the test result
 * 2. Verifies a dismissal acknowledgment tip replaces the result
 * 3. Clicks the "Got it" button in the tip
 * 4. Verifies the tip is dismissed
 *
 * @param {object} options
 *   Options object
 * @param {boolean} options.shouldBeSelected
 *   True if the test result is expected to be selected initially. If true, this
 *   function verifies the "Got it" button in the dismissal acknowledgment tip
 *   also becomes selected.
 * @param {number} options.resultIndex
 *   The index of the test result, if known beforehand. Leave -1 to find it
 *   automatically.
 */
async function doDismissTest({ shouldBeSelected, resultIndex = -1 }) {
  if (resultIndex < 0) {
    resultIndex = await getTestResultIndex();
  }

  let selectedElement = gURLBar.view.selectedElement;
  Assert.ok(selectedElement, "There should be an initially selected element");

  if (shouldBeSelected) {
    Assert.equal(
      gURLBar.view.selectedRowIndex,
      resultIndex,
      "The test result should be selected"
    );
  } else {
    Assert.notEqual(
      gURLBar.view.selectedRowIndex,
      resultIndex,
      "The test result should not be selected"
    );
  }

  let resultCount = UrlbarTestUtils.getResultCount(window);

  // Click the dismiss command.
  await UrlbarTestUtils.openResultMenuAndClickItem(window, "dismiss", {
    resultIndex,
    openByMouse: true,
  });

  Assert.equal(
    gTestProvider.commandCount.dismiss,
    1,
    "One dismissal should have happened"
  );
  gTestProvider.commandCount.dismiss = 0;

  // The row should be a tip now.
  Assert.ok(gURLBar.view.isOpen, "The view should remain open after dismissal");
  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    resultCount,
    "The result count should not haved changed after dismissal"
  );

  let details = await UrlbarTestUtils.getDetailsOfResultAt(window, resultIndex);
  Assert.equal(
    details.type,
    UrlbarUtils.RESULT_TYPE.TIP,
    "Row should be a tip after dismissal"
  );
  Assert.equal(
    details.result.payload.type,
    "dismissalAcknowledgment",
    "Tip type should be dismissalAcknowledgment"
  );
  Assert.ok(
    !details.element.row.hasAttribute("selected"),
    "Row should not have 'selected' attribute"
  );
  Assert.ok(
    !details.element.row._content.hasAttribute("selected"),
    "Row-inner should not have 'selected' attribute"
  );
  Assert.ok(
    !details.element.row.hasAttribute("feedback-acknowledgment"),
    "Row should not have feedback acknowledgment after dismissal"
  );

  // Get the dismissal acknowledgment's "Got it" button.
  let gotItButton = UrlbarTestUtils.getButtonForResultIndex(
    window,
    "0",
    resultIndex
  );
  Assert.ok(gotItButton, "Row should have a 'Got it' button");

  if (shouldBeSelected) {
    Assert.equal(
      gURLBar.view.selectedElement,
      gotItButton,
      "The 'Got it' button should be selected"
    );
  } else {
    Assert.notEqual(
      gURLBar.view.selectedElement,
      gotItButton,
      "The 'Got it' button should not be selected"
    );
    Assert.equal(
      gURLBar.view.selectedElement,
      selectedElement,
      "The initially selected element should remain selected"
    );
  }

  // Click it.
  EventUtils.synthesizeMouseAtCenter(gotItButton, {}, window);

  // The view should remain open and the tip row should be gone.
  Assert.ok(
    gURLBar.view.isOpen,
    "The view should remain open clicking the 'Got it' button"
  );
  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    resultCount - 1,
    "The result count should be one less after clicking 'Got it' button"
  );
  for (let i = 0; i < UrlbarTestUtils.getResultCount(window); i++) {
    details = await UrlbarTestUtils.getDetailsOfResultAt(window, i);
    Assert.ok(
      details.type != UrlbarUtils.RESULT_TYPE.TIP &&
        details.result.providerName != gTestProvider.name,
      "Tip result and test result should not be present"
    );
  }

  await UrlbarTestUtils.promisePopupClose(window);
}

/**
 * A provider that acknowledges feedback and dismissals.
 */
class TestProvider extends UrlbarTestUtils.TestProvider {
  getResultCommands(result) {
    return [
      {
        name: FEEDBACK_COMMAND,
        l10n: {
          id: "firefox-suggest-weather-command-inaccurate-location",
        },
      },
      {
        name: "dismiss",
        l10n: {
          id: "firefox-suggest-weather-command-not-interested",
        },
      },
    ];
  }

  onEngagement(isPrivate, state, queryContext, details) {
    if (details.result?.providerName == this.name) {
      let { selType } = details;

      info(`onEngagement called, selType=` + selType);

      if (!this.commandCount.hasOwnProperty(selType)) {
        this.commandCount[selType] = 0;
      }
      this.commandCount[selType]++;

      if (selType == FEEDBACK_COMMAND) {
        queryContext.view.acknowledgeFeedback(details.result);
      } else if (selType == "dismiss") {
        queryContext.view.acknowledgeDismissal(details.result);
      }
    }
  }
}

async function getTestResultIndex() {
  let index = 0;
  let resultCount = UrlbarTestUtils.getResultCount(window);
  for (; index < resultCount; index++) {
    let details = await UrlbarTestUtils.getDetailsOfResultAt(window, index);
    if (details.result.providerName == gTestProvider.name) {
      break;
    }
  }
  Assert.less(index, resultCount, "The test result should be present");
  return index;
}
