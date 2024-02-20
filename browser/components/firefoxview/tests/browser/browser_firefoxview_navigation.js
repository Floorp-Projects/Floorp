/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const URL_BASE = `${getFirefoxViewURL()}#`;

function assertCorrectPage(document, view, event) {
  is(
    document.location.hash,
    `#${view}`,
    `Navigation button for ${view} navigates to ${URL_BASE + view} on ${event}.`
  );
  is(
    document.querySelector("named-deck").selectedViewName,
    view,
    "The correct deck child is selected"
  );
}

add_task(async function test_side_component_navigation_by_click() {
  await withFirefoxView({}, async browser => {
    await SimpleTest.promiseFocus(browser);

    const { document } = browser.contentWindow;
    let win = browser.ownerGlobal;
    const pageNavButtons = document.querySelectorAll("moz-page-nav-button");

    for (let element of pageNavButtons) {
      const view = element.view;
      let buttonClicked = BrowserTestUtils.waitForEvent(
        element.buttonEl,
        "click",
        win
      );

      info(`Clicking navigation button for ${view}`);
      EventUtils.synthesizeMouseAtCenter(element.buttonEl, {}, content);
      await buttonClicked;

      assertCorrectPage(document, view, "click");
    }
  });
});

add_task(async function test_side_component_navigation_by_keyboard() {
  await withFirefoxView({}, async browser => {
    await SimpleTest.promiseFocus(browser);

    const { document } = browser.contentWindow;
    let win = browser.ownerGlobal;
    const pageNavButtons = document.querySelectorAll("moz-page-nav-button");
    const firstButton = pageNavButtons[0].buttonEl;

    firstButton.focus();
    is(
      document.activeElement.shadowRoot.activeElement,
      firstButton,
      "The first page nav button has focus"
    );

    for (let element of Array.from(pageNavButtons).slice(1)) {
      const view = element.view;
      let buttonFocused = BrowserTestUtils.waitForEvent(element, "focus", win);

      info(`Focus is on ${document.activeElement.view}`);
      info(`Arrow down on navigation to ${view}`);
      EventUtils.synthesizeKey("KEY_ArrowDown", {}, win);
      await buttonFocused;

      assertCorrectPage(document, view, "key press");
    }
  });
});

add_task(async function test_direct_navigation_to_correct_view() {
  await withFirefoxView({}, async browser => {
    const { document } = browser.contentWindow;
    const pageNavButtons = document.querySelectorAll("moz-page-nav-button");
    const namedDeck = document.querySelector("named-deck");

    for (let element of pageNavButtons) {
      const view = element.view;

      info(`Navigating to ${URL_BASE + view}`);
      document.location.assign(URL_BASE + view);
      await BrowserTestUtils.waitForCondition(() => {
        return namedDeck.selectedViewName === view;
      }, "Wait for navigation to complete");

      is(
        namedDeck.selectedViewName,
        view,
        `The correct deck child for view ${view} is selected`
      );
    }
  });
});
