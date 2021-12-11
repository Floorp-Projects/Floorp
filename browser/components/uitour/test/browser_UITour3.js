"use strict";

const { CustomizableUITestUtils } = ChromeUtils.import(
  "resource://testing-common/CustomizableUITestUtils.jsm"
);
let gCUITestUtils = new CustomizableUITestUtils(window);

var gTestTab;
var gContentAPI;

requestLongerTimeout(2);

add_task(setup_UITourTest);

add_UITour_task(async function test_info_icon() {
  let popup = document.getElementById("UITourTooltip");
  let title = document.getElementById("UITourTooltipTitle");
  let desc = document.getElementById("UITourTooltipDescription");
  let icon = document.getElementById("UITourTooltipIcon");
  let buttons = document.getElementById("UITourTooltipButtons");

  // Disable the animation to prevent the mouse clicks from hitting the main
  // window during the transition instead of the buttons in the popup.
  popup.setAttribute("animate", "false");

  await showInfoPromise("urlbar", "a title", "some text", "image.png");

  is(title.textContent, "a title", "Popup should have correct title");
  is(
    desc.textContent,
    "some text",
    "Popup should have correct description text"
  );

  let imageURL = getRootDirectory(gTestPath) + "image.png";
  imageURL = imageURL.replace(
    "chrome://mochitests/content/",
    "https://example.org/"
  );
  is(icon.src, imageURL, "Popup should have correct icon shown");

  is(buttons.hasChildNodes(), false, "Popup should have no buttons");
});

add_UITour_task(async function test_info_buttons_1() {
  let popup = document.getElementById("UITourTooltip");
  let title = document.getElementById("UITourTooltipTitle");
  let desc = document.getElementById("UITourTooltipDescription");
  let icon = document.getElementById("UITourTooltipIcon");

  await showInfoPromise(
    "urlbar",
    "another title",
    "moar text",
    "./image.png",
    "makeButtons"
  );

  is(title.textContent, "another title", "Popup should have correct title");
  is(
    desc.textContent,
    "moar text",
    "Popup should have correct description text"
  );

  let imageURL = getRootDirectory(gTestPath) + "image.png";
  imageURL = imageURL.replace(
    "chrome://mochitests/content/",
    "https://example.org/"
  );
  is(icon.src, imageURL, "Popup should have correct icon shown");

  let buttons = document.getElementById("UITourTooltipButtons");
  is(buttons.childElementCount, 4, "Popup should have four buttons");

  is(buttons.children[0].nodeName, "label", "Text label should be a <label>");
  is(
    buttons.children[0].getAttribute("value"),
    "Regular text",
    "Text label should have correct value"
  );
  is(
    buttons.children[0].getAttribute("image"),
    "",
    "Text should have no image"
  );
  is(buttons.children[0].className, "", "Text should have no class");

  is(buttons.children[1].nodeName, "button", "Link should be a <button>");
  is(
    buttons.children[1].getAttribute("label"),
    "Link",
    "Link should have correct label"
  );
  is(
    buttons.children[1].getAttribute("image"),
    "",
    "Link should have no image"
  );
  is(buttons.children[1].className, "button-link", "Check link class");

  is(buttons.children[2].nodeName, "button", "Button 1 should be a <button>");
  is(
    buttons.children[2].getAttribute("label"),
    "Button 1",
    "First button should have correct label"
  );
  is(
    buttons.children[2].getAttribute("image"),
    "",
    "First button should have no image"
  );
  is(buttons.children[2].className, "", "Button 1 should have no class");

  is(buttons.children[3].nodeName, "button", "Button 2 should be a <button>");
  is(
    buttons.children[3].getAttribute("label"),
    "Button 2",
    "Second button should have correct label"
  );
  is(
    buttons.children[3].getAttribute("image"),
    imageURL,
    "Second button should have correct image"
  );
  is(buttons.children[3].className, "button-primary", "Check button 2 class");

  let promiseHidden = promisePanelElementHidden(window, popup);
  EventUtils.synthesizeMouseAtCenter(buttons.children[2], {}, window);
  await promiseHidden;

  ok(true, "Popup should close automatically");

  let returnValue = await waitForCallbackResultPromise();
  is(returnValue.result, "button1", "Correct callback should have been called");
});

add_UITour_task(async function test_info_buttons_2() {
  let popup = document.getElementById("UITourTooltip");
  let title = document.getElementById("UITourTooltipTitle");
  let desc = document.getElementById("UITourTooltipDescription");
  let icon = document.getElementById("UITourTooltipIcon");

  await showInfoPromise(
    "urlbar",
    "another title",
    "moar text",
    "./image.png",
    "makeButtons"
  );

  is(title.textContent, "another title", "Popup should have correct title");
  is(
    desc.textContent,
    "moar text",
    "Popup should have correct description text"
  );

  let imageURL = getRootDirectory(gTestPath) + "image.png";
  imageURL = imageURL.replace(
    "chrome://mochitests/content/",
    "https://example.org/"
  );
  is(icon.src, imageURL, "Popup should have correct icon shown");

  let buttons = document.getElementById("UITourTooltipButtons");
  is(buttons.childElementCount, 4, "Popup should have four buttons");

  is(
    buttons.children[1].getAttribute("label"),
    "Link",
    "Link should have correct label"
  );
  is(
    buttons.children[1].getAttribute("image"),
    "",
    "Link should have no image"
  );
  ok(
    buttons.children[1].classList.contains("button-link"),
    "Link should have button-link class"
  );

  is(
    buttons.children[2].getAttribute("label"),
    "Button 1",
    "First button should have correct label"
  );
  is(
    buttons.children[2].getAttribute("image"),
    "",
    "First button should have no image"
  );

  is(
    buttons.children[3].getAttribute("label"),
    "Button 2",
    "Second button should have correct label"
  );
  is(
    buttons.children[3].getAttribute("image"),
    imageURL,
    "Second button should have correct image"
  );

  let promiseHidden = promisePanelElementHidden(window, popup);
  EventUtils.synthesizeMouseAtCenter(buttons.children[3], {}, window);
  await promiseHidden;

  ok(true, "Popup should close automatically");

  let returnValue = await waitForCallbackResultPromise();

  is(returnValue.result, "button2", "Correct callback should have been called");
});

add_UITour_task(async function test_info_close_button() {
  let closeButton = document.getElementById("UITourTooltipClose");

  await showInfoPromise(
    "urlbar",
    "Close me",
    "X marks the spot",
    null,
    null,
    "makeInfoOptions"
  );

  EventUtils.synthesizeMouseAtCenter(closeButton, {}, window);

  let returnValue = await waitForCallbackResultPromise();

  is(returnValue.result, "closeButton", "Close button callback called");
});

add_UITour_task(async function test_info_target_callback() {
  let popup = document.getElementById("UITourTooltip");

  await showInfoPromise(
    "appMenu",
    "I want to know when the target is clicked",
    "*click*",
    null,
    null,
    "makeInfoOptions"
  );

  await gCUITestUtils.openMainMenu();

  let returnValue = await waitForCallbackResultPromise();

  is(returnValue.result, "target", "target callback called");
  is(
    returnValue.data.target,
    "appMenu",
    "target callback was from the appMenu"
  );
  is(
    returnValue.data.type,
    "popupshown",
    "target callback was from the mousedown"
  );

  // Cleanup.
  await hideInfoPromise();

  popup.removeAttribute("animate");
});

add_UITour_task(async function test_getConfiguration_selectedSearchEngine() {
  let engine = await Services.search.getDefault();
  let data = await getConfigurationPromise("selectedSearchEngine");
  is(
    data.searchEngineIdentifier,
    engine.identifier,
    "Correct engine identifier"
  );
});

add_UITour_task(async function test_setSearchTerm() {
  // Place the search bar in the navigation toolbar temporarily.
  await SpecialPowers.pushPrefEnv({
    set: [["browser.search.widget.inNavBar", true]],
  });

  const TERM = "UITour Search Term";
  await gContentAPI.setSearchTerm(TERM);

  let searchbar = document.getElementById("searchbar");
  // The UITour gets to the searchbar element through a promise, so the value setting
  // only happens after a tick.
  await waitForConditionPromise(
    () => searchbar.value == TERM,
    "Correct term set"
  );

  await SpecialPowers.popPrefEnv();
});

add_UITour_task(async function test_clearSearchTerm() {
  // Place the search bar in the navigation toolbar temporarily.
  await SpecialPowers.pushPrefEnv({
    set: [["browser.search.widget.inNavBar", true]],
  });

  await gContentAPI.setSearchTerm("");

  let searchbar = document.getElementById("searchbar");
  // The UITour gets to the searchbar element through a promise, so the value setting
  // only happens after a tick.
  await waitForConditionPromise(
    () => searchbar.value == "",
    "Search term cleared"
  );

  await SpecialPowers.popPrefEnv();
});
