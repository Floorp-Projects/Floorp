/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../mochitest/attributes.js */
loadScripts({ name: "attributes.js", dir: MOCHITESTS_DIR });

const isCacheEnabled = Services.prefs.getBoolPref(
  "accessibility.cache.enabled",
  false
);

/**
 * Default textbox accessible attributes.
 */
const defaultAttributes = {
  "margin-top": "0px",
  "margin-right": "0px",
  "margin-bottom": "0px",
  "margin-left": "0px",
  "text-align": "start",
  "text-indent": "0px",
  id: "textbox",
  tag: "input",
  display: "inline-block",
};

/**
 * Test data has the format of:
 * {
 *   desc        {String}         description for better logging
 *   expected    {Object}         expected attributes for given accessibles
 *   unexpected  {Object}         unexpected attributes for given accessibles
 *
 *   action      {?AsyncFunction} an optional action that awaits a change in
 *                                attributes
 *   attrs       {?Array}         an optional list of attributes to update
 *   waitFor     {?Number}        an optional event to wait for
 * }
 */
const attributesTests = [
  {
    desc: "Initiall accessible attributes",
    expected: defaultAttributes,
    unexpected: {
      "line-number": "1",
      "explicit-name": "true",
      "container-live": "polite",
      live: "polite",
    },
  },
  {
    desc: "@line-number attribute is present when textbox is focused",
    async action(browser) {
      await invokeFocus(browser, "textbox");
    },
    waitFor: EVENT_FOCUS,
    expected: Object.assign({}, defaultAttributes, { "line-number": "1" }),
    unexpected: {
      "explicit-name": "true",
      "container-live": "polite",
      live: "polite",
    },
  },
  {
    desc: "@aria-live sets container-live and live attributes",
    attrs: [
      {
        attr: "aria-live",
        value: "polite",
      },
    ],
    expected: Object.assign({}, defaultAttributes, {
      "line-number": "1",
      "container-live": "polite",
      live: "polite",
    }),
    unexpected: {
      "explicit-name": "true",
    },
  },
  {
    desc: "@title attribute sets explicit-name attribute to true",
    attrs: [
      {
        attr: "title",
        value: "textbox",
      },
    ],
    expected: Object.assign({}, defaultAttributes, {
      "line-number": "1",
      "explicit-name": "true",
      "container-live": "polite",
      live: "polite",
    }),
    unexpected: {},
  },
];

/**
 * Test caching of accessible object attributes
 */
addAccessibleTask(
  `
  <input id="textbox" value="hello">`,
  async function(browser, accDoc) {
    let textbox = findAccessibleChildByID(accDoc, "textbox");
    for (let {
      desc,
      action,
      attrs,
      expected,
      waitFor,
      unexpected,
    } of attributesTests) {
      info(desc);
      let onUpdate;

      if (waitFor) {
        onUpdate = waitForEvent(waitFor, "textbox");
      }

      if (action) {
        await action(browser);
      } else if (attrs) {
        for (let { attr, value } of attrs) {
          await invokeSetAttribute(browser, "textbox", attr, value);
        }
      }

      await onUpdate;
      testAttrs(textbox, expected);
      testAbsentAttrs(textbox, unexpected);
    }
  },
  {
    // These tests don't work yet with the parent process cache enabled.
    topLevel: !isCacheEnabled,
    iframe: !isCacheEnabled,
    remoteIframe: !isCacheEnabled,
  }
);

/**
 * Test caching of the tag attribute.
 */
addAccessibleTask(
  `
<p id="p">text</p>
<textarea id="textarea"></textarea>
  `,
  async function(browser, docAcc) {
    testAttrs(docAcc, { tag: "body" }, true);
    const p = findAccessibleChildByID(docAcc, "p");
    testAttrs(p, { tag: "p" }, true);
    const textLeaf = p.firstChild;
    testAbsentAttrs(textLeaf, { tag: "" });
    const textarea = findAccessibleChildByID(docAcc, "textarea");
    testAttrs(textarea, { tag: "textarea" }, true);
  },
  { chrome: true, topLevel: true, iframe: true, remoteIframe: true }
);

/**
 * Test caching of the text-input-type attribute.
 */
addAccessibleTask(
  `
  <input id="default">
  <input id="email" type="email">
  <input id="password" type="password">
  <input id="text" type="text">
  <input id="date" type="date">
  <input id="time" type="time">
  <input id="checkbox" type="checkbox">
  <input id="radio" type="radio">
  `,
  async function(browser, docAcc) {
    function testInputType(id, inputType) {
      if (inputType == undefined) {
        testAbsentAttrs(findAccessibleChildByID(docAcc, id), {
          "text-input-type": "",
        });
      } else {
        testAttrs(
          findAccessibleChildByID(docAcc, id),
          { "text-input-type": inputType },
          true
        );
      }
    }

    testInputType("default");
    testInputType("email", "email");
    testInputType("password", "password");
    testInputType("text", "text");
    testInputType("date", "date");
    testInputType("time", "time");
    testInputType("checkbox");
    testInputType("radio");
  },
  { chrome: true, topLevel: true, iframe: false, remoteIframe: false }
);
