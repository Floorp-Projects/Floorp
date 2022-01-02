/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that nodes are marked as displayed and not-displayed dynamically, when
// their display changes

const TEST_URL = URL_ROOT + "doc_markup_not_displayed.html";
const TEST_DATA = [
  {
    desc: "Hiding a node by creating a new stylesheet",
    selector: "#normal-div",
    before: true,
    changeStyle: () => {
      const div = content.document.createElement("div");
      div.id = "new-style";
      div.innerHTML = "<style>#normal-div {display:none;}</style>";
      content.document.body.appendChild(div);
    },
    after: false,
  },
  {
    desc: "Showing a node by deleting an existing stylesheet",
    selector: "#normal-div",
    before: false,
    changeStyle: () => content.document.getElementById("new-style").remove(),
    after: true,
  },
  {
    desc: "Hiding a node by changing its style property",
    selector: "#display-none",
    before: false,
    changeStyle: () => {
      const node = content.document.querySelector("#display-none");
      node.style.display = "block";
    },
    after: true,
  },
  {
    desc: "Showing a node by removing its hidden attribute",
    selector: "#hidden-true",
    before: false,
    changeStyle: () =>
      content.document.querySelector("#hidden-true").removeAttribute("hidden"),
    after: true,
  },
  {
    desc: "Hiding a node by adding a hidden attribute",
    selector: "#hidden-true",
    before: true,
    changeStyle: () =>
      content.document
        .querySelector("#hidden-true")
        .setAttribute("hidden", true),
    after: false,
  },
  {
    desc: "Showing a node by changin a stylesheet's rule",
    selector: "#hidden-via-stylesheet",
    before: false,
    changeStyle: () => {
      content.document.styleSheets[0].cssRules[0].style.setProperty(
        "display",
        "inline"
      );
    },
    after: true,
  },
  {
    desc: "Hiding a node by adding a new rule to a stylesheet",
    selector: "#hidden-via-stylesheet",
    before: true,
    changeStyle: () => {
      content.document.styleSheets[0].insertRule(
        "#hidden-via-stylesheet {display: none;}",
        1
      );
    },
    after: false,
  },
  {
    desc: "Hiding a node by adding a class that matches an existing rule",
    selector: "#normal-div",
    before: true,
    changeStyle: () => {
      content.document.styleSheets[0].insertRule(
        ".a-new-class {display: none;}",
        2
      );
      content.document
        .querySelector("#normal-div")
        .classList.add("a-new-class");
    },
    after: false,
  },
];

add_task(async function() {
  const { inspector } = await openInspectorForURL(TEST_URL);

  for (const data of TEST_DATA) {
    info("Running test case: " + data.desc);
    await runTestData(inspector, data);
  }
});

async function runTestData(
  inspector,
  { selector, before, changeStyle, after }
) {
  info("Getting the " + selector + " test node");
  const nodeFront = await getNodeFront(selector, inspector);
  const container = getContainerForNodeFront(nodeFront, inspector);
  is(
    !container.elt.classList.contains("not-displayed"),
    before,
    "The container is marked as " + (before ? "shown" : "hidden")
  );

  info("Listening for the display-change event");
  const onDisplayChanged = new Promise(resolve => {
    inspector.markup.walker.once("display-change", resolve);
  });

  info("Making style changes");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], changeStyle);
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

  is(
    !container.elt.classList.contains("not-displayed"),
    after,
    "The container is marked as " + (after ? "shown" : "hidden")
  );
}
