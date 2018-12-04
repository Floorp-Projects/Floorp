/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function synthesizeMouseOver(element) {
  let promise = BrowserTestUtils.waitForEvent(element, "mouseover");

  EventUtils.synthesizeMouse(element, 1, 1, {type: "mouseover"});
  EventUtils.synthesizeMouse(element, 2, 2, {type: "mousemove"});
  EventUtils.synthesizeMouse(element, 3, 3, {type: "mousemove"});
  EventUtils.synthesizeMouse(element, 4, 4, {type: "mousemove"});

  return promise;
}

function synthesizeMouseOut(element) {
  let promise = BrowserTestUtils.waitForEvent(element, "mouseout");

  EventUtils.synthesizeMouse(element, 0, 0, {type: "mouseout"});
  EventUtils.synthesizeMouseAtCenter(document.documentElement, {type: "mousemove"});
  EventUtils.synthesizeMouseAtCenter(document.documentElement, {type: "mousemove"});
  EventUtils.synthesizeMouseAtCenter(document.documentElement, {type: "mousemove"});

  return promise;
}

async function expectTooltip(text) {
  if (!gURLBar._overflowing && !gURLBar._inOverflow) {
    info("waiting for overflow event");
    await BrowserTestUtils.waitForEvent(gURLBar.inputField, "overflow");
  }

  let tooltip = document.getElementById("aHTMLTooltip");
  let element = gURLBar.inputField;

  let popupShownPromise = BrowserTestUtils.waitForEvent(tooltip, "popupshown");
  await synthesizeMouseOver(element);
  await popupShownPromise;

  is(element.getAttribute("title"), text, "title attribute has expected text");
  is(tooltip.textContent, text, "tooltip shows expected text");

  await synthesizeMouseOut(element);
}

async function expectNoTooltip() {
  if (gURLBar._overflowing || gURLBar._inOverflow) {
    info("waiting for underflow event");
    await BrowserTestUtils.waitForEvent(gURLBar.inputField, "underflow");
  }

  let element = gURLBar.inputField;
  await synthesizeMouseOver(element);

  is(element.getAttribute("title"), null, "title attribute shouldn't be set");

  await synthesizeMouseOut(element);
}

add_task(async function() {
  window.windowUtils.disableNonTestMouseEvents(true);
  registerCleanupFunction(() => {
    window.windowUtils.disableNonTestMouseEvents(false);
  });

  // Ensure the URL bar is neither focused nor hovered before we start.
  gBrowser.selectedBrowser.focus();
  await synthesizeMouseOut(gURLBar.inputField);

  gURLBar.value = "short string";
  await expectNoTooltip();

  let longURL = "http://longurl.com/" + "foobar/".repeat(30);
  gURLBar.value = longURL;
  is(gURLBar.inputField.value, longURL.replace(/^http:\/\//, ""), "Urlbar value has http:// stripped");
  await expectTooltip(longURL);
});
