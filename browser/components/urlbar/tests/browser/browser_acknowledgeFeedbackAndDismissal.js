/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests feedback and dismissal acknowledgments in the view.
 */

"use strict";

// See the comment in the setup task for the expected index of the main test
// result.
const RESULT_INDEX = 2;

// The command that dismisses a single result.
const DISMISS_ONE_COMMAND = "dismiss-one";

// The command that dismisses all results of a particular type.
const DISMISS_ALL_COMMAND = "dismiss-all";

// The name of this command must be one that's recognized as not ending the
// urlbar session. See `isSessionOngoing` comments for details.
const FEEDBACK_COMMAND = "show_less_frequently";

let gTestProvider;

add_setup(async function () {
  // This test expects the following results in the following order:
  //
  // 1. The heuristic
  // 2. A history result
  // 3. A result from our test provider. This will be the main result we'll use
  //    during this test.
  // 4. Another history result
  //
  // This ensures a couple things:
  //
  // * The main test result has rows above and below it. Feedback and dismissal
  //   acknowledgments in the main result row should not affect adjacent rows,
  //   except that when the dismissal acknowledgment itself is dismissed, it
  //   should be replaced by the row below it.
  // * The main result does not have a row label (a.k.a. group label). There's a
  //   separate task that specifically checks the row label, and that way this
  //   test covers both cases, where the row does and does not have a row label.
  gTestProvider = new TestProvider({
    results: [
      Object.assign(
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
        // This ensures the result is sandwiched between the two history results
        // in the Firefox Suggest group.
        { suggestedIndex: 1, isSuggestedIndexRelativeToGroup: true }
      ),
    ],
  });

  gTestProvider.commandCount = {};
  UrlbarProvidersManager.registerProvider(gTestProvider);

  await PlacesUtils.history.clear();
  await PlacesUtils.bookmarks.eraseEverything();
  await UrlbarTestUtils.formHistory.clear();

  // Add visits for the two history results.
  await PlacesTestUtils.addVisits([
    "https://example.com/aaa",
    "https://example.com/bbb",
  ]);

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
  await doDismissTest({
    command: DISMISS_ONE_COMMAND,
    shouldBeSelected: false,
  });
});

// Tests dismissal acknowledgment when the dismissed row is selected.
add_task(async function acknowledgeDismissal_rowSelected() {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "test",
  });

  // Select the row.
  gURLBar.view.selectedRowIndex = RESULT_INDEX;

  await doDismissTest({
    command: DISMISS_ONE_COMMAND,
    shouldBeSelected: true,
  });
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

  let details = await UrlbarTestUtils.getDetailsOfResultAt(
    window,
    RESULT_INDEX
  );

  // Click the feedback command.
  await UrlbarTestUtils.openResultMenuAndClickItem(window, FEEDBACK_COMMAND, {
    resultIndex: RESULT_INDEX,
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
  await doDismissTest({
    command: DISMISS_ONE_COMMAND,
    shouldBeSelected: true,
  });
});

// Tests dismissal of all results of a particular type.
add_task(async function acknowledgeDismissal_all() {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "test",
  });
  await doDismissTest({
    command: DISMISS_ALL_COMMAND,
    shouldBeSelected: false,
  });
});

// When a row with a row label (a.k.a. group label) is dismissed, the dismissal
// acknowledgment tip should retain the label. When the tip is then dismissed,
// the row that replaces it should also retain the label.
add_task(async function acknowledgeDismissal_rowLabel() {
  // Show the result as the first row in the Firefox Suggest section so that it
  // has the "Firefox Suggest" group label.
  let { suggestedIndex } = gTestProvider.results[0];
  gTestProvider.results[0].suggestedIndex = 0;

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "test",
  });
  await doDismissTest({
    resultIndex: 1,
    command: DISMISS_ALL_COMMAND,
    shouldBeSelected: false,
    expectedLabel: "Firefox Suggest",
  });

  gTestProvider.results[0].suggestedIndex = suggestedIndex;
});

/**
 * Does a dismissal test:
 *
 * 1. Clicks a dismiss command in the test result
 * 2. Verifies a dismissal acknowledgment tip replaces the result
 * 3. Clicks the "Got it" button in the tip
 * 4. Verifies the tip is dismissed
 *
 * @param {object} options
 *   Options object
 * @param {string} options.command
 *   One of: DISMISS_ONE_COMMAND, DISMISS_ALL_COMMAND
 * @param {boolean} options.shouldBeSelected
 *   True if the test result is expected to be selected initially. If true, this
 *   function verifies the "Got it" button in the dismissal acknowledgment tip
 *   also becomes selected.
 * @param {number} options.resultIndex
 *   The index of the test result, if known beforehand. Leave -1 to find it
 *   automatically.
 * @param {string} options.expectedLabel
 *   The row label (a.k.a. group label) the row is expected to have. This should
 *   be the expected translated en-US string, not an l10n object. If null, the
 *   row is expected not to have a row label at all.
 */
async function doDismissTest({
  command,
  shouldBeSelected,
  resultIndex = 2,
  expectedLabel = null,
}) {
  let details = await UrlbarTestUtils.getDetailsOfResultAt(window, resultIndex);
  Assert.equal(
    details.result.providerName,
    gTestProvider.name,
    "The test result should be at the expected index"
  );
  if (details.result.providerName != gTestProvider.name) {
    return;
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

  info("Checking the row label on the original row");
  await checkRowLabel(resultIndex, expectedLabel);

  let resultCount = UrlbarTestUtils.getResultCount(window);

  // Click the command.
  await UrlbarTestUtils.openResultMenuAndClickItem(window, command, {
    resultIndex,
    openByMouse: true,
  });

  Assert.equal(
    gTestProvider.commandCount[command],
    1,
    "One dismissal should have happened"
  );
  gTestProvider.commandCount[command] = 0;

  // The row should be a tip now.
  Assert.ok(gURLBar.view.isOpen, "The view should remain open after dismissal");
  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    resultCount,
    "The result count should not haved changed after dismissal"
  );

  details = await UrlbarTestUtils.getDetailsOfResultAt(window, resultIndex);
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
  Assert.equal(
    details.displayed.title,
    command == DISMISS_ONE_COMMAND
      ? "Thanks for your feedback. You won’t see this suggestion again."
      : "Thanks for your feedback. You won’t see these suggestions anymore.",
    "Tip text should be correct for the dismiss type"
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

  info("Checking the row label on the dismissal acknowledgment tip");
  await checkRowLabel(resultIndex, expectedLabel);

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

  info(
    "Checking the row label on the row that replaced the dismissal acknowledgment tip"
  );
  await checkRowLabel(resultIndex, expectedLabel);

  await UrlbarTestUtils.promisePopupClose(window);
}

/**
 * A provider that acknowledges feedback and dismissals.
 */
class TestProvider extends UrlbarTestUtils.TestProvider {
  getResultCommands(_result) {
    // The l10n values aren't important.
    return [
      {
        name: FEEDBACK_COMMAND,
        l10n: {
          id: "firefox-suggest-weather-command-inaccurate-location",
        },
      },
      {
        name: DISMISS_ONE_COMMAND,
        l10n: {
          id: "firefox-suggest-weather-command-not-interested",
        },
      },
      {
        name: DISMISS_ALL_COMMAND,
        l10n: {
          id: "firefox-suggest-weather-command-not-interested",
        },
      },
    ];
  }

  onEngagement(queryContext, controller, details) {
    if (details.result?.providerName == this.name) {
      let { selType } = details;

      info(`onEngagement called, selType=` + selType);

      if (!this.commandCount.hasOwnProperty(selType)) {
        this.commandCount[selType] = 0;
      }
      this.commandCount[selType]++;

      switch (selType) {
        case FEEDBACK_COMMAND:
          controller.view.acknowledgeFeedback(details.result);
          break;
        case DISMISS_ONE_COMMAND:
          details.result.acknowledgeDismissalL10n = {
            id: "firefox-suggest-dismissal-acknowledgment-one",
          };
          controller.removeResult(details.result);
          break;
        case DISMISS_ALL_COMMAND:
          details.result.acknowledgeDismissalL10n = {
            id: "firefox-suggest-dismissal-acknowledgment-all",
          };
          controller.removeResult(details.result);
          break;
      }
    }
  }
}

async function checkRowLabel(resultIndex, expectedLabel) {
  let details = await UrlbarTestUtils.getDetailsOfResultAt(window, resultIndex);
  let { row } = details.element;
  let before = getComputedStyle(row, "::before");

  if (expectedLabel) {
    Assert.equal(
      before.content,
      "attr(label)",
      "::before content should use the row label"
    );
    Assert.equal(
      row.getAttribute("label"),
      expectedLabel,
      "Row should have the expected label attribute"
    );
  } else {
    Assert.equal(before.content, "none", "::before content should be 'none'");
    Assert.ok(
      !row.hasAttribute("label"),
      "Row should not have a label attribute"
    );
  }
}
