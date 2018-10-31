/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function synthesizeMouseOver(element) {
  window.windowUtils.disableNonTestMouseEvents(true);

  EventUtils.synthesizeMouse(element, 1, 1, {type: "mouseover"});
  EventUtils.synthesizeMouse(element, 2, 2, {type: "mousemove"});
  EventUtils.synthesizeMouse(element, 3, 3, {type: "mousemove"});
  EventUtils.synthesizeMouse(element, 4, 4, {type: "mousemove"});
}

function synthesizeMouseOut(element) {
  EventUtils.synthesizeMouse(element, 0, 0, {type: "mouseout"});
  EventUtils.synthesizeMouseAtCenter(document.documentElement, {type: "mousemove"});
  EventUtils.synthesizeMouseAtCenter(document.documentElement, {type: "mousemove"});
  EventUtils.synthesizeMouseAtCenter(document.documentElement, {type: "mousemove"});

  window.windowUtils.disableNonTestMouseEvents(false);
}

async function expectTooltip(text) {
  ok(gURLBar.hasAttribute("textoverflow"), "Urlbar is overflowing");

  let tooltip = document.getElementById("aHTMLTooltip");
  let element = gURLBar.inputField;

  let popupShownPromise = BrowserTestUtils.waitForEvent(tooltip, "popupshown");
  synthesizeMouseOver(element);
  await popupShownPromise;

  is(element.getAttribute("title"), text, "title attribute has expected text");
  is(tooltip.textContent, text, "tooltip shows expected text");

  synthesizeMouseOut(element);
}

async function expectNoTooltip() {
  ok(!gURLBar.hasAttribute("textoverflow"), "Urlbar isn't overflowing");

  let element = gURLBar.inputField;
  synthesizeMouseOver(element);

  is(element.getAttribute("title"), null, "title attribute shouldn't be set");

  synthesizeMouseOut(element);
}

add_task(async function() {
  gURLBar.value = "short string";
  await expectNoTooltip();

  let longURL = "http://longurl.com/" + "foobar/".repeat(30);
  let overflowPromise = BrowserTestUtils.waitForEvent(gURLBar.inputField, "overflow");
  gURLBar.value = longURL;
  await overflowPromise;
  is(gURLBar.inputField.value, longURL.replace(/^http:\/\//, ""), "Urlbar value has http:// stripped");
  await expectTooltip(longURL);
});
