/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that markup display node are updated when their display changes.

const TEST_URI = `
  <style type="text/css">
    #grid {
      display: grid;
    }
    #flex {
      display: flex;
    }
    #flex[hidden] {
      display: none;
    }
    #block {
      display: block;
    }
    #flex
  </style>
  <div id="grid">Grid</div>
  <div id="flex" hidden="">Flex</div>
  <div id="block">Block</div>
`;

const TEST_DATA = [
  {
    desc: "Hiding the #grid display node by changing its style property",
    selector: "#grid",
    before: {
      textContent: "grid",
      visible: true,
    },
    changeStyle: async function() {
      await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
        const node = content.document.getElementById("grid");
        node.style.display = "block";
      });
    },
    after: {
      visible: false,
    },
  },
  {
    desc: "Reusing the 'grid' node, updating the display to 'grid again",
    selector: "#grid",
    before: {
      visible: false,
    },
    changeStyle: async function() {
      await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
        const node = content.document.getElementById("grid");
        node.style.display = "grid";
      });
    },
    after: {
      textContent: "grid",
      visible: true,
    },
  },
  {
    desc: "Showing a 'grid' node by changing its style property",
    selector: "#block",
    before: {
      visible: false,
    },
    changeStyle: async function() {
      await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
        const node = content.document.getElementById("block");
        node.style.display = "grid";
      });
    },
    after: {
      textContent: "grid",
      visible: true,
    },
  },
  {
    desc: "Showing a 'flex' node by removing its hidden attribute",
    selector: "#flex",
    before: {
      visible: false,
    },
    changeStyle: async function() {
      await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () =>
        content.document.getElementById("flex").removeAttribute("hidden")
      );
    },
    after: {
      textContent: "flex",
      visible: true,
    },
  },
];

add_task(async function() {
  const { inspector } = await openInspectorForURL(
    "data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI)
  );

  for (const data of TEST_DATA) {
    info("Running test case: " + data.desc);
    await runTestData(inspector, data);
  }
});

async function runTestData(
  inspector,
  { selector, before, changeStyle, after }
) {
  await selectNode(selector, inspector);
  const container = await getContainerForSelector(selector, inspector);

  const beforeBadge = container.elt.querySelector(
    ".inspector-badge.interactive[data-display]"
  );
  is(
    !!beforeBadge,
    before.visible,
    `Display badge is visible as expected for ${selector}: ${before.visible}`
  );
  if (before.visible) {
    is(
      beforeBadge.textContent,
      before.textContent,
      `Got the correct before display type for ${selector}: ${beforeBadge.textContent}`
    );
  }

  info("Listening for the display-change event");
  const onDisplayChanged = inspector.markup.walker.once("display-change");
  info("Making style changes");
  await changeStyle();
  const nodes = await onDisplayChanged;

  info("Verifying that the list of changed nodes include our container");
  ok(nodes.length, "The display-change event was received with a nodes");
  let foundContainer = false;
  for (const node of nodes) {
    if (getContainerForNodeFront(node, inspector) === container) {
      foundContainer = true;
      break;
    }
  }
  ok(foundContainer, "Container is part of the list of changed nodes");

  const afterBadge = container.elt.querySelector(
    ".inspector-badge.interactive[data-display]"
  );
  is(
    !!afterBadge,
    after.visible,
    `Display badge is visible as expected for ${selector}: ${after.visible}`
  );
  if (after.visible) {
    is(
      afterBadge.textContent,
      after.textContent,
      `Got the correct after display type for ${selector}: ${afterBadge.textContent}`
    );
  }
}
