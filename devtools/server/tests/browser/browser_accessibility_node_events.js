/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Checks for the AccessibleActor events

add_task(function* () {
  let {client, walker, accessibility} =
    yield initAccessibilityFrontForUrl(MAIN_DOMAIN + "doc_accessibility.html");

  let a11yWalker = yield accessibility.getWalker(walker);
  let a11yDoc = yield a11yWalker.getDocument();
  let buttonNode = yield walker.querySelector(walker.rootNode, "#button");
  let accessibleFront = yield a11yWalker.getAccessibleFor(buttonNode);
  let sliderNode = yield walker.querySelector(walker.rootNode, "#slider");
  let accessibleSliderFront = yield a11yWalker.getAccessibleFor(sliderNode);
  let browser = gBrowser.selectedBrowser;

  checkA11yFront(accessibleFront, {
    name: "Accessible Button",
    role: "pushbutton",
    value: "",
    description: "Accessibility Test",
    help: "",
    keyboardShortcut: "",
    childCount: 1,
    domNodeType: 1
  });

  info("Name change event");
  yield emitA11yEvent(accessibleFront, "name-change",
    (name, parent) => {
      checkA11yFront(accessibleFront, { name: "Renamed" });
      checkA11yFront(parent, { }, a11yDoc);
    }, () => ContentTask.spawn(browser, null, () =>
      content.document.getElementById("button").setAttribute(
        "aria-label", "Renamed")));

  info("Description change event");
  yield emitA11yEvent(accessibleFront, "description-change",
    () => checkA11yFront(accessibleFront, { description: "" }),
    () => ContentTask.spawn(browser, null, () =>
      content.document.getElementById("button").removeAttribute("aria-describedby")));

  info("State change event");
  let states = yield accessibleFront.getState();
  let expectedStates = ["unavailable", "selectable text", "opaque"];
  SimpleTest.isDeeply(states, ["focusable", "selectable text", "opaque",
                               "enabled", "sensitive"], "States are correct");
  yield emitA11yEvent(accessibleFront, "state-change",
    newStates => SimpleTest.isDeeply(newStates, expectedStates,
                                     "States are updated"),
    () => ContentTask.spawn(browser, null, () =>
      content.document.getElementById("button").setAttribute("disabled", true)));
  states = yield accessibleFront.getState();
  SimpleTest.isDeeply(states, expectedStates, "States are updated");

  info("Attributes change event");
  let attrs = yield accessibleFront.getAttributes();
  ok(!attrs.live, "Attribute is not present");
  yield emitA11yEvent(accessibleFront, "attributes-change",
    newAttrs => is(newAttrs.live, "polite", "Attributes are updated"),
    () => ContentTask.spawn(browser, null, () =>
      content.document.getElementById("button").setAttribute("aria-live", "polite")));
  attrs = yield accessibleFront.getAttributes();
  is(attrs.live, "polite", "Attributes are updated");

  info("Value change event");
  checkA11yFront(accessibleSliderFront, { value: "5" });
  yield emitA11yEvent(accessibleSliderFront, "value-change",
    () => checkA11yFront(accessibleSliderFront, { value: "6" }),
    () => ContentTask.spawn(browser, null, () =>
      content.document.getElementById("slider").setAttribute("aria-valuenow", "6")));

  info("Reorder event");
  is(accessibleSliderFront.childCount, 1, "Slider has only 1 child");
  yield emitA11yEvent(accessibleSliderFront, "reorder",
    childCount => is(childCount, 2, "Child count is updated"),
    () => ContentTask.spawn(browser, null, () => {
      let button = content.document.createElement("button");
      button.innerText = "Slider button";
      content.document.getElementById("slider").appendChild(button);
    }));
  is(accessibleSliderFront.childCount, 2, "Child count is updated");

  let a11yShutdown = waitForA11yShutdown();
  yield client.close();
  forceCollections();
  yield a11yShutdown;
  gBrowser.removeCurrentTab();
});
