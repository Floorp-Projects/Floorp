/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
requestLongerTimeout(2);

/* import-globals-from ../../mochitest/name.js */
loadScripts({ name: "name.js", dir: MOCHITESTS_DIR });

/**
 * Rules for name tests that are inspired by
 *   accessible/tests/mochitest/name/markuprules.xul
 *
 * Each element in the list of rules represents a name calculation rule for a
 * particular test case.
 *
 * The rules have the following format:
 *   { attr } - calculated from attribute
 *   { elm } - calculated from another element
 *   { fromsubtree } - calculated from element's subtree
 *
 */
const ARIARule = [{ attr: "aria-labelledby" }, { attr: "aria-label" }];
const HTMLControlHeadRule = [...ARIARule, { elm: "label" }];
const rules = {
  CSSContent: [{ elm: "style" }, { fromsubtree: true }],
  HTMLARIAGridCell: [...ARIARule, { fromsubtree: true }, { attr: "title" }],
  HTMLControl: [
    ...HTMLControlHeadRule,
    { fromsubtree: true },
    { attr: "title" },
  ],
  HTMLElm: [...ARIARule, { attr: "title" }],
  HTMLImg: [...ARIARule, { attr: "alt" }, { attr: "title" }],
  HTMLImgEmptyAlt: [...ARIARule, { attr: "title" }, { attr: "alt" }],
  HTMLInputButton: [
    ...HTMLControlHeadRule,
    { attr: "value" },
    { attr: "title" },
  ],
  HTMLInputImage: [
    ...HTMLControlHeadRule,
    { attr: "alt" },
    { attr: "value" },
    { attr: "title" },
  ],
  HTMLInputImageNoValidSrc: [
    ...HTMLControlHeadRule,
    { attr: "alt" },
    { attr: "value" },
  ],
  HTMLInputReset: [...HTMLControlHeadRule, { attr: "value" }],
  HTMLInputSubmit: [...HTMLControlHeadRule, { attr: "value" }],
  HTMLLink: [...ARIARule, { fromsubtree: true }, { attr: "title" }],
  HTMLLinkImage: [...ARIARule, { fromsubtree: true }, { attr: "title" }],
  HTMLOption: [
    ...ARIARule,
    { attr: "label" },
    { fromsubtree: true },
    { attr: "title" },
  ],
  HTMLTable: [
    ...ARIARule,
    { elm: "caption" },
    { attr: "summary" },
    { attr: "title" },
  ],
};

const markupTests = [
  {
    id: "btn",
    ruleset: "HTMLControl",
    markup: `
    <span id="l1">test2</span>
    <span id="l2">test3</span>
    <label for="btn">test4</label>
    <button id="btn"
            aria-label="test1"
            aria-labelledby="l1 l2"
            title="test5">press me</button>`,
    expected: ["test2 test3", "test1", "test4", "press me", "test5"],
  },
  {
    id: "btn",
    ruleset: "HTMLInputButton",
    markup: `
    <span id="l1">test2</span>
    <span id="l2">test3</span>
    <label for="btn">test4</label>
    <input id="btn"
           type="button"
           aria-label="test1"
           aria-labelledby="l1 l2"
           value="name from value"
           alt="no name from al"
           src="no name from src"
           data="no name from data"
           title="name from title"/>`,
    expected: [
      "test2 test3",
      "test1",
      "test4",
      "name from value",
      "name from title",
    ],
  },
  {
    id: "btn-submit",
    ruleset: "HTMLInputSubmit",
    markup: `
    <span id="l1">test2</span>
    <span id="l2">test3</span>
    <label for="btn-submit">test4</label>
    <input id="btn-submit"
           type="submit"
           aria-label="test1"
           aria-labelledby="l1 l2"
           value="name from value"
           alt="no name from atl"
           src="no name from src"
           data="no name from data"
           title="no name from title"/>`,
    expected: ["test2 test3", "test1", "test4", "name from value"],
  },
  {
    id: "btn-reset",
    ruleset: "HTMLInputReset",
    markup: `
    <span id="l1">test2</span>
    <span id="l2">test3</span>
    <label for="btn-reset">test4</label>
    <input id="btn-reset"
           type="reset"
           aria-label="test1"
           aria-labelledby="l1 l2"
           value="name from value"
           alt="no name from alt"
           src="no name from src"
           data="no name from data"
           title="no name from title"/>`,
    expected: ["test2 test3", "test1", "test4", "name from value"],
  },
  {
    id: "btn-image",
    ruleset: "HTMLInputImage",
    markup: `
    <span id="l1">test2</span>
    <span id="l2">test3</span>
    <label for="btn-image">test4</label>
    <input id="btn-image"
           type="image"
           aria-label="test1"
           aria-labelledby="l1 l2"
           alt="name from alt"
           value="name from value"
           src="http://example.com/a11y/accessible/tests/mochitest/moz.png"
           data="no name from data"
           title="name from title"/>`,
    expected: [
      "test2 test3",
      "test1",
      "test4",
      "name from alt",
      "name from value",
      "name from title",
    ],
  },
  {
    id: "btn-image",
    ruleset: "HTMLInputImageNoValidSrc",
    markup: `
    <span id="l1">test2</span>
    <span id="l2">test3</span>
    <label for="btn-image">test4</label>
    <input id="btn-image"
           type="image"
           aria-label="test1"
           aria-labelledby="l1 l2"
           alt="name from alt"
           value="name from value"
           data="no name from data"
           title="no name from title"/>`,
    expected: [
      "test2 test3",
      "test1",
      "test4",
      "name from alt",
      "name from value",
    ],
  },
  {
    id: "opt",
    ruleset: "HTMLOption",
    markup: `
    <span id="l1">test2</span>
    <span id="l2">test3</span>
    <select>
      <option id="opt"
              aria-label="test1"
              aria-labelledby="l1 l2"
              label="test4"
              title="test5">option1</option>
      <option>option2</option>
    </select>`,
    expected: ["test2 test3", "test1", "test4", "option1", "test5"],
  },
  {
    id: "img",
    ruleset: "HTMLImg",
    markup: `
    <span id="l1">test2</span>
    <span id="l2">test3</span>
    <img id="img"
         aria-label="Logo of Mozilla"
         aria-labelledby="l1 l2"
         alt="Mozilla logo"
         title="This is a logo"
         src="http://example.com/a11y/accessible/tests/mochitest/moz.png"/>`,
    expected: [
      "test2 test3",
      "Logo of Mozilla",
      "Mozilla logo",
      "This is a logo",
    ],
  },
  {
    id: "tc",
    ruleset: "HTMLElm",
    markup: `
    <span id="l1">test2</span>
    <span id="l2">test3</span>
    <label for="tc">test4</label>
    <table>
      <tr>
        <td id="tc"
            aria-label="test1"
            aria-labelledby="l1 l2"
            title="test5">
          <p>This is a paragraph</p>
          <a href="#">This is a link</a>
          <ul>
            <li>This is a list</li>
          </ul>
        </td>
      </tr>
    </table>`,
    expected: ["test2 test3", "test1", "test5"],
  },
  {
    id: "gc",
    ruleset: "HTMLARIAGridCell",
    markup: `
    <span id="l1">test2</span>
    <span id="l2">test3</span>
    <label for="gc">test4</label>
    <table>
      <tr>
        <td id="gc"
            role="gridcell"
            aria-label="test1"
            aria-labelledby="l1 l2"
            title="This is a paragraph This is a link This is a list">
          <p>This is a paragraph</p>
          <a href="#">This is a link</a>
          <ul>
            <li>Listitem1</li>
            <li>Listitem2</li>
          </ul>
        </td>
      </tr>
    </table>`,
    expected: [
      "test2 test3",
      "test1",
      "This is a paragraph This is a link \u2022 Listitem1 \u2022 Listitem2",
      "This is a paragraph This is a link This is a list",
    ],
  },
  {
    id: "t",
    ruleset: "HTMLTable",
    markup: `
    <span id="l1">lby_tst6_1</span>
    <span id="l2">lby_tst6_2</span>
    <label for="t">label_tst6</label>
    <table id="t"
           aria-label="arialabel_tst6"
           aria-labelledby="l1 l2"
           summary="summary_tst6"
           title="title_tst6">
      <caption>caption_tst6</caption>
      <tr>
        <td>cell1</td>
        <td>cell2</td>
      </tr>
    </table>`,
    expected: [
      "lby_tst6_1 lby_tst6_2",
      "arialabel_tst6",
      "caption_tst6",
      "summary_tst6",
      "title_tst6",
    ],
  },
  {
    id: "btn",
    ruleset: "CSSContent",
    markup: `
    <div role="main">
      <style>
        button::before {
          content: "do not ";
        }
      </style>
      <button id="btn">press me</button>
    </div>`,
    expected: ["do not press me", "press me"],
  },
  {
    // TODO: uncomment when Bug-1256382 is resoved.
    // id: 'li',
    // ruleset: 'CSSContent',
    // markup: `
    //   <style>
    //     ul {
    //       list-style-type: decimal;
    //     }
    //   </style>
    //   <ul id="ul">
    //     <li id="li">Listitem</li>
    //   </ul>`,
    // expected: ['1. Listitem', `${String.fromCharCode(0x2022)} Listitem`]
    // }, {
    id: "a",
    ruleset: "HTMLLink",
    markup: `
    <span id="l1">test2</span>
    <span id="l2">test3</span>
    <a id="a"
       href=""
       aria-label="test1"
       aria-labelledby="l1 l2"
       title="test4">test5</a>`,
    expected: ["test2 test3", "test1", "test5", "test4"],
  },
  {
    id: "a-img",
    ruleset: "HTMLLinkImage",
    markup: `
    <span id="l1">test2</span>
    <span id="l2">test3</span>
    <a id="a-img"
       href=""
       aria-label="test1"
       aria-labelledby="l1 l2"
       title="test4"><img alt="test5"/></a>`,
    expected: ["test2 test3", "test1", "test5", "test4"],
  },
];

/**
 * Test accessible name that is calculated from an attribute, remove the
 * attribute before proceeding to the next name test. If attribute removal
 * results in a reorder or text inserted event - wait for it. If accessible
 * becomes defunct, update its reference using the one that is attached to one
 * of the above events.
 * @param {Object} browser      current "tabbrowser" element
 * @param {Object} target       { acc, id } structure that contains an
 *                               accessible and its content element
 *                               id.
 * @param {Object} rule         current attr rule for name calculation
 * @param {[type]} expected     expected name value
 */
async function testAttrRule(browser, target, rule, expected) {
  let { id, acc } = target;
  let { attr } = rule;

  testName(acc, expected);

  let nameChange = waitForEvent(EVENT_NAME_CHANGE, id);
  await invokeContentTask(browser, [id, attr], (contentId, contentAttr) => {
    content.document.getElementById(contentId).removeAttribute(contentAttr);
  });
  let event = await nameChange;

  // Update accessible just in case it is now defunct.
  target.acc = findAccessibleChildByID(event.accessible, id);
}

/**
 * Test accessible name that is calculated from an element name, remove the
 * element before proceeding to the next name test. If element removal results
 * in a reorder event - wait for it. If accessible becomes defunct, update its
 * reference using the one that is attached to a possible reorder event.
 * @param {Object} browser      current "tabbrowser" element
 * @param {Object} target       { acc, id } structure that contains an
 *                               accessible and its content element
 *                               id.
 * @param {Object} rule         current elm rule for name calculation
 * @param {[type]} expected     expected name value
 */
async function testElmRule(browser, target, rule, expected) {
  let { id, acc } = target;
  let { elm } = rule;

  testName(acc, expected);
  let nameChange = waitForEvent(EVENT_NAME_CHANGE, id);

  await invokeContentTask(browser, [elm], contentElm => {
    content.document.querySelector(`${contentElm}`).remove();
  });
  let event = await nameChange;

  // Update accessible just in case it is now defunct.
  target.acc = findAccessibleChildByID(event.accessible, id);
}

/**
 * Test accessible name that is calculated from its subtree, remove the subtree
 * and wait for a reorder event before proceeding to the next name test. If
 * accessible becomes defunct, update its reference using the one that is
 * attached to a reorder event.
 * @param {Object} browser      current "tabbrowser" element
 * @param {Object} target       { acc, id } structure that contains an
 *                               accessible and its content element
 *                               id.
 * @param {Object} rule         current subtree rule for name calculation
 * @param {[type]} expected     expected name value
 */
async function testSubtreeRule(browser, target, rule, expected) {
  let { id, acc } = target;

  testName(acc, expected);
  let nameChange = waitForEvent(EVENT_NAME_CHANGE, id);

  await invokeContentTask(browser, [id], contentId => {
    let elm = content.document.getElementById(contentId);
    while (elm.firstChild) {
      elm.firstChild.remove();
    }
  });
  let event = await nameChange;

  // Update accessible just in case it is now defunct.
  target.acc = findAccessibleChildByID(event.accessible, id);
}

/**
 * Iterate over a list of rules and test accessible names for each one of the
 * rules.
 * @param {Object} browser      current "tabbrowser" element
 * @param {Object} target       { acc, id } structure that contains an
 *                               accessible and its content element
 *                               id.
 * @param {Array}  ruleset      A list of rules to test a target with
 * @param {Array}  expected     A list of expected name value for each rule
 */
async function testNameRule(browser, target, ruleset, expected) {
  for (let i = 0; i < ruleset.length; ++i) {
    let rule = ruleset[i];
    let testFn;
    if (rule.attr) {
      testFn = testAttrRule;
    } else if (rule.elm) {
      testFn = testElmRule;
    } else if (rule.fromsubtree) {
      testFn = testSubtreeRule;
    }
    await testFn(browser, target, rule, expected[i]);
  }
}

markupTests.forEach(({ id, ruleset, markup, expected }) =>
  addAccessibleTask(
    markup,
    async function (browser, accDoc) {
      const observer = {
        observe(subject, topic, data) {
          const event = subject.QueryInterface(nsIAccessibleEvent);
          console.log(eventToString(event));
        },
      };
      Services.obs.addObserver(observer, "accessible-event");
      // Find a target accessible from an accessible subtree.
      let acc = findAccessibleChildByID(accDoc, id);
      let target = { id, acc };
      await testNameRule(browser, target, rules[ruleset], expected);
      Services.obs.removeObserver(observer, "accessible-event");
    },
    { iframe: true, remoteIframe: true }
  )
);

/**
 * Test caching of the document title.
 */
addAccessibleTask(
  ``,
  async function (browser, docAcc) {
    let nameChanged = waitForEvent(EVENT_NAME_CHANGE, docAcc);
    await invokeContentTask(browser, [], () => {
      content.document.title = "new title";
    });
    await nameChanged;
    testName(docAcc, "new title");
  },
  { chrome: true, topLevel: true, iframe: true, remoteIframe: true }
);

/**
 * Test that the name is updated when the content of a hidden aria-labelledby
 * subtree changes.
 */
addAccessibleTask(
  `
<button id="button" aria-labelledby="label">
<div id="label" hidden>a</div>
  `,
  async function (browser, docAcc) {
    const button = findAccessibleChildByID(docAcc, "button");
    testName(button, "a");
    info("Changing label textContent");
    let nameChanged = waitForEvent(EVENT_NAME_CHANGE, button);
    await invokeContentTask(browser, [], () => {
      content.document.getElementById("label").textContent = "c";
    });
    await nameChanged;
    testName(button, "c");
    info("Prepending text node to label");
    nameChanged = waitForEvent(EVENT_NAME_CHANGE, button);
    await invokeContentTask(browser, [], () => {
      content.document
        .getElementById("label")
        .prepend(content.document.createTextNode("b"));
    });
    await nameChanged;
    testName(button, "bc");
  },
  { chrome: true, topLevel: true, iframe: true, remoteIframe: true }
);
