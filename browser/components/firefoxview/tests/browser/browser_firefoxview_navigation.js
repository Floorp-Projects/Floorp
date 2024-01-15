/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const URL_BASE = `${getFirefoxViewURL()}#`;

function assertCorrectPage(document, name, event) {
  is(
    document.location.hash,
    `#${name}`,
    `Navigation button for ${name} navigates to ${URL_BASE + name} on ${event}.`
  );
  is(
    document.querySelector("named-deck").selectedViewName,
    name,
    "The correct deck child is selected"
  );
}

add_task(async function test_side_component_navigation_by_click() {
  await withFirefoxView({}, async browser => {
    await SimpleTest.promiseFocus(browser);

    const { document } = browser.contentWindow;
    let win = browser.ownerGlobal;
    const categoryButtons = document.querySelectorAll("fxview-category-button");

    for (let element of categoryButtons) {
      const name = element.name;
      let buttonClicked = BrowserTestUtils.waitForEvent(
        element.buttonEl,
        "click",
        win
      );

      info(`Clicking navigation button for ${name}`);
      EventUtils.synthesizeMouseAtCenter(element.buttonEl, {}, content);
      await buttonClicked;

      assertCorrectPage(document, name, "click");
    }
  });
});

add_task(async function test_side_component_navigation_by_keyboard() {
  await withFirefoxView({}, async browser => {
    await SimpleTest.promiseFocus(browser);

    const { document } = browser.contentWindow;
    let win = browser.ownerGlobal;
    const categoryButtons = document.querySelectorAll("fxview-category-button");
    const firstButton = categoryButtons[0];

    firstButton.focus();
    is(
      document.activeElement,
      firstButton,
      "The first category button has focus"
    );

    for (let element of Array.from(categoryButtons).slice(1)) {
      const name = element.name;
      let buttonFocused = BrowserTestUtils.waitForEvent(element, "focus", win);

      info(`Focus is on ${document.activeElement.name}`);
      info(`Arrow down on navigation to ${name}`);
      EventUtils.synthesizeKey("KEY_ArrowDown", {}, win);
      await buttonFocused;

      assertCorrectPage(document, name, "key press");
    }
  });
});

add_task(async function test_direct_navigation_to_correct_category() {
  await withFirefoxView({}, async browser => {
    const { document } = browser.contentWindow;
    const categoryButtons = document.querySelectorAll("fxview-category-button");
    const namedDeck = document.querySelector("named-deck");

    for (let element of categoryButtons) {
      const name = element.name;

      info(`Navigating to ${URL_BASE + name}`);
      document.location.assign(URL_BASE + name);
      await BrowserTestUtils.waitForCondition(() => {
        return namedDeck.selectedViewName === name;
      }, "Wait for navigation to complete");

      is(
        namedDeck.selectedViewName,
        name,
        `The correct deck child for category ${name} is selected`
      );
    }
  });
});
