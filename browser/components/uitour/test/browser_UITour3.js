/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let gTestTab;
let gContentAPI;
let gContentWindow;

requestLongerTimeout(2);

function test() {
  UITourTest();
}

let tests = [
  taskify(function* test_info_icon() {
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
    imageURL = imageURL.replace("chrome://mochitests/content/", "https://example.com/");
    is(icon.src, imageURL,  "Popup should have correct icon shown");

    is(buttons.hasChildNodes(), false, "Popup should have no buttons");
  }),

  taskify(function* test_info_buttons_1() {
    let popup = document.getElementById("UITourTooltip");
    let title = document.getElementById("UITourTooltipTitle");
    let desc = document.getElementById("UITourTooltipDescription");
    let icon = document.getElementById("UITourTooltipIcon");

    let buttons = gContentWindow.makeButtons();

    yield showInfoPromise("urlbar", "another title", "moar text", "./image.png", buttons);

    is(title.textContent, "another title", "Popup should have correct title");
    is(desc.textContent, "moar text", "Popup should have correct description text");

    let imageURL = getRootDirectory(gTestPath) + "image.png";
    imageURL = imageURL.replace("chrome://mochitests/content/", "https://example.com/");
    is(icon.src, imageURL,  "Popup should have correct icon shown");

    buttons = document.getElementById("UITourTooltipButtons");
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

    yield waitForCallbackResultPromise();

    is(gContentWindow.callbackResult, "button1", "Correct callback should have been called");
  }),
  taskify(function* test_info_buttons_2() {
    let popup = document.getElementById("UITourTooltip");
    let title = document.getElementById("UITourTooltipTitle");
    let desc = document.getElementById("UITourTooltipDescription");
    let icon = document.getElementById("UITourTooltipIcon");

    let buttons = gContentWindow.makeButtons();

    yield showInfoPromise("urlbar", "another title", "moar text", "./image.png", buttons);

    is(title.textContent, "another title", "Popup should have correct title");
    is(desc.textContent, "moar text", "Popup should have correct description text");

    let imageURL = getRootDirectory(gTestPath) + "image.png";
    imageURL = imageURL.replace("chrome://mochitests/content/", "https://example.com/");
    is(icon.src, imageURL,  "Popup should have correct icon shown");

    buttons = document.getElementById("UITourTooltipButtons");
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

    yield waitForCallbackResultPromise();

    is(gContentWindow.callbackResult, "button2", "Correct callback should have been called");
  }),

  taskify(function* test_info_close_button() {
    let popup = document.getElementById("UITourTooltip");
    let closeButton = document.getElementById("UITourTooltipClose");
    let infoOptions = gContentWindow.makeInfoOptions();

    yield showInfoPromise("urlbar", "Close me", "X marks the spot", null, null, infoOptions);

    EventUtils.synthesizeMouseAtCenter(closeButton, {}, window);

    yield waitForCallbackResultPromise();

    is(gContentWindow.callbackResult, "closeButton", "Close button callback called");
  }),

  taskify(function* test_info_target_callback() {
    let popup = document.getElementById("UITourTooltip");
    let infoOptions = gContentWindow.makeInfoOptions();

    yield showInfoPromise("appMenu", "I want to know when the target is clicked", "*click*", null, null, infoOptions);

    yield PanelUI.show();

    yield waitForCallbackResultPromise();

    is(gContentWindow.callbackResult, "target", "target callback called");
    is(gContentWindow.callbackData.target, "appMenu", "target callback was from the appMenu");
    is(gContentWindow.callbackData.type, "popupshown", "target callback was from the mousedown");

    // Cleanup.
    yield hideInfoPromise();

    popup.removeAttribute("animate");
  }),

  function test_getConfiguration_selectedSearchEngine(done) {
    Services.search.init(rv => {
      ok(Components.isSuccessCode(rv), "Search service initialized");
      let engine = Services.search.defaultEngine;
      gContentAPI.getConfiguration("selectedSearchEngine", (data) => {
        is(data.searchEngineIdentifier, engine.identifier, "Correct engine identifier");
        done();
      });
    });
  },

  function test_setSearchTerm(done) {
    const TERM = "UITour Search Term";
    gContentAPI.setSearchTerm(TERM);

    let searchbar = document.getElementById("searchbar");
    // The UITour gets to the searchbar element through a promise, so the value setting
    // only happens after a tick.
    waitForCondition(() => searchbar.value == TERM, done, "Correct term set");
  },

  function test_clearSearchTerm(done) {
    gContentAPI.setSearchTerm("");

    let searchbar = document.getElementById("searchbar");
    // The UITour gets to the searchbar element through a promise, so the value setting
    // only happens after a tick.
    waitForCondition(() => searchbar.value == "", done, "Search term cleared");
  },
];
