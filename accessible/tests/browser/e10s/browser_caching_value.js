/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../mochitest/states.js */
/* import-globals-from ../../mochitest/value.js */
loadScripts(
  { name: "states.js", dir: MOCHITESTS_DIR },
  { name: "value.js", dir: MOCHITESTS_DIR }
);

/**
 * Test data has the format of:
 * {
 *   desc      {String}            description for better logging
 *   id        {String}            given accessible DOMNode ID
 *   expected  {String}            expected value for a given accessible
 *   action    {?AsyncFunction}    an optional action that awaits a value change
 *   attrs     {?Array}            an optional list of attributes to update
 *   waitFor   {?Number}           an optional value change event to wait for
 * }
 */
const valueTests = [
  {
    desc: "Initially value is set to 1st element of select",
    id: "select",
    expected: "1st",
  },
  {
    desc: "Value should update to 3rd when 3 is pressed",
    id: "select",
    async action(browser) {
      await invokeFocus(browser, "select");
      await invokeContentTask(browser, [], () => {
        const { ContentTaskUtils } = ChromeUtils.import(
          "resource://testing-common/ContentTaskUtils.jsm"
        );
        const EventUtils = ContentTaskUtils.getEventUtils(content);
        EventUtils.synthesizeKey("3", {}, content);
      });
    },
    waitFor: EVENT_TEXT_VALUE_CHANGE,
    expected: "3rd",
  },
  {
    desc: "Initially value is set to @aria-valuenow for slider",
    id: "slider",
    expected: ["5", 5, 0, 7, 0],
  },
  {
    desc: "Value should change when @aria-valuenow is updated",
    id: "slider",
    attrs: [
      {
        attr: "aria-valuenow",
        value: "6",
      },
    ],
    waitFor: EVENT_VALUE_CHANGE,
    expected: ["6", 6, 0, 7, 0],
  },
  {
    desc: "Value should change when @aria-valuetext is set",
    id: "slider",
    attrs: [
      {
        attr: "aria-valuetext",
        value: "plain",
      },
    ],
    waitFor: EVENT_TEXT_VALUE_CHANGE,
    expected: ["plain", 6, 0, 7, 0],
  },
  {
    desc: "Value should change when @aria-valuetext is updated",
    id: "slider",
    attrs: [
      {
        attr: "aria-valuetext",
        value: "hey!",
      },
    ],
    waitFor: EVENT_TEXT_VALUE_CHANGE,
    expected: ["hey!", 6, 0, 7, 0],
  },
  {
    desc:
      "Value should change to @aria-valuetext when @aria-valuenow is removed",
    id: "slider",
    attrs: [
      {
        attr: "aria-valuenow",
      },
    ],
    expected: ["hey!", 3.5, 0, 7, 0],
  },
  {
    desc: "Initially value is not set for combobox",
    id: "combobox",
    expected: "",
  },
  {
    desc: "Value should change when @value attribute is updated",
    id: "combobox",
    attrs: [
      {
        attr: "value",
        value: "hello",
      },
    ],
    waitFor: EVENT_TEXT_VALUE_CHANGE,
    expected: "hello",
  },
  {
    desc: "Initially value corresponds to @value attribute for progress",
    id: "progress",
    expected: "22%",
  },
  {
    desc: "Value should change when @value attribute is updated",
    id: "progress",
    attrs: [
      {
        attr: "value",
        value: "50",
      },
    ],
    waitFor: EVENT_VALUE_CHANGE,
    expected: "50%",
  },
  {
    desc: "Initially value corresponds to @value attribute for range",
    id: "range",
    expected: "6",
  },
  {
    desc: "Value should change when slider is moved",
    id: "range",
    async action(browser) {
      await invokeFocus(browser, "range");
      await invokeContentTask(browser, [], () => {
        const { ContentTaskUtils } = ChromeUtils.import(
          "resource://testing-common/ContentTaskUtils.jsm"
        );
        const EventUtils = ContentTaskUtils.getEventUtils(content);
        EventUtils.synthesizeKey("VK_LEFT", {}, content);
      });
    },
    waitFor: EVENT_VALUE_CHANGE,
    expected: "5",
  },
  {
    desc: "Initially textbox value is text subtree",
    id: "textbox",
    expected: "Some rich text",
  },
  {
    desc: "Textbox value changes when subtree changes",
    id: "textbox",
    async action(browser) {
      await invokeContentTask(browser, [], () => {
        let boldText = content.document.createElement("strong");
        boldText.textContent = " bold";
        content.document.getElementById("textbox").appendChild(boldText);
      });
    },
    waitFor: EVENT_TEXT_VALUE_CHANGE,
    expected: "Some rich text bold",
  },
];

/**
 * Test caching of accessible object values
 */
addAccessibleTask(
  `
  <div id="slider" role="slider" aria-valuenow="5"
       aria-valuemin="0" aria-valuemax="7">slider</div>
  <select id="select">
    <option>1st</option>
    <option>2nd</option>
    <option>3rd</option>
  </select>
  <input id="combobox" role="combobox" aria-autocomplete="inline">
  <progress id="progress" value="22" max="100"></progress>
  <input type="range" id="range" min="0" max="10" value="6">
  <div contenteditable="yes" role="textbox" id="textbox">Some <a href="#">rich</a> text</div>`,
  async function(browser, accDoc) {
    for (let { desc, id, action, attrs, expected, waitFor } of valueTests) {
      info(desc);
      let acc = findAccessibleChildByID(accDoc, id);
      let onUpdate;

      if (waitFor) {
        onUpdate = waitForEvent(waitFor, id);
      }

      if (action) {
        await action(browser);
      } else if (attrs) {
        for (let { attr, value } of attrs) {
          await invokeSetAttribute(browser, id, attr, value);
        }
      }

      await onUpdate;
      if (Array.isArray(expected)) {
        acc.QueryInterface(nsIAccessibleValue);
        testValue(acc, ...expected);
      } else {
        is(acc.value, expected, `Correct value for ${prettyName(acc)}`);
      }
    }
  },
  { iframe: true, remoteIframe: true }
);

/**
 * Test caching of link URL values.
 */
addAccessibleTask(
  `<a id="link" href="https://example.com/">Test</a>`,
  async function(browser, docAcc) {
    const link = findAccessibleChildByID(docAcc, "link");
    is(link.value, "https://example.com/", "link initial value correct");
    const textLeaf = link.firstChild;
    is(textLeaf.value, "https://example.com/", "link initial value correct");

    info("Changing link href");
    await invokeSetAttribute(browser, "link", "href", "https://example.net/");
    await untilCacheIs(
      () => link.value,
      "https://example.net/",
      "link value correct after change"
    );

    info("Removing link href");
    await invokeSetAttribute(browser, "link", "href");
    await untilCacheIs(() => link.value, "", "link value empty after removal");

    info("Setting link href");
    await invokeSetAttribute(browser, "link", "href", "https://example.com/");
    await untilCacheIs(
      () => link.value,
      "https://example.com/",
      "link value correct after change"
    );
  },
  { chrome: true, topLevel: true, iframe: true, remoteIframe: true }
);

/**
 * Test caching of active state for select options - see bug 1788143.
 */
addAccessibleTask(
  `
  <select id="select">
    <option id="first_option">First</option>
    <option id="second_option">Second</option>
  </select>`,
  async function(browser, docAcc) {
    const select = findAccessibleChildByID(docAcc, "select");
    is(select.value, "First", "Select initial value correct");

    // Focus the combo box.
    await invokeFocus(browser, "select");

    // Select the second option (drop-down collapsed).
    let p = waitForEvents({
      expected: [
        [EVENT_SELECTION, "second_option"],
        [EVENT_TEXT_VALUE_CHANGE, "select"],
      ],
      unexpected: [
        stateChangeEventArgs("second_option", EXT_STATE_ACTIVE, true, true),
        stateChangeEventArgs("first_option", EXT_STATE_ACTIVE, false, true),
      ],
    });
    await invokeContentTask(browser, [], () => {
      content.document.getElementById("select").selectedIndex = 1;
    });
    await p;

    is(select.value, "Second", "Select value correct after changing option");

    // Expand the combobox dropdown.
    p = waitForEvent(EVENT_STATE_CHANGE, "ContentSelectDropdown");
    EventUtils.synthesizeKey("VK_SPACE");
    await p;

    p = waitForEvents({
      expected: [
        [EVENT_SELECTION, "first_option"],
        [EVENT_TEXT_VALUE_CHANGE, "select"],
        [EVENT_HIDE, "ContentSelectDropdown"],
      ],
      unexpected: [
        stateChangeEventArgs("first_option", EXT_STATE_ACTIVE, true, true),
        stateChangeEventArgs("second_option", EXT_STATE_ACTIVE, false, true),
      ],
    });

    // Press the up arrow to select the first option (drop-down expanded).
    // Then, press Enter to confirm the selection and close the dropdown.
    // We do both of these together to unify testing across platforms, since
    // events are not entirely consistent on Windows vs. Linux + macOS.
    EventUtils.synthesizeKey("VK_UP");
    EventUtils.synthesizeKey("VK_RETURN");
    await p;

    is(
      select.value,
      "First",
      "Select value correct after changing option back"
    );
  },
  { chrome: true, topLevel: true, iframe: true, remoteIFrame: true }
);
