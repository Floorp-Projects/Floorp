/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the contextual menu shows the right items when clicking on a link
// in an attribute.

const TEST_URL = URL_ROOT + "doc_markup_links.html";

const TOOLBOX_L10N = new LocalizationHelper("devtools/client/locales/toolbox.properties");

// The test case array contains objects with the following properties:
// - selector: css selector for the node to select in the inspector
// - attributeName: name of the attribute to test
// - popupNodeSelector: css selector for the element inside the attribute
//   element to use as the contextual menu anchor
// - isLinkFollowItemVisible: is the follow-link item expected to be displayed
// - isLinkCopyItemVisible: is the copy-link item expected to be displayed
// - linkFollowItemLabel: the expected label of the follow-link item
// - linkCopyItemLabel: the expected label of the copy-link item
const TEST_DATA = [{
  selector: "link",
  attributeName: "href",
  popupNodeSelector: ".link",
  isLinkFollowItemVisible: true,
  isLinkCopyItemVisible: true,
  linkFollowItemLabel: TOOLBOX_L10N.getStr(
    "toolbox.viewCssSourceInStyleEditor.label"),
  linkCopyItemLabel: INSPECTOR_L10N.getStr(
    "inspector.menu.copyUrlToClipboard.label")
}, {
  selector: "link[rel=icon]",
  attributeName: "href",
  popupNodeSelector: ".link",
  isLinkFollowItemVisible: true,
  isLinkCopyItemVisible: true,
  linkFollowItemLabel: INSPECTOR_L10N.getStr(
    "inspector.menu.openUrlInNewTab.label"),
  linkCopyItemLabel: INSPECTOR_L10N.getStr(
    "inspector.menu.copyUrlToClipboard.label")
}, {
  selector: "link",
  attributeName: "rel",
  popupNodeSelector: ".attr-value",
  isLinkFollowItemVisible: false,
  isLinkCopyItemVisible: false
}, {
  selector: "output",
  attributeName: "for",
  popupNodeSelector: ".link",
  isLinkFollowItemVisible: true,
  isLinkCopyItemVisible: false,
  linkFollowItemLabel: INSPECTOR_L10N.getFormatStr(
    "inspector.menu.selectElement.label", "name")
}, {
  selector: "script",
  attributeName: "src",
  popupNodeSelector: ".link",
  isLinkFollowItemVisible: true,
  isLinkCopyItemVisible: true,
  linkFollowItemLabel: TOOLBOX_L10N.getStr(
    "toolbox.viewJsSourceInDebugger.label"),
  linkCopyItemLabel: INSPECTOR_L10N.getStr(
    "inspector.menu.copyUrlToClipboard.label")
}, {
  selector: "p[for]",
  attributeName: "for",
  popupNodeSelector: ".attr-value",
  isLinkFollowItemVisible: false,
  isLinkCopyItemVisible: false
}];

add_task(async function() {
  const {inspector} = await openInspectorForURL(TEST_URL);

  for (const test of TEST_DATA) {
    info("Selecting test node " + test.selector);
    await selectNode(test.selector, inspector);

    info("Finding the popupNode to anchor the context-menu to");
    const {editor} = await getContainerForSelector(test.selector, inspector);
    const popupNode = editor.attrElements.get(test.attributeName)
                    .querySelector(test.popupNodeSelector);
    ok(popupNode, "Found the popupNode in attribute " + test.attributeName);

    info("Simulating a context click on the popupNode");
    const allMenuItems = openContextMenuAndGetAllItems(inspector, {
      target: popupNode,
    });

    const linkFollow = allMenuItems.find(i => i.id === "node-menu-link-follow");
    const linkCopy = allMenuItems.find(i => i.id === "node-menu-link-copy");

    is(linkFollow.visible, test.isLinkFollowItemVisible,
      "The follow-link item display is correct");
    is(linkCopy.visible, test.isLinkCopyItemVisible,
      "The copy-link item display is correct");

    if (test.isLinkFollowItemVisible) {
      is(linkFollow.label, test.linkFollowItemLabel,
        "the follow-link label is correct");
    }
    if (test.isLinkCopyItemVisible) {
      is(linkCopy.label, test.linkCopyItemLabel,
        "the copy-link label is correct");
    }
  }
});
