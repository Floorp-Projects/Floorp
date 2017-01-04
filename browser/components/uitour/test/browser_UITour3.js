"use strict";

var gTestTab;
var gContentAPI;
var gContentWindow;

requestLongerTimeout(2);

add_task(setup_UITourTest);

add_UITour_task(function* test_info_icon() {
  let popup = document.getElementById("UITourTooltip");
  let title = document.getElementById("UITourTooltipTitle");
  let desc = document.getElementById("UITourTooltipDescription");
  let icon = document.getElementById("UITourTooltipIcon");
  let buttons = document.getElementById("UITourTooltipButtons");

  // Disable the animation to prevent the mouse clicks from hitting the main
  // window during the transition instead of the buttons in the popup.
  popup.setAttribute("animate", "false");

  yield showInfoPromise("urlbar", "a title", "some text", "image.png");

  is(title.textContent, "a title", "Popup should have correct title");
  is(desc.textContent, "some text", "Popup should have correct description text");

  let imageURL = getRootDirectory(gTestPath) + "image.png";
  imageURL = imageURL.replace("chrome://mochitests/content/", "https://example.org/");
  is(icon.src, imageURL, "Popup should have correct icon shown");

  is(buttons.hasChildNodes(), false, "Popup should have no buttons");
}),

add_UITour_task(function* test_info_buttons_1() {
  let popup = document.getElementById("UITourTooltip");
  let title = document.getElementById("UITourTooltipTitle");
  let desc = document.getElementById("UITourTooltipDescription");
  let icon = document.getElementById("UITourTooltipIcon");

  yield showInfoPromise("urlbar", "another title", "moar text", "./image.png", "makeButtons");

  is(title.textContent, "another title", "Popup should have correct title");
  is(desc.textContent, "moar text", "Popup should have correct description text");

  let imageURL = getRootDirectory(gTestPath) + "image.png";
  imageURL = imageURL.replace("chrome://mochitests/content/", "https://example.org/");
  is(icon.src, imageURL, "Popup should have correct icon shown");

  let buttons = document.getElementById("UITourTooltipButtons");
  is(buttons.childElementCount, 4, "Popup should have four buttons");

  is(buttons.childNodes[0].nodeName, "label", "Text label should be a <label>");
  is(buttons.childNodes[0].getAttribute("value"), "Regular text", "Text label should have correct value");
  is(buttons.childNodes[0].getAttribute("image"), "", "Text should have no image");
  is(buttons.childNodes[0].className, "", "Text should have no class");

  is(buttons.childNodes[1].nodeName, "button", "Link should be a <button>");
  is(buttons.childNodes[1].getAttribute("label"), "Link", "Link should have correct label");
  is(buttons.childNodes[1].getAttribute("image"), "", "Link should have no image");
  is(buttons.childNodes[1].className, "button-link", "Check link class");

  is(buttons.childNodes[2].nodeName, "button", "Button 1 should be a <button>");
  is(buttons.childNodes[2].getAttribute("label"), "Button 1", "First button should have correct label");
  is(buttons.childNodes[2].getAttribute("image"), "", "First button should have no image");
  is(buttons.childNodes[2].className, "", "Button 1 should have no class");

  is(buttons.childNodes[3].nodeName, "button", "Button 2 should be a <button>");
  is(buttons.childNodes[3].getAttribute("label"), "Button 2", "Second button should have correct label");
  is(buttons.childNodes[3].getAttribute("image"), imageURL, "Second button should have correct image");
  is(buttons.childNodes[3].className, "button-primary", "Check button 2 class");

  let promiseHidden = promisePanelElementHidden(window, popup);
  EventUtils.synthesizeMouseAtCenter(buttons.childNodes[2], {}, window);
  yield promiseHidden;

  ok(true, "Popup should close automatically");

  let returnValue = yield waitForCallbackResultPromise();
  is(returnValue.result, "button1", "Correct callback should have been called");
});

add_UITour_task(function* test_info_buttons_2() {
  let popup = document.getElementById("UITourTooltip");
  let title = document.getElementById("UITourTooltipTitle");
  let desc = document.getElementById("UITourTooltipDescription");
  let icon = document.getElementById("UITourTooltipIcon");

  yield showInfoPromise("urlbar", "another title", "moar text", "./image.png", "makeButtons");

  is(title.textContent, "another title", "Popup should have correct title");
  is(desc.textContent, "moar text", "Popup should have correct description text");

  let imageURL = getRootDirectory(gTestPath) + "image.png";
  imageURL = imageURL.replace("chrome://mochitests/content/", "https://example.org/");
  is(icon.src, imageURL, "Popup should have correct icon shown");

  let buttons = document.getElementById("UITourTooltipButtons");
  is(buttons.childElementCount, 4, "Popup should have four buttons");

  is(buttons.childNodes[1].getAttribute("label"), "Link", "Link should have correct label");
  is(buttons.childNodes[1].getAttribute("image"), "", "Link should have no image");
  ok(buttons.childNodes[1].classList.contains("button-link"), "Link should have button-link class");

  is(buttons.childNodes[2].getAttribute("label"), "Button 1", "First button should have correct label");
  is(buttons.childNodes[2].getAttribute("image"), "", "First button should have no image");

  is(buttons.childNodes[3].getAttribute("label"), "Button 2", "Second button should have correct label");
  is(buttons.childNodes[3].getAttribute("image"), imageURL, "Second button should have correct image");

  let promiseHidden = promisePanelElementHidden(window, popup);
  EventUtils.synthesizeMouseAtCenter(buttons.childNodes[3], {}, window);
  yield promiseHidden;

  ok(true, "Popup should close automatically");

  let returnValue = yield waitForCallbackResultPromise();

  is(returnValue.result, "button2", "Correct callback should have been called");
}),

add_UITour_task(function* test_info_close_button() {
  let closeButton = document.getElementById("UITourTooltipClose");

  yield showInfoPromise("urlbar", "Close me", "X marks the spot", null, null, "makeInfoOptions");

  EventUtils.synthesizeMouseAtCenter(closeButton, {}, window);

  let returnValue = yield waitForCallbackResultPromise();

  is(returnValue.result, "closeButton", "Close button callback called");
}),

add_UITour_task(function* test_info_target_callback() {
  let popup = document.getElementById("UITourTooltip");

  yield showInfoPromise("appMenu", "I want to know when the target is clicked", "*click*", null, null, "makeInfoOptions");

  yield PanelUI.show();

  let returnValue = yield waitForCallbackResultPromise();

  is(returnValue.result, "target", "target callback called");
  is(returnValue.data.target, "appMenu", "target callback was from the appMenu");
  is(returnValue.data.type, "popupshown", "target callback was from the mousedown");

  // Cleanup.
  yield hideInfoPromise();

  popup.removeAttribute("animate");
}),

add_UITour_task(function* test_getConfiguration_selectedSearchEngine() {
  yield new Promise((resolve) => {
    Services.search.init(Task.async(function*(rv) {
      ok(Components.isSuccessCode(rv), "Search service initialized");
      let engine = Services.search.defaultEngine;
      let data = yield getConfigurationPromise("selectedSearchEngine");
      is(data.searchEngineIdentifier, engine.identifier, "Correct engine identifier");
      resolve();
    }));
  });
});

add_UITour_task(function* test_setSearchTerm() {
  const TERM = "UITour Search Term";
  yield gContentAPI.setSearchTerm(TERM);

  let searchbar = document.getElementById("searchbar");
  // The UITour gets to the searchbar element through a promise, so the value setting
  // only happens after a tick.
  yield waitForConditionPromise(() => searchbar.value == TERM, "Correct term set");
});

add_UITour_task(function* test_clearSearchTerm() {
  yield gContentAPI.setSearchTerm("");

  let searchbar = document.getElementById("searchbar");
  // The UITour gets to the searchbar element through a promise, so the value setting
  // only happens after a tick.
  yield waitForConditionPromise(() => searchbar.value == "", "Search term cleared");
});
