/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Checks for the AccessibleActor events

add_task(async function () {
  const { target, walker, a11yWalker, parentAccessibility } =
    await initAccessibilityFrontsForUrl(MAIN_DOMAIN + "doc_accessibility.html");
  const modifiers =
    Services.appinfo.OS === "Darwin" ? "\u2303\u2325" : "Alt+Shift+";

  const rootNode = await walker.getRootNode();
  const a11yDoc = await a11yWalker.getAccessibleFor(rootNode);
  const buttonNode = await walker.querySelector(walker.rootNode, "#button");
  const accessibleFront = await a11yWalker.getAccessibleFor(buttonNode);
  const sliderNode = await walker.querySelector(walker.rootNode, "#slider");
  const accessibleSliderFront = await a11yWalker.getAccessibleFor(sliderNode);
  const browser = gBrowser.selectedBrowser;

  checkA11yFront(accessibleFront, {
    name: "Accessible Button",
    role: "button",
    childCount: 1,
  });

  await accessibleFront.hydrate();

  checkA11yFront(accessibleFront, {
    name: "Accessible Button",
    role: "button",
    value: "",
    description: "Accessibility Test",
    keyboardShortcut: modifiers + "b",
    childCount: 1,
    domNodeType: 1,
    indexInParent: 1,
    states: ["focusable", "opaque", "enabled", "sensitive"],
    actions: ["Press"],
    attributes: {
      "margin-top": "0px",
      display: "inline-block",
      "text-align": "center",
      "text-indent": "0px",
      "margin-left": "0px",
      tag: "button",
      "margin-right": "0px",
      id: "button",
      "margin-bottom": "0px",
    },
  });

  info("Name change event");
  await emitA11yEvent(
    accessibleFront,
    "name-change",
    (name, parent) => {
      checkA11yFront(accessibleFront, { name: "Renamed" });
      checkA11yFront(parent, {}, a11yDoc);
    },
    () =>
      SpecialPowers.spawn(browser, [], () =>
        content.document
          .getElementById("button")
          .setAttribute("aria-label", "Renamed")
      )
  );

  info("Description change event");
  await emitA11yEvent(
    accessibleFront,
    "description-change",
    () => checkA11yFront(accessibleFront, { description: "" }),
    () =>
      SpecialPowers.spawn(browser, [], () =>
        content.document
          .getElementById("button")
          .removeAttribute("aria-describedby")
      )
  );

  info("State change event");
  const expectedStates = ["unavailable", "opaque"];
  await emitA11yEvent(
    accessibleFront,
    "states-change",
    newStates => {
      checkA11yFront(accessibleFront, { states: expectedStates });
      SimpleTest.isDeeply(newStates, expectedStates, "States are updated");
    },
    () =>
      SpecialPowers.spawn(browser, [], () =>
        content.document.getElementById("button").setAttribute("disabled", true)
      )
  );

  info("Attributes change event");
  await emitA11yEvent(
    accessibleFront,
    "attributes-change",
    newAttrs => {
      checkA11yFront(accessibleFront, {
        attributes: {
          "container-live": "polite",
          display: "inline-block",
          "explicit-name": "true",
          id: "button",
          live: "polite",
          "margin-bottom": "0px",
          "margin-left": "0px",
          "margin-right": "0px",
          "margin-top": "0px",
          tag: "button",
          "text-align": "center",
          "text-indent": "0px",
        },
      });
      is(newAttrs.live, "polite", "Attributes are updated");
    },
    () =>
      SpecialPowers.spawn(browser, [], () =>
        content.document
          .getElementById("button")
          .setAttribute("aria-live", "polite")
      )
  );

  info("Value change event");
  await accessibleSliderFront.hydrate();
  checkA11yFront(accessibleSliderFront, { value: "5" });
  await emitA11yEvent(
    accessibleSliderFront,
    "value-change",
    () => checkA11yFront(accessibleSliderFront, { value: "6" }),
    () =>
      SpecialPowers.spawn(browser, [], () =>
        content.document
          .getElementById("slider")
          .setAttribute("aria-valuenow", "6")
      )
  );

  info("Reorder event");
  is(accessibleSliderFront.childCount, 1, "Slider has only 1 child");
  const [firstChild] = await accessibleSliderFront.children();
  await firstChild.hydrate();
  is(
    firstChild.indexInParent,
    0,
    "Slider's first child has correct index in parent"
  );
  await emitA11yEvent(
    accessibleSliderFront,
    "reorder",
    childCount => {
      is(childCount, 2, "Child count is updated");
      is(accessibleSliderFront.childCount, 2, "Child count is updated");
      is(
        firstChild.indexInParent,
        1,
        "Slider's first child has an updated index in parent"
      );
    },
    () =>
      SpecialPowers.spawn(browser, [], () => {
        const doc = content.document;
        const slider = doc.getElementById("slider");
        const button = doc.createElement("button");
        button.innerText = "Slider button";
        content.document
          .getElementById("slider")
          .insertBefore(button, slider.firstChild);
      })
  );

  await emitA11yEvent(
    firstChild,
    "index-in-parent-change",
    indexInParent =>
      is(
        indexInParent,
        0,
        "Slider's first child has an updated index in parent"
      ),
    () =>
      SpecialPowers.spawn(browser, [], () =>
        content.document.getElementById("slider").firstChild.remove()
      )
  );

  await waitForA11yShutdown(parentAccessibility);
  await target.destroy();
  gBrowser.removeCurrentTab();
});
