/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../mochitest/role.js */
/* import-globals-from ../../mochitest/states.js */
loadScripts(
  { name: "role.js", dir: MOCHITESTS_DIR },
  { name: "states.js", dir: MOCHITESTS_DIR }
);

/**
 * Test data has the format of:
 * {
 *   desc      {String}   description for better logging
 *   expected  {Array}    expected states for a given accessible that have the
 *                        following format:
 *                          [
 *                            expected state,
 *                            expected extra state,
 *                            absent state,
 *                            absent extra state
 *                          ]
 *   attrs     {?Array}   an optional list of attributes to update
 * }
 */

// State caching tests for attribute changes
const attributeTests = [
  {
    desc:
      "Checkbox with @checked attribute set to true should have checked " +
      "state",
    attrs: [
      {
        attr: "checked",
        value: "true",
      },
    ],
    expected: [STATE_CHECKED, 0],
  },
  {
    desc: "Checkbox with no @checked attribute should not have checked state",
    attrs: [
      {
        attr: "checked",
      },
    ],
    expected: [0, 0, STATE_CHECKED],
  },
];

// State caching tests for ARIA changes
const ariaTests = [
  {
    desc: "File input has busy state when @aria-busy attribute is set to true",
    attrs: [
      {
        attr: "aria-busy",
        value: "true",
      },
    ],
    expected: [STATE_BUSY, 0, STATE_REQUIRED | STATE_INVALID],
  },
  {
    desc:
      "File input has required state when @aria-required attribute is set " +
      "to true",
    attrs: [
      {
        attr: "aria-required",
        value: "true",
      },
    ],
    expected: [STATE_REQUIRED, 0, STATE_INVALID],
  },
  {
    desc:
      "File input has invalid state when @aria-invalid attribute is set to " +
      "true",
    attrs: [
      {
        attr: "aria-invalid",
        value: "true",
      },
    ],
    expected: [STATE_INVALID, 0],
  },
];

// Extra state caching tests
const extraStateTests = [
  {
    desc:
      "Input has no extra enabled state when aria and native disabled " +
      "attributes are set at once",
    attrs: [
      {
        attr: "aria-disabled",
        value: "true",
      },
      {
        attr: "disabled",
        value: "true",
      },
    ],
    expected: [0, 0, 0, EXT_STATE_ENABLED],
  },
  {
    desc:
      "Input has an extra enabled state when aria and native disabled " +
      "attributes are unset at once",
    attrs: [
      {
        attr: "aria-disabled",
      },
      {
        attr: "disabled",
      },
    ],
    expected: [0, EXT_STATE_ENABLED],
  },
];

async function runStateTests(browser, accDoc, id, tests) {
  let acc = findAccessibleChildByID(accDoc, id);
  for (let { desc, attrs, expected } of tests) {
    const [expState, expExtState, absState, absExtState] = expected;
    info(desc);
    let onUpdate = waitForEvent(EVENT_STATE_CHANGE, evt => {
      if (getAccessibleDOMNodeID(evt.accessible) != id) {
        return false;
      }
      // Events can be fired for states other than the ones we're interested
      // in. If this happens, the states we're expecting might not be exposed
      // yet.
      const scEvt = evt.QueryInterface(nsIAccessibleStateChangeEvent);
      if (scEvt.isExtraState) {
        if (scEvt.state & expExtState || scEvt.state & absExtState) {
          return true;
        }
        return false;
      }
      return scEvt.state & expState || scEvt.state & absState;
    });
    for (let { attr, value } of attrs) {
      await invokeSetAttribute(browser, id, attr, value);
    }
    await onUpdate;
    testStates(acc, ...expected);
  }
}

/**
 * Test caching of accessible object states
 */
addAccessibleTask(
  `
  <input id="checkbox" type="checkbox">
  <input id="file" type="file">
  <input id="text">`,
  async function(browser, accDoc) {
    await runStateTests(browser, accDoc, "checkbox", attributeTests);
    await runStateTests(browser, accDoc, "file", ariaTests);
    await runStateTests(browser, accDoc, "text", extraStateTests);
  },
  { iframe: true, remoteIframe: true }
);

/**
 * Test caching of the focused state.
 */
addAccessibleTask(
  `
  <button id="b1">b1</button>
  <button id="b2">b2</button>
  `,
  async function(browser, docAcc) {
    const b1 = findAccessibleChildByID(docAcc, "b1");
    const b2 = findAccessibleChildByID(docAcc, "b2");

    let focused = waitForEvent(EVENT_FOCUS, b1);
    await invokeFocus(browser, "b1");
    await focused;
    testStates(docAcc, 0, 0, STATE_FOCUSED);
    testStates(b1, STATE_FOCUSED);
    testStates(b2, 0, 0, STATE_FOCUSED);

    focused = waitForEvent(EVENT_FOCUS, b2);
    await invokeFocus(browser, "b2");
    await focused;
    testStates(b2, STATE_FOCUSED);
    testStates(b1, 0, 0, STATE_FOCUSED);
  },
  { iframe: true, remoteIframe: true }
);

/**
 * Test that the document initially gets the focused state.
 * We can't do this in the test above because that test runs in iframes as well
 * as a top level document.
 */
addAccessibleTask(
  `
  <button id="b1">b1</button>
  <button id="b2">b2</button>
  `,
  async function(browser, docAcc) {
    testStates(docAcc, STATE_FOCUSED);
  }
);

function checkOpacity(acc, present) {
  // eslint-disable-next-line no-unused-vars
  let [_, extraState] = getStates(acc);
  let currOpacity = extraState & EXT_STATE_OPAQUE;
  return present ? currOpacity : !currOpacity;
}

/**
 * Test caching of the OPAQUE1 state.
 */
addAccessibleTask(
  `
  <div id="div">hello world</div>
  `,
  async function(browser, docAcc) {
    const div = findAccessibleChildByID(docAcc, "div");
    await untilCacheOk(() => checkOpacity(div, true), "Found opaque state");

    await invokeContentTask(browser, [], () => {
      let elm = content.document.getElementById("div");
      elm.style = "opacity: 0.4;";
    });

    await untilCacheOk(
      () => checkOpacity(div, false),
      "Did not find opaque state"
    );

    await invokeContentTask(browser, [], () => {
      let elm = content.document.getElementById("div");
      elm.style = "opacity: 1;";
    });

    await untilCacheOk(() => checkOpacity(div, true), "Found opaque state");
  },
  { iframe: true, remoteIframe: true, chrome: true }
);

/**
 * Test caching of the editable state.
 */
addAccessibleTask(
  `<div id="div" contenteditable></div>`,
  async function(browser, docAcc) {
    const div = findAccessibleChildByID(docAcc, "div");
    testStates(div, 0, EXT_STATE_EDITABLE, 0, 0);
    // Ensure that a contentEditable descendant doesn't cause editable to be
    // exposed on the document.
    testStates(docAcc, STATE_READONLY, 0, 0, EXT_STATE_EDITABLE);

    info("Setting contentEditable on the body");
    let stateChanged = Promise.all([
      waitForStateChange(docAcc, EXT_STATE_EDITABLE, true, true),
      waitForStateChange(docAcc, STATE_READONLY, false, false),
    ]);
    await invokeContentTask(browser, [], () => {
      content.document.body.contentEditable = true;
    });
    await stateChanged;
    testStates(docAcc, 0, EXT_STATE_EDITABLE, STATE_READONLY, 0);

    info("Clearing contentEditable on the body");
    stateChanged = Promise.all([
      waitForStateChange(docAcc, EXT_STATE_EDITABLE, false, true),
      waitForStateChange(docAcc, STATE_READONLY, true, false),
    ]);
    await invokeContentTask(browser, [], () => {
      content.document.body.contentEditable = false;
    });
    await stateChanged;
    testStates(docAcc, STATE_READONLY, 0, 0, EXT_STATE_EDITABLE);

    info("Clearing contentEditable on div");
    stateChanged = waitForStateChange(div, EXT_STATE_EDITABLE, false, true);
    await invokeContentTask(browser, [], () => {
      content.document.getElementById("div").contentEditable = false;
    });
    await stateChanged;
    testStates(div, 0, 0, 0, EXT_STATE_EDITABLE);

    info("Setting contentEditable on div");
    stateChanged = waitForStateChange(div, EXT_STATE_EDITABLE, true, true);
    await invokeContentTask(browser, [], () => {
      content.document.getElementById("div").contentEditable = true;
    });
    await stateChanged;
    testStates(div, 0, EXT_STATE_EDITABLE, 0, 0);

    info("Setting designMode on document");
    stateChanged = Promise.all([
      waitForStateChange(docAcc, EXT_STATE_EDITABLE, true, true),
      waitForStateChange(docAcc, STATE_READONLY, false, false),
    ]);
    await invokeContentTask(browser, [], () => {
      content.document.designMode = "on";
    });
    await stateChanged;
    testStates(docAcc, 0, EXT_STATE_EDITABLE, STATE_READONLY, 0);

    info("Clearing designMode on document");
    stateChanged = Promise.all([
      waitForStateChange(docAcc, EXT_STATE_EDITABLE, false, true),
      waitForStateChange(docAcc, STATE_READONLY, true, false),
    ]);
    await invokeContentTask(browser, [], () => {
      content.document.designMode = "off";
    });
    await stateChanged;
    testStates(docAcc, STATE_READONLY, 0, 0, EXT_STATE_EDITABLE);
  },
  { topLevel: true, iframe: true, remoteIframe: true, chrome: true }
);
