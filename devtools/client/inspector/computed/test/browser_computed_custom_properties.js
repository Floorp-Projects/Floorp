/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that custom properties are displayed in the computed view.

const TEST_URI = `
  <style type="text/css">
    body {
      --global-custom-property: red;
    }

    h1 {
      color: var(--global-custom-property);
    }

    #match-1 {
      --global-custom-property: blue;
      --custom-property-1: lime;
    }
    #match-2 {
      --global-custom-property: gold;
      --custom-property-2: cyan;
    }
  </style>
  <h1 id="match-1">Hello</h1>
  <h1 id="match-2">World</h1>
`;

add_task(async function () {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view } = await openComputedView();

  await assertComputedPropertiesForNode(inspector, view, "body", [
    {
      name: "--global-custom-property",
      value: "red",
    },
  ]);

  await assertComputedPropertiesForNode(inspector, view, "#match-1", [
    {
      name: "color",
      value: "rgb(0, 0, 255)",
    },
    {
      name: "--custom-property-1",
      value: "lime",
    },
    {
      name: "--global-custom-property",
      value: "blue",
    },
  ]);

  await assertComputedPropertiesForNode(inspector, view, "#match-2", [
    {
      name: "color",
      value: "rgb(255, 215, 0)",
    },
    {
      name: "--custom-property-2",
      value: "cyan",
    },
    {
      name: "--global-custom-property",
      value: "gold",
    },
  ]);

  await assertComputedPropertiesForNode(inspector, view, "html", []);
});

async function assertComputedPropertiesForNode(
  inspector,
  view,
  selector,
  expected
) {
  await selectNode(selector, inspector);

  const computedItems = getComputedViewProperties(view);
  is(
    computedItems.length,
    expected.length,
    `Computed view has the expected number of items for "${selector}"`
  );
  for (let i = 0; i < computedItems.length; i++) {
    const expectedData = expected[i];
    const computedEl = computedItems[i];
    const nameSpan = computedEl.querySelector(".computed-property-name");
    const valueSpan = computedEl.querySelector(".computed-property-value");

    is(
      nameSpan.firstChild.textContent,
      expectedData.name,
      `computed item #${i} for "${selector}" is the expected one`
    );
    is(
      valueSpan.textContent,
      expectedData.value,
      `computed item #${i} for "${selector}" has expected value`
    );
  }
}
