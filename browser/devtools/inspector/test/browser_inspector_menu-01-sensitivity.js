/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that context menu items are enabled / disabled correctly.

const TEST_URL = TEST_URL_ROOT + "doc_inspector_menu.html";

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
  "node-menu-useinconsole"
];

const ALL_MENU_ITEMS = [
  "node-menu-edithtml",
  "node-menu-copyinner",
  "node-menu-copyouter",
  "node-menu-copyuniqueselector",
  "node-menu-copyimagedatauri",
  "node-menu-delete",
  "node-menu-pseudo-hover",
  "node-menu-pseudo-active",
  "node-menu-pseudo-focus",
  "node-menu-scrollnodeintoview",
  "node-menu-screenshotnode"
].concat(PASTE_MENU_ITEMS, ACTIVE_ON_DOCTYPE_ITEMS);

const INACTIVE_ON_DOCTYPE_ITEMS =
  ALL_MENU_ITEMS.filter(item => ACTIVE_ON_DOCTYPE_ITEMS.indexOf(item) === -1);

const TEST_CASES = [
  {
    desc: "doctype node with empty clipboard",
    selector: null,
    disabled: INACTIVE_ON_DOCTYPE_ITEMS,
  },
  {
    desc: "doctype node with html on clipboard",
    clipboardData: "<p>some text</p>",
    clipboardDataType: "html",
    selector: null,
    disabled: INACTIVE_ON_DOCTYPE_ITEMS,
  },
  {
    desc: "element node HTML on the clipboard",
    clipboardData: "<p>some text</p>",
    clipboardDataType: "html",
    disabled: ["node-menu-copyimagedatauri"],
    selector: "#sensitivity",
  },
  {
    desc: "<html> element",
    clipboardData: "<p>some text</p>",
    clipboardDataType: "html",
    selector: "html",
    disabled: [
      "node-menu-copyimagedatauri",
      "node-menu-pastebefore",
      "node-menu-pasteafter",
      "node-menu-pastefirstchild",
      "node-menu-pastelastchild",
    ],
  },
  {
    desc: "<body> with HTML on clipboard",
    clipboardData: "<p>some text</p>",
    clipboardDataType: "html",
    selector: "body",
    disabled: [
      "node-menu-copyimagedatauri",
      "node-menu-pastebefore",
      "node-menu-pasteafter",
    ]
  },
  {
    desc: "<img> with HTML on clipboard",
    clipboardData: "<p>some text</p>",
    clipboardDataType: "html",
    selector: "img",
    disabled: []
  },
  {
    desc: "<head> with HTML on clipboard",
    clipboardData: "<p>some text</p>",
    clipboardDataType: "html",
    selector: "head",
    disabled: [
      "node-menu-copyimagedatauri",
      "node-menu-pastebefore",
      "node-menu-pasteafter",
      "node-menu-screenshotnode",
    ],
  },
  {
    desc: "<head> with no html on clipboard",
    selector: "head",
    disabled: PASTE_MENU_ITEMS.concat([
      "node-menu-copyimagedatauri",
      "node-menu-screenshotnode",
    ]),
  },
  {
    desc: "<element> with text on clipboard",
    clipboardData: "some text",
    clipboardDataType: undefined,
    selector: "#paste-area",
    disabled: ["node-menu-copyimagedatauri"],
  },
  {
    desc: "<element> with base64 encoded image data uri on clipboard",
    clipboardData:
      "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABC" +
      "AAAAAA6fptVAAAACklEQVQYV2P4DwABAQEAWk1v8QAAAABJRU5ErkJggg==",
    clipboardDataType: undefined,
    selector: "#paste-area",
    disabled: PASTE_MENU_ITEMS.concat(["node-menu-copyimagedatauri"]),
  },
  {
    desc: "<element> with empty string on clipboard",
    clipboardData: "",
    clipboardDataType: undefined,
    selector: "#paste-area",
    disabled: PASTE_MENU_ITEMS.concat(["node-menu-copyimagedatauri"]),
  },
  {
    desc: "<element> with whitespace only on clipboard",
    clipboardData: " \n\n\t\n\n  \n",
    clipboardDataType: undefined,
    selector: "#paste-area",
    disabled: PASTE_MENU_ITEMS.concat(["node-menu-copyimagedatauri"]),
  },
  {
    desc: "<element> that isn't visible on the page, empty clipboard",
    selector: "#hiddenElement",
    disabled: PASTE_MENU_ITEMS.concat([
      "node-menu-copyimagedatauri",
      "node-menu-screenshotnode",
    ]),
  },
  {
    desc: "<element> nested in another hidden element, empty clipboard",
    selector: "#nestedHiddenElement",
    disabled: PASTE_MENU_ITEMS.concat([
      "node-menu-copyimagedatauri",
      "node-menu-screenshotnode",
    ]),
  }
];

let clipboard = require("sdk/clipboard");
registerCleanupFunction(() => {
  clipboard = null;
});

add_task(function *() {
  let { inspector } = yield openInspectorForURL(TEST_URL);
  for (let test of TEST_CASES) {
    let { desc, disabled, selector } = test;

    info(`Test ${desc}`);
    setupClipboard(test.clipboardData, test.clipboardDataType);

    let front = yield getNodeFrontForSelector(selector, inspector);

    info("Selecting the specified node.");
    yield selectNode(front, inspector);

    info("Simulating context menu click on the selected node container.");
    contextMenuClick(getContainerForNodeFront(front, inspector).tagLine);

    for (let menuitem of ALL_MENU_ITEMS) {
      let elt = inspector.panelDoc.getElementById(menuitem);
      let shouldBeDisabled = disabled.indexOf(menuitem) !== -1;
      let isDisabled = elt.hasAttribute("disabled");

      is(isDisabled, shouldBeDisabled,
        `#${menuitem} should be ${shouldBeDisabled ? "disabled" : "enabled"} `);
    }
  }
});

/**
 * A helper that fetches a front for a node that matches the given selector or
 * doctype node if the selector is falsy.
 */
function* getNodeFrontForSelector(selector, inspector) {
  if (selector) {
    info("Retrieving front for selector " + selector);
    return getNodeFront(selector, inspector);
  } else {
    info("Retrieving front for doctype node");
    let {nodes} = yield inspector.walker.children(inspector.walker.rootNode);
    return nodes[0];
  }
}

/**
 * A helper that populates the clipboard with data of given type. Clears the
 * clipboard if data is falsy.
 */
function setupClipboard(data, type) {
  if (data) {
    info("Populating clipboard with " + type + " data.");
    clipboard.set(data, type);
  } else {
    info("Clearing clipboard.");
    clipboard.set("", "text");
  }
}

/**
 * A helper that simulates a contextmenu event on the given chrome DOM element.
 */
function contextMenuClick(element) {
  let evt = element.ownerDocument.createEvent('MouseEvents');
  let button = 2;  // right click

  evt.initMouseEvent('contextmenu', true, true,
       element.ownerDocument.defaultView, 1, 0, 0, 0, 0, false,
       false, false, false, button, null);

  element.dispatchEvent(evt);
}
