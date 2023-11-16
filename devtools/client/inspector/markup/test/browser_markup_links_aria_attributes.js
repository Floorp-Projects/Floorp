/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the contextual menu shows the right items when clicking on a link
// in aria attributes.

const TEST_URL = URL_ROOT + "doc_markup_links_aria_attributes.html";

// The test case array contains objects with the following properties:
// - selector: css selector for the node to select in the inspector
// - attributeName: name of the attribute to test
// - links: an array of id strings that are expected to be in the attribute
const TEST_DATA = [
  {
    selector: "#aria-activedescendant",
    attributeName: "aria-activedescendant",
    links: ["activedescendant01"],
  },
  {
    selector: "#aria-controls",
    attributeName: "aria-controls",
    links: ["controls01", "controls02"],
  },
  {
    selector: "#aria-describedby",
    attributeName: "aria-describedby",
    links: ["describedby01", "describedby02"],
  },
  {
    selector: "#aria-details",
    attributeName: "aria-details",
    links: ["details01", "details02"],
  },
  {
    selector: "#aria-errormessage",
    attributeName: "aria-errormessage",
    links: ["errormessage01"],
  },
  {
    selector: "#aria-flowto",
    attributeName: "aria-flowto",
    links: ["flowto01", "flowto02"],
  },
  {
    selector: "#aria-labelledby",
    attributeName: "aria-labelledby",
    links: ["labelledby01", "labelledby02"],
  },
  {
    selector: "#aria-owns",
    attributeName: "aria-owns",
    links: ["owns01", "owns02"],
  },
];

add_task(async function () {
  const { inspector } = await openInspectorForURL(TEST_URL);

  for (const test of TEST_DATA) {
    info("Selecting test node " + test.selector);
    await selectNode(test.selector, inspector);

    info("Finding the popupNode to anchor the context-menu to");
    const { editor } = await getContainerForSelector(test.selector, inspector);
    const attributeEl = editor.attrElements.get(test.attributeName);
    const linksEl = attributeEl.querySelectorAll(".link");

    is(
      linksEl.length,
      test.links.length,
      "We have the expected number of links in attribute " + test.attributeName
    );

    for (let i = 0; i < test.links.length; i++) {
      info(`Checking link # ${i} for attribute "${test.attributeName}"`);

      const linkEl = linksEl[i];
      ok(linkEl, "Found the link");

      const expectedReferencedNodeId = test.links[i];

      info("Simulating a context click on the link");
      const allMenuItems = openContextMenuAndGetAllItems(inspector, {
        target: linkEl,
      });

      const linkFollow = allMenuItems.find(
        ({ id }) => id === "node-menu-link-follow"
      );
      const linkCopy = allMenuItems.find(
        ({ id }) => id === "node-menu-link-copy"
      );

      is(linkFollow.visible, true, "The follow-link item is visible");
      is(linkCopy.visible, false, "The copy-link item is not visible");
      const linkFollowItemLabel = INSPECTOR_L10N.getFormatStr(
        "inspector.menu.selectElement.label",
        expectedReferencedNodeId
      );
      is(
        linkFollow.label,
        linkFollowItemLabel,
        "the follow-link label is correct"
      );

      info("Check that select node button is displayed");
      const buttonEl = linkEl.querySelector("button.select-node");
      ok(buttonEl, "Found the select node button");
      is(
        buttonEl.getAttribute("title"),
        linkFollowItemLabel,
        "Button has expected title"
      );

      info("Check that clicking on button selects the associated node");
      const onSelection = inspector.selection.once("new-node-front");
      buttonEl.click();
      await onSelection;

      is(
        inspector.selection.nodeFront.id,
        expectedReferencedNodeId,
        "The expected new node was selected"
      );
    }
  }
});
