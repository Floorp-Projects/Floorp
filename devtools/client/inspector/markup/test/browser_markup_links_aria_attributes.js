/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the contextual menu shows the right items when clicking on a link
// in aria attributes.

const TEST_URL = URL_ROOT + "doc_markup_links_aria.html";

// The test case array contains objects with the following properties:
// - selector: css selector for the node to select in the inspector
// - attributeName: name of the attribute to test
// - popupNodeSelector: css selector for the element inside the attribute
//   element to use as the contextual menu anchor
// - linkFollowItemLabel: the expected label of the follow-link item
// - linkCopyItemLabel: the expected label of the copy-link item
const TEST_DATA = [
  {
    selector: "#aria-activedescendant",
    attributeName: "aria-activedescendant",
    popupNodeSelector: ".link",
    linkFollowItemLabel: INSPECTOR_L10N.getFormatStr(
      "inspector.menu.selectElement.label",
      "activedescendant01"
    ),
  },
  {
    selector: "#aria-controls",
    attributeName: "aria-controls",
    popupNodeSelector: ".link",
    linkFollowItemLabel: INSPECTOR_L10N.getFormatStr(
      "inspector.menu.selectElement.label",
      "controls01"
    ),
  },
  {
    selector: "#aria-controls",
    attributeName: "aria-controls",
    popupNodeSelector: ".link:nth-of-type(2)",
    linkFollowItemLabel: INSPECTOR_L10N.getFormatStr(
      "inspector.menu.selectElement.label",
      "controls02"
    ),
  },
  {
    selector: "#aria-describedby",
    attributeName: "aria-describedby",
    popupNodeSelector: ".link",
    linkFollowItemLabel: INSPECTOR_L10N.getFormatStr(
      "inspector.menu.selectElement.label",
      "describedby01"
    ),
  },
  {
    selector: "#aria-describedby",
    attributeName: "aria-describedby",
    popupNodeSelector: ".link:nth-of-type(2)",
    linkFollowItemLabel: INSPECTOR_L10N.getFormatStr(
      "inspector.menu.selectElement.label",
      "describedby02"
    ),
  },
  {
    selector: "#aria-details",
    attributeName: "aria-details",
    popupNodeSelector: ".link",
    linkFollowItemLabel: INSPECTOR_L10N.getFormatStr(
      "inspector.menu.selectElement.label",
      "details01"
    ),
  },
  {
    selector: "#aria-details",
    attributeName: "aria-details",
    popupNodeSelector: ".link:nth-of-type(2)",
    linkFollowItemLabel: INSPECTOR_L10N.getFormatStr(
      "inspector.menu.selectElement.label",
      "details02"
    ),
  },
  {
    selector: "#aria-errormessage",
    attributeName: "aria-errormessage",
    popupNodeSelector: ".link",
    linkFollowItemLabel: INSPECTOR_L10N.getFormatStr(
      "inspector.menu.selectElement.label",
      "errormessage01"
    ),
  },
  {
    selector: "#aria-flowto",
    attributeName: "aria-flowto",
    popupNodeSelector: ".link",
    linkFollowItemLabel: INSPECTOR_L10N.getFormatStr(
      "inspector.menu.selectElement.label",
      "flowto01"
    ),
  },
  {
    selector: "#aria-flowto",
    attributeName: "aria-flowto",
    popupNodeSelector: ".link:nth-of-type(2)",
    linkFollowItemLabel: INSPECTOR_L10N.getFormatStr(
      "inspector.menu.selectElement.label",
      "flowto02"
    ),
  },
  {
    selector: "#aria-labelledby",
    attributeName: "aria-labelledby",
    popupNodeSelector: ".link",
    linkFollowItemLabel: INSPECTOR_L10N.getFormatStr(
      "inspector.menu.selectElement.label",
      "labelledby01"
    ),
  },
  {
    selector: "#aria-labelledby",
    attributeName: "aria-labelledby",
    popupNodeSelector: ".link:nth-of-type(2)",
    linkFollowItemLabel: INSPECTOR_L10N.getFormatStr(
      "inspector.menu.selectElement.label",
      "labelledby02"
    ),
  },
  {
    selector: "#aria-owns",
    attributeName: "aria-owns",
    popupNodeSelector: ".link",
    linkFollowItemLabel: INSPECTOR_L10N.getFormatStr(
      "inspector.menu.selectElement.label",
      "owns01"
    ),
  },
  {
    selector: "#aria-owns",
    attributeName: "aria-owns",
    popupNodeSelector: ".link:nth-of-type(2)",
    linkFollowItemLabel: INSPECTOR_L10N.getFormatStr(
      "inspector.menu.selectElement.label",
      "owns02"
    ),
  },
];

add_task(async function () {
  const { inspector } = await openInspectorForURL(TEST_URL);

  for (const test of TEST_DATA) {
    info("Selecting test node " + test.selector);
    await selectNode(test.selector, inspector);

    info("Finding the popupNode to anchor the context-menu to");
    const { editor } = await getContainerForSelector(test.selector, inspector);
    const popupNode = editor.attrElements
      .get(test.attributeName)
      .querySelector(test.popupNodeSelector);
    ok(popupNode, "Found the popupNode in attribute " + test.attributeName);

    info("Simulating a context click on the popupNode");
    const allMenuItems = openContextMenuAndGetAllItems(inspector, {
      target: popupNode,
    });

    const linkFollow = allMenuItems.find(i => i.id === "node-menu-link-follow");
    const linkCopy = allMenuItems.find(i => i.id === "node-menu-link-copy");

    is(linkFollow.visible, true, "The follow-link item is visible");
    is(linkCopy.visible, false, "The copy-link item is not visible");
    is(
      linkFollow.label,
      test.linkFollowItemLabel,
      "the follow-link label is correct"
    );
  }
});
