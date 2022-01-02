/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that context menu items are enabled / disabled correctly.

const TEST_URL = URL_ROOT + "doc_inspector_menu.html";

const PASTE_MENU_ITEMS = [
  "node-menu-pasteinnerhtml",
  "node-menu-pasteouterhtml",
  "node-menu-pastebefore",
  "node-menu-pasteafter",
  "node-menu-pastefirstchild",
  "node-menu-pastelastchild",
];

const ACTIVE_ON_DOCTYPE_ITEMS = [
  "node-menu-showdomproperties",
  "node-menu-useinconsole",
];

const ACTIVE_ON_SHADOW_ROOT_ITEMS = [
  "node-menu-pasteinnerhtml",
  "node-menu-copyinner",
  "node-menu-edithtml",
].concat(ACTIVE_ON_DOCTYPE_ITEMS);

const ALL_MENU_ITEMS = [
  "node-menu-edithtml",
  "node-menu-copyinner",
  "node-menu-copyouter",
  "node-menu-copyuniqueselector",
  "node-menu-copycsspath",
  "node-menu-copyxpath",
  "node-menu-copyimagedatauri",
  "node-menu-delete",
  "node-menu-pseudo-hover",
  "node-menu-pseudo-active",
  "node-menu-pseudo-focus",
  "node-menu-pseudo-focus-within",
  "node-menu-scrollnodeintoview",
  "node-menu-screenshotnode",
  "node-menu-add-attribute",
  "node-menu-copy-attribute",
  "node-menu-edit-attribute",
  "node-menu-remove-attribute",
].concat(PASTE_MENU_ITEMS, ACTIVE_ON_DOCTYPE_ITEMS);

const INACTIVE_ON_DOCTYPE_ITEMS = ALL_MENU_ITEMS.filter(
  item => !ACTIVE_ON_DOCTYPE_ITEMS.includes(item)
);

const INACTIVE_ON_DOCUMENT_ITEMS = INACTIVE_ON_DOCTYPE_ITEMS;

const INACTIVE_ON_SHADOW_ROOT_ITEMS = ALL_MENU_ITEMS.filter(
  item => !ACTIVE_ON_SHADOW_ROOT_ITEMS.includes(item)
);

/**
 * Test cases, each item of this array may define the following properties:
 *   desc: string that will be logged
 *   selector: selector of the node to be selected
 *   disabled: items that should have disabled state
 *   clipboardData: clipboard content
 *   clipboardDataType: clipboard content type
 *   attributeTrigger: attribute that will be used as context menu trigger
 *   shadowRoot: if true, selects the shadow root from the node, rather than
 *   the node itself.
 */
const TEST_CASES = [
  {
    desc: "doctype node with empty clipboard",
    selector: null,
    disabled: INACTIVE_ON_DOCTYPE_ITEMS,
  },
  {
    desc: "doctype node with html on clipboard",
    clipboardData: "<p>some text</p>",
    clipboardDataType: "text",
    selector: null,
    disabled: INACTIVE_ON_DOCTYPE_ITEMS,
  },
  {
    desc: "element node HTML on the clipboard",
    clipboardData: "<p>some text</p>",
    clipboardDataType: "text",
    disabled: [
      "node-menu-copyimagedatauri",
      "node-menu-copy-attribute",
      "node-menu-edit-attribute",
      "node-menu-remove-attribute",
    ],
    selector: "#sensitivity",
  },
  {
    desc: "<html> element",
    clipboardData: "<p>some text</p>",
    clipboardDataType: "text",
    selector: "html",
    disabled: [
      "node-menu-copyimagedatauri",
      "node-menu-pastebefore",
      "node-menu-pasteafter",
      "node-menu-pastefirstchild",
      "node-menu-pastelastchild",
      "node-menu-copy-attribute",
      "node-menu-edit-attribute",
      "node-menu-remove-attribute",
      "node-menu-delete",
    ],
  },
  {
    desc: "<body> with HTML on clipboard",
    clipboardData: "<p>some text</p>",
    clipboardDataType: "text",
    selector: "body",
    disabled: [
      "node-menu-copyimagedatauri",
      "node-menu-pastebefore",
      "node-menu-pasteafter",
      "node-menu-copy-attribute",
      "node-menu-edit-attribute",
      "node-menu-remove-attribute",
    ],
  },
  {
    desc: "<img> with HTML on clipboard",
    clipboardData: "<p>some text</p>",
    clipboardDataType: "text",
    selector: "img",
    disabled: [
      "node-menu-copy-attribute",
      "node-menu-edit-attribute",
      "node-menu-remove-attribute",
    ],
  },
  {
    desc: "<head> with HTML on clipboard",
    clipboardData: "<p>some text</p>",
    clipboardDataType: "text",
    selector: "head",
    disabled: [
      "node-menu-copyimagedatauri",
      "node-menu-pastebefore",
      "node-menu-pasteafter",
      "node-menu-screenshotnode",
      "node-menu-copy-attribute",
      "node-menu-edit-attribute",
      "node-menu-remove-attribute",
    ],
  },
  {
    desc: "<head> with no html on clipboard",
    selector: "head",
    disabled: PASTE_MENU_ITEMS.concat([
      "node-menu-copyimagedatauri",
      "node-menu-screenshotnode",
      "node-menu-copy-attribute",
      "node-menu-edit-attribute",
      "node-menu-remove-attribute",
    ]),
  },
  {
    desc: "<element> with text on clipboard",
    clipboardData: "some text",
    clipboardDataType: "text",
    selector: "#paste-area",
    disabled: [
      "node-menu-copyimagedatauri",
      "node-menu-copy-attribute",
      "node-menu-edit-attribute",
      "node-menu-remove-attribute",
    ],
  },
  {
    desc: "<element> with base64 encoded image data uri on clipboard",
    clipboardData:
      "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABC" +
      "AAAAAA6fptVAAAACklEQVQYV2P4DwABAQEAWk1v8QAAAABJRU5ErkJggg==",
    clipboardDataType: "image",
    selector: "#paste-area",
    disabled: PASTE_MENU_ITEMS.concat([
      "node-menu-copyimagedatauri",
      "node-menu-copy-attribute",
      "node-menu-edit-attribute",
      "node-menu-remove-attribute",
    ]),
  },
  {
    desc: "<element> with empty string on clipboard",
    clipboardData: "",
    clipboardDataType: "text",
    selector: "#paste-area",
    disabled: PASTE_MENU_ITEMS.concat([
      "node-menu-copyimagedatauri",
      "node-menu-copy-attribute",
      "node-menu-edit-attribute",
      "node-menu-remove-attribute",
    ]),
  },
  {
    desc: "<element> with whitespace only on clipboard",
    clipboardData: " \n\n\t\n\n  \n",
    clipboardDataType: "text",
    selector: "#paste-area",
    disabled: PASTE_MENU_ITEMS.concat([
      "node-menu-copyimagedatauri",
      "node-menu-copy-attribute",
      "node-menu-edit-attribute",
      "node-menu-remove-attribute",
    ]),
  },
  {
    desc: "<element> that isn't visible on the page, empty clipboard",
    selector: "#hiddenElement",
    disabled: PASTE_MENU_ITEMS.concat([
      "node-menu-copyimagedatauri",
      "node-menu-screenshotnode",
      "node-menu-copy-attribute",
      "node-menu-edit-attribute",
      "node-menu-remove-attribute",
    ]),
  },
  {
    desc: "<element> nested in another hidden element, empty clipboard",
    selector: "#nestedHiddenElement",
    disabled: PASTE_MENU_ITEMS.concat([
      "node-menu-copyimagedatauri",
      "node-menu-screenshotnode",
      "node-menu-copy-attribute",
      "node-menu-edit-attribute",
      "node-menu-remove-attribute",
    ]),
  },
  {
    desc: "<element> with context menu triggered on attribute, empty clipboard",
    selector: "#attributes",
    disabled: PASTE_MENU_ITEMS.concat(["node-menu-copyimagedatauri"]),
    attributeTrigger: "data-edit",
  },
  {
    desc: "Shadow Root",
    clipboardData: "<p>some text</p>",
    clipboardDataType: "text",
    disabled: INACTIVE_ON_SHADOW_ROOT_ITEMS,
    selector: "#host",
    shadowRoot: true,
  },
  {
    desc: "Document node in iFrame",
    disabled: INACTIVE_ON_DOCUMENT_ITEMS,
    selector: "iframe",
    documentNode: true,
  },
];

var clipboard = require("devtools/shared/platform/clipboard");
registerCleanupFunction(() => {
  clipboard.copyString("");
  clipboard = null;
});

add_task(async function() {
  const { inspector } = await openInspectorForURL(TEST_URL);
  for (const test of TEST_CASES) {
    const {
      desc,
      disabled,
      selector,
      attributeTrigger,
      documentNode = false,
      shadowRoot = false,
    } = test;

    info(`Test ${desc}`);
    setupClipboard(test.clipboardData, test.clipboardDataType);

    const front = await getNodeFrontForSelector(
      selector,
      inspector,
      documentNode,
      shadowRoot
    );

    info("Selecting the specified node.");
    await selectNode(front, inspector);

    info("Simulating context menu click on the selected node container.");
    const nodeFrontContainer = getContainerForNodeFront(front, inspector);
    const contextMenuTrigger = attributeTrigger
      ? nodeFrontContainer.tagLine.querySelector(
          `[data-attr="${attributeTrigger}"]`
        )
      : nodeFrontContainer.tagLine;

    const allMenuItems = openContextMenuAndGetAllItems(inspector, {
      target: contextMenuTrigger,
    });

    for (const id of ALL_MENU_ITEMS) {
      const menuItem = allMenuItems.find(item => item.id === id);
      const shouldBeDisabled = disabled.includes(id);
      const shouldBeDisabledText = shouldBeDisabled ? "disabled" : "enabled";
      is(
        menuItem.disabled,
        shouldBeDisabled,
        `#${id} should be ${shouldBeDisabledText} for test case ${desc}`
      );
    }
  }
});

/**
 * A helper that fetches a front for a node that matches the given selector or
 * doctype node if the selector is falsy.
 */
async function getNodeFrontForSelector(
  selector,
  inspector,
  documentNode,
  shadowRoot
) {
  if (selector) {
    info("Retrieving front for selector " + selector);
    const node = await getNodeFront(selector, inspector);
    if (shadowRoot) {
      return getShadowRoot(node, inspector);
    }
    if (documentNode) {
      return getFrameDocument(node, inspector);
    }
    return node;
  }

  info("Retrieving front for doctype node");
  const { nodes } = await inspector.walker.children(inspector.walker.rootNode);
  return nodes[0];
}

/**
 * A helper that populates the clipboard with data of given type. Clears the
 * clipboard if data is falsy.
 */
function setupClipboard(data, type) {
  if (!data) {
    info("Clearing the clipboard.");
    clipboard.copyString("");
  } else if (type === "text") {
    info("Populating clipboard with text.");
    clipboard.copyString(data);
  } else if (type === "image") {
    info("Populating clipboard with image content");
    copyImageToClipboard(data);
  }
}

/**
 * The code below is a simplified version of the sdk/clipboard helper set() method.
 */
function copyImageToClipboard(data) {
  const imageTools = Cc["@mozilla.org/image/tools;1"].getService(Ci.imgITools);

  // Image data is stored as base64 in the test.
  const image = atob(data);

  const imgPtr = Cc["@mozilla.org/supports-interface-pointer;1"].createInstance(
    Ci.nsISupportsInterfacePointer
  );
  imgPtr.data = imageTools.decodeImageFromBuffer(
    image,
    image.length,
    "image/png"
  );

  const xferable = Cc["@mozilla.org/widget/transferable;1"].createInstance(
    Ci.nsITransferable
  );
  xferable.init(null);
  xferable.addDataFlavor("image/png");
  xferable.setTransferData("image/png", imgPtr);

  Services.clipboard.setData(
    xferable,
    null,
    Services.clipboard.kGlobalClipboard
  );
}
