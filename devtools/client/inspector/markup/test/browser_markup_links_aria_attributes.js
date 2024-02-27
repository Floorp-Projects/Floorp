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
    links: [{ id: "activedescendant01" }],
  },
  {
    selector: "#aria-controls",
    attributeName: "aria-controls",
    links: [{ id: "controls01" }, { id: "controls02" }],
  },
  {
    selector: "#aria-describedby",
    attributeName: "aria-describedby",
    links: [{ id: "describedby01" }, { id: "describedby02" }],
  },
  {
    selector: "#aria-details",
    attributeName: "aria-details",
    links: [{ id: "details01" }, { id: "details02" }],
  },
  {
    selector: "#aria-errormessage",
    attributeName: "aria-errormessage",
    links: [{ id: "errormessage01" }],
  },
  {
    selector: "#aria-flowto",
    attributeName: "aria-flowto",
    links: [{ id: "flowto01" }, { id: "flowto02" }],
  },
  {
    selector: "#aria-labelledby",
    attributeName: "aria-labelledby",
    links: [{ id: "labelledby01" }, { id: "labelledby02" }],
  },
  {
    selector: "#aria-owns",
    attributeName: "aria-owns",
    links: [{ id: "owns01" }, { id: "owns02" }],
  },
  {
    getContainer: async inspector => {
      info("Find and expand the test-component shadow DOM host.");
      const hostFront = await getNodeFront("test-component", inspector);
      const hostContainer = inspector.markup.getContainer(hostFront);
      await expandContainer(inspector, hostContainer);

      info("Expand the shadow root");
      const shadowRootContainer = hostContainer.getChildContainers()[0];
      await expandContainer(inspector, shadowRootContainer);

      info("Expand the slot");
      const slotContainer = shadowRootContainer.getChildContainers()[0];

      is(
        slotContainer.elt
          .querySelector(`[data-attr="id"]`)
          .getAttribute("data-value"),
        "aria-describedby-shadow-dom",
        `This is the container for button#aria-describedby-shadow-dom`
      );

      // The test expect the node to be selected
      const updated = inspector.once("inspector-updated");
      inspector.selection.setNodeFront(slotContainer.node, { reason: "test" });
      await updated;

      return slotContainer;
    },
    attributeName: "aria-describedby",
    links: [{ id: "describedby01", valid: false }, { id: "describedbyshadow" }],
  },
  {
    selector: "#empty-attributes",
    attributeName: "aria-activedescendant",
    links: [],
  },
  {
    selector: "#empty-attributes",
    attributeName: "aria-details",
    links: [],
  },
];

add_task(async function () {
  const { inspector } = await openInspectorForURL(TEST_URL);

  for (const test of TEST_DATA) {
    let editor;
    if (typeof test.getContainer === "function") {
      ({ editor } = await test.getContainer(inspector));
    } else {
      info("Selecting test node " + test.selector);
      await selectNode(test.selector, inspector);
      info("Finding the popupNode to anchor the context-menu to");
      ({ editor } = await getContainerForSelector(test.selector, inspector));
    }

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
      const { id: expectedReferencedNodeId } = test.links[i];
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

      if (test.links[i].valid !== false) {
        info("Check that clicking on button selects the associated node");
        const onSelection = inspector.selection.once("new-node-front");
        buttonEl.click();
        await onSelection;

        is(
          inspector.selection.nodeFront.id,
          expectedReferencedNodeId,
          "The expected new node was selected"
        );
      } else {
        info(
          "Check that clicking on button triggers idref-attribute-link-failed event"
        );
        const onIdrefAttributeLinkFailed = inspector.markup.once(
          "idref-attribute-link-failed"
        );
        const onSelection = inspector.selection.once("new-node-front");
        const onTimeout = wait(500).then(() => "TIMEOUT");
        buttonEl.click();
        await onIdrefAttributeLinkFailed;
        ok(true, "Got expected idref-attribute-link-failed event");
        const res = await Promise.race([onSelection, onTimeout]);
        is(res, "TIMEOUT", "Clicking the button did not select a new node");
      }
    }
  }
});
