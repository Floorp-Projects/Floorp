/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../mochitest/value.js */
loadScripts({ name: "value.js", dir: MOCHITESTS_DIR });

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
const valueTests = [{
  desc: "Initially value is set to 1st element of select",
  id: "select",
  expected: "1st"
}, {
  desc: "Value should update to 3rd when 3 is pressed",
  id: "select",
  async action(browser) {
    await invokeFocus(browser, "select");
    await BrowserTestUtils.synthesizeKey("3", {}, browser);
  },
  waitFor: EVENT_TEXT_VALUE_CHANGE,
  expected: "3rd"
}, {
  desc: "Initially value is set to @aria-valuenow for slider",
  id: "slider",
  expected: ["5", 5, 0, 7, 0]
}, {
  desc: "Value should change when @aria-valuenow is updated",
  id: "slider",
  attrs: [{
    attr: "aria-valuenow",
    value: "6"
  }],
  waitFor: EVENT_VALUE_CHANGE,
  expected: ["6", 6, 0, 7, 0]
}, {
  desc: "Value should change when @aria-valuetext is set",
  id: "slider",
  attrs: [{
    attr: "aria-valuetext",
    value: "plain"
  }],
  waitFor: EVENT_TEXT_VALUE_CHANGE,
  expected: ["plain", 6, 0, 7, 0]
}, {
  desc: "Value should change when @aria-valuetext is updated",
  id: "slider",
  attrs: [{
    attr: "aria-valuetext",
    value: "hey!"
  }],
  waitFor: EVENT_TEXT_VALUE_CHANGE,
  expected: ["hey!", 6, 0, 7, 0]
}, {
  desc: "Value should change to @aria-valuetext when @aria-valuenow is removed",
  id: "slider",
  attrs: [{
    attr: "aria-valuenow"
  }],
  expected: ["hey!", 0, 0, 7, 0]
}, {
  desc: "Initially value is not set for combobox",
  id: "combobox",
  expected: ""
}, {
  desc: "Value should change when @value attribute is updated",
  id: "combobox",
  attrs: [{
    attr: "value",
    value: "hello"
  }],
  waitFor: EVENT_TEXT_VALUE_CHANGE,
  expected: "hello"
}, {
  desc: "Initially value corresponds to @value attribute for progress",
  id: "progress",
  expected: "22%"
}, {
  desc: "Value should change when @value attribute is updated",
  id: "progress",
  attrs: [{
    attr: "value",
    value: "50"
  }],
  waitFor: EVENT_VALUE_CHANGE,
  expected: "50%"
}, {
  desc: "Initially value corresponds to @value attribute for range",
  id: "range",
  expected: "6"
}, {
  desc: "Value should change when slider is moved",
  id: "range",
  async action(browser) {
    await invokeFocus(browser, "range");
    await BrowserTestUtils.synthesizeKey("VK_LEFT", {}, browser);
  },
  waitFor: EVENT_VALUE_CHANGE,
  expected: "5"
}];

/**
 * Test caching of accessible object values
 */
addAccessibleTask(`
  <div id="slider" role="slider" aria-valuenow="5"
       aria-valuemin="0" aria-valuemax="7">slider</div>
  <select id="select">
    <option>1st</option>
    <option>2nd</option>
    <option>3rd</option>
  </select>
  <input id="combobox" role="combobox" aria-autocomplete="inline">
  <progress id="progress" value="22" max="100"></progress>
  <input type="range" id="range" min="0" max="10" value="6">`,
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
  }
);
