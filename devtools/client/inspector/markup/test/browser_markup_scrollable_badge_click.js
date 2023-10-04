/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Tests that the correct elements show up and get highlighted in the markup view when the
// scrollable badge is clicked.

const TEST_URI = `
  <style type="text/css">
    .parent {
        width: 200px;
        height: 200px;
        overflow: scroll;
    }
    .fixed {
        width: 50px;
        height: 50px;
    }
    .shift {
        margin-left: 300px;
    }
    .has-before::before {
      content: "-";
    }
  </style>
  <div id="top" class="parent">
    <div id="child1" class="fixed shift">
        <div id="child2" class="fixed"></div>
    </div>
    <div id="child3" class="shift has-before">
        <div id="child4" class="fixed"></div>
    </div>
  </div>
`;

add_task(async function () {
  const { inspector } = await openInspectorForURL(
    "data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI)
  );

  const container = await getContainerForSelector("#top", inspector);
  const scrollableBage = container.elt.querySelector(".scrollable-badge");
  is(
    scrollableBage.getAttribute("aria-pressed"),
    "false",
    "Scrollable badge is not pressed by default"
  );

  info(
    "Clicking on the scrollable badge so that the overflow causing elements show up in the markup view."
  );
  scrollableBage.click();

  await waitForContainers(["#child1", "#child3", "#child4"], inspector);

  await checkOverflowHighlight(
    ["#child1", "#child4"],
    ["#child2", "#child3"],
    inspector
  );

  ok(scrollableBage.classList.contains("active"), "Scrollable badge is active");
  is(
    scrollableBage.getAttribute("aria-pressed"),
    "true",
    "Scrollable badge is pressed"
  );

  checkTelemetry("devtools.markup.scrollable.badge.clicked", "", 1, "scalar");

  info(
    "Changing CSS so elements update their overflow highlights accordingly."
  );
  await toggleClass(inspector);

  // By default, #child2 will not be visible in the markup view,
  // so expand its parent to make it visible.
  const child1 = await getContainerForSelector("#child1", inspector);
  await expandContainer(inspector, child1);

  await checkOverflowHighlight(
    ["#child2", "#child3"],
    ["#child1", "#child4"],
    inspector
  );

  info(
    "Clicking on the scrollable badge again so that all the overflow highlight gets removed."
  );
  scrollableBage.click();

  await checkOverflowHighlight(
    [],
    ["#child1", "#child2", "#child3", "#child4"],
    inspector
  );

  ok(
    !scrollableBage.classList.contains("active"),
    "Scrollable badge is not active"
  );
  is(
    scrollableBage.getAttribute("aria-pressed"),
    "false",
    "Scrollable badge is not pressed anymore"
  );

  checkTelemetry("devtools.markup.scrollable.badge.clicked", "", 2, "scalar");

  info("Triggering badge with the keyboard");
  scrollableBage.focus();
  EventUtils.synthesizeKey("VK_RETURN", {}, scrollableBage.ownerGlobal);
  await checkOverflowHighlight(
    ["#child2", "#child3"],
    ["#child1", "#child4"],
    inspector
  );
  ok(
    scrollableBage.classList.contains("active"),
    "badge can be activated with the keyboard"
  );
  is(
    scrollableBage.getAttribute("aria-pressed"),
    "true",
    "Scrollable badge is pressed"
  );

  EventUtils.synthesizeKey("VK_RETURN", {}, scrollableBage.ownerGlobal);
  await checkOverflowHighlight(
    [],
    ["#child1", "#child2", "#child3", "#child4"],
    inspector
  );
  ok(
    !scrollableBage.classList.contains("active"),
    "Scrollable badge can be deactivated with the keyboard"
  );
  is(
    scrollableBage.getAttribute("aria-pressed"),
    "false",
    "Scrollable badge is not pressed anymore"
  );

  info("Double-click on the scrollable badge");
  EventUtils.sendMouseEvent({ type: "dblclick" }, scrollableBage);
  ok(
    container.expanded,
    "Double clicking on the badge did not collapse the container"
  );
});

async function getContainerForSelector(selector, inspector) {
  const nodeFront = await getNodeFront(selector, inspector);
  return getContainerForNodeFront(nodeFront, inspector);
}

async function waitForContainers(selectors, inspector) {
  for (const selector of selectors) {
    info(`Wait for markup container of ${selector}`);
    await asyncWaitUntil(() => getContainerForSelector(selector, inspector));
  }
}

async function elementHasHighlight(selector, inspector) {
  const container = await getContainerForSelector(selector, inspector);
  return container?.tagState.classList.contains("overflow-causing-highlighted");
}

async function checkOverflowHighlight(
  selectorWithHighlight,
  selectorWithNoHighlight,
  inspector
) {
  for (const selector of selectorWithHighlight) {
    ok(
      await elementHasHighlight(selector, inspector),
      `${selector} contains overflow highlight`
    );
  }
  for (const selector of selectorWithNoHighlight) {
    ok(
      !(await elementHasHighlight(selector, inspector)),
      `${selector} does not contain overflow highlight`
    );
  }
}

async function toggleClass(inspector) {
  const onStateChanged = inspector.walker.once("overflow-change");

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function () {
    content.document.querySelector("#child1").classList.toggle("fixed");
    content.document.querySelector("#child3").classList.toggle("fixed");
  });

  await onStateChanged;
}
