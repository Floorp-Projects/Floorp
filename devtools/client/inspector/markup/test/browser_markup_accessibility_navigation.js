/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* global getContainerForSelector, openInspectorForURL */

// Test keyboard navigation accessibility of inspector's markup view.

/**
 * Test data has the format of:
 * {
 *   desc              {String}   description for better logging
 *   key               {String}   key event's key
 *   options           {?Object}  optional event data such as shiftKey, etc
 *   focused           {String}   path to expected focused element relative to
 *                                its container
 *   activedescendant  {String}   path to expected aria-activedescendant element
 *                                relative to its container
 *   waitFor           {String}   optional event to wait for if keyboard actions
 *                                result in asynchronous updates
 * }
 */
const TESTS = [
  {
    desc: "Collapse body container",
    focused: "root.elt",
    activedescendant: "body.tagLine",
    key: "VK_LEFT",
    options: { },
    waitFor: "collapsed"
  },
  {
    desc: "Expand body container",
    focused: "root.elt",
    activedescendant: "body.tagLine",
    key: "VK_RIGHT",
    options: { },
    waitFor: "expanded"
  },
  {
    desc: "Select header container",
    focused: "root.elt",
    activedescendant: "header.tagLine",
    key: "VK_DOWN",
    options: { },
    waitFor: "inspector-updated"
  },
  {
    desc: "Expand header container",
    focused: "root.elt",
    activedescendant: "header.tagLine",
    key: "VK_RIGHT",
    options: { },
    waitFor: "expanded"
  },
  {
    desc: "Select text container",
    focused: "root.elt",
    activedescendant: "container-0.tagLine",
    key: "VK_DOWN",
    options: { },
    waitFor: "inspector-updated"
  },
  {
    desc: "Select header container again",
    focused: "root.elt",
    activedescendant: "header.tagLine",
    key: "VK_UP",
    options: { },
    waitFor: "inspector-updated"
  },
  {
    desc: "Collapse header container",
    focused: "root.elt",
    activedescendant: "header.tagLine",
    key: "VK_LEFT",
    options: { },
    waitFor: "collapsed"
  },
  {
    desc: "Focus on header container tag",
    focused: "header.focusableElms.0",
    activedescendant: "header.tagLine",
    key: "VK_RETURN",
    options: { }
  },
  {
    desc: "Remove focus from header container tag",
    focused: "root.elt",
    activedescendant: "header.tagLine",
    key: "VK_ESCAPE",
    options: { }
  },
  {
    desc: "Focus on header container tag again",
    focused: "header.focusableElms.0",
    activedescendant: "header.tagLine",
    key: "VK_SPACE",
    options: { }
  },
  {
    desc: "Focus on header id attribute",
    focused: "header.focusableElms.1",
    activedescendant: "header.tagLine",
    key: "VK_TAB",
    options: { }
  },
  {
    desc: "Focus on header class attribute",
    focused: "header.focusableElms.2",
    activedescendant: "header.tagLine",
    key: "VK_TAB",
    options: { }
  },
  {
    desc: "Focus on header new attribute",
    focused: "header.focusableElms.3",
    activedescendant: "header.tagLine",
    key: "VK_TAB",
    options: { }
  },
  {
    desc: "Circle back and focus on header tag again",
    focused: "header.focusableElms.0",
    activedescendant: "header.tagLine",
    key: "VK_TAB",
    options: { }
  },
  {
    desc: "Circle back and focus on header new attribute again",
    focused: "header.focusableElms.3",
    activedescendant: "header.tagLine",
    key: "VK_TAB",
    options: { shiftKey: true }
  },
  {
    desc: "Tab back and focus on header class attribute",
    focused: "header.focusableElms.2",
    activedescendant: "header.tagLine",
    key: "VK_TAB",
    options: { shiftKey: true }
  },
  {
    desc: "Tab back and focus on header id attribute",
    focused: "header.focusableElms.1",
    activedescendant: "header.tagLine",
    key: "VK_TAB",
    options: { shiftKey: true }
  },
  {
    desc: "Tab back and focus on header tag",
    focused: "header.focusableElms.0",
    activedescendant: "header.tagLine",
    key: "VK_TAB",
    options: { shiftKey: true }
  },
  {
    desc: "Expand header container, ensure that focus is still on header tag",
    focused: "header.focusableElms.0",
    activedescendant: "header.tagLine",
    key: "VK_RIGHT",
    options: { },
    waitFor: "expanded"
  },
  {
    desc: "Activate header tag editor",
    focused: "header.editor.tag.inplaceEditor.input",
    activedescendant: "header.tagLine",
    key: "VK_RETURN",
    options: { }
  },
  {
    desc: "Activate header id attribute editor",
    focused: "header.editor.attrList.children.0.children.1.inplaceEditor.input",
    activedescendant: "header.tagLine",
    key: "VK_TAB",
    options: { }
  },
  {
    desc: "Deselect text in header id attribute editor",
    focused: "header.editor.attrList.children.0.children.1.inplaceEditor.input",
    activedescendant: "header.tagLine",
    key: "VK_TAB",
    options: { }
  },
  {
    desc: "Activate header class attribute editor",
    focused: "header.editor.attrList.children.1.children.1.inplaceEditor.input",
    activedescendant: "header.tagLine",
    key: "VK_TAB",
    options: { }
  },
  {
    desc: "Deselect text in header class attribute editor",
    focused: "header.editor.attrList.children.1.children.1.inplaceEditor.input",
    activedescendant: "header.tagLine",
    key: "VK_TAB",
    options: { }
  },
  {
    desc: "Activate header new attribute editor",
    focused: "header.editor.newAttr.inplaceEditor.input",
    activedescendant: "header.tagLine",
    key: "VK_TAB",
    options: { }
  },
  {
    desc: "Circle back and activate header tag editor again",
    focused: "header.editor.tag.inplaceEditor.input",
    activedescendant: "header.tagLine",
    key: "VK_TAB",
    options: { }
  },
  {
    desc: "Circle back and activate header new attribute editor again",
    focused: "header.editor.newAttr.inplaceEditor.input",
    activedescendant: "header.tagLine",
    key: "VK_TAB",
    options: { shiftKey: true }
  },
  {
    desc: "Exit edit mode and keep focus on header new attribute",
    focused: "header.focusableElms.3",
    activedescendant: "header.tagLine",
    key: "VK_ESCAPE",
    options: { }
  },
  {
    desc: "Move the selection to body and reset focus to container tree",
    focused: "docBody",
    activedescendant: "body.tagLine",
    key: "VK_UP",
    options: { },
    waitFor: "inspector-updated"
  },
];

let elms = {};
let containerID = 0;

add_task(function* () {
  let { inspector } = yield openInspectorForURL(`data:text/html;charset=utf-8,
    <h1 id="some-id" class="some-class">foo<span>Child span<span></h1>`);
  let markup = inspector.markup;
  let doc = markup.doc;
  let win = doc.defaultView;

  // Record containers that are created after inspector is initialized to be
  // useful in testing.
  inspector.on("container-created", memorizeContainer);
  registerCleanupFunction(() => {
    inspector.off("container-created", memorizeContainer);
  });

  elms.docBody = doc.body;
  elms.root = markup.getContainer(markup._rootNode);
  elms.header = yield getContainerForSelector("h1", inspector);
  elms.body = yield getContainerForSelector("body", inspector);

  // Initial focus is on root element and active descendant should be set on
  // body tag line.
  testNavigationState(doc, elms.docBody, elms.body.tagLine);

  // Focus on the tree element.
  elms.root.elt.focus();

  for (let {desc, waitFor, focused, activedescendant, key, options} of TESTS) {
    info(desc);
    let updated;
    if (waitFor) {
      updated = waitFor === "inspector-updated" ?
        inspector.once(waitFor) : markup.once(waitFor);
    } else {
      updated = Promise.resolve();
    }

    EventUtils.synthesizeKey(key, options, win);
    yield updated;
    testNavigationState(doc, getElm(focused), getElm(activedescendant));
  }
});

// Record all containers that are created dynamically into elms object.
function memorizeContainer(event, container) {
  elms[`container-${containerID++}`] = container;
}

// Parse and lookup an element from elms object based on dotted path.
function getElm(path) {
  let segments = path.split(".");
  return segments.reduce((prev, current) => prev[current], elms);
}

function testNavigationState(doc, focused, activedescendant) {
  let id = activedescendant.getAttribute("id");
  is(doc.activeElement, focused, `Keyboard focus should be set to ${focused}`);
  is(elms.root.elt.getAttribute("aria-activedescendant"), id,
    `Active descendant should be set to ${id}`);
}
