/* vim: set ts=2 et sw=2 tw=80: */
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
      display: "inline-block"
    },
    changeStyle: async function(testActor) {
      await testActor.eval(`
        let node = document.getElementById("grid");
        node.style.display = "block";
      `);
    },
    after: {
      textContent: "block",
      display: "none"
    }
  },
  {
    desc: "Showing a 'grid' node by changing its style property",
    selector: "#block",
    before: {
      textContent: "block",
      display: "none"
    },
    changeStyle: async function(testActor) {
      await testActor.eval(`
        let node = document.getElementById("block");
        node.style.display = "grid";
      `);
    },
    after: {
      textContent: "grid",
      display: "inline-block"
    }
  },
  {
    desc: "Showing a 'flex' node by removing its hidden attribute",
    selector: "#flex",
    before: {
      textContent: "none",
      display: "none"
    },
    changeStyle: async function(testActor) {
      await testActor.eval(`
        document.getElementById("flex").removeAttribute("hidden");
      `);
    },
    after: {
      textContent: "flex",
      display: "inline-block"
    }
  }
];

add_task(async function() {
  const {inspector, testActor} = await openInspectorForURL(
    "data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));

  for (const data of TEST_DATA) {
    info("Running test case: " + data.desc);
    await runTestData(inspector, testActor, data);
  }
});

async function runTestData(inspector, testActor,
                      {selector, before, changeStyle, after}) {
  await selectNode(selector, inspector);
  const container = await getContainerForSelector(selector, inspector);
  const displayNode = container.elt.querySelector(".markup-badge[data-display]");

  is(displayNode.textContent, before.textContent,
    `Got the correct before display type for ${selector}: ${displayNode.textContent}`);
  is(displayNode.style.display, before.display,
    `Got the correct before display style for ${selector}: ${displayNode.style.display}`);

  info("Listening for the display-change event");
  const onDisplayChanged = inspector.markup.walker.once("display-change");
  info("Making style changes");
  await changeStyle(testActor);
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

  is(displayNode.textContent, after.textContent,
    `Got the correct after display type for ${selector}: ${displayNode.textContent}`);
  is(displayNode.style.display, after.display,
    `Got the correct after display style for ${selector}: ${displayNode.style.display}`);
}

