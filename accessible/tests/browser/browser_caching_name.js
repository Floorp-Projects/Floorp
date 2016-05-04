/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

/* global EVENT_REORDER, EVENT_TEXT_INSERTED */

loadScripts({ name: 'name.js', dir: MOCHITESTS_DIR });

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
 *
 * Options include:
 *   * recreated   - subrtee is recreated and the test should only continue
 *                   after a reorder event
 *   * textchanged - text is inserted into a subtree and the test should only
 *                   continue after a text inserted event
 */
const ARIARule = [{ attr: 'aria-labelledby' }, { attr: 'aria-label' }];
const HTMLControlHeadRule = [...ARIARule, { elm: 'label', isSibling: true }];
const rules = {
  CSSContent: [{ elm: 'style', isSibling: true }, { fromsubtree: true }],
  HTMLARIAGridCell: [...ARIARule, { fromsubtree: true }, { attr: 'title' }],
  HTMLControl: [...HTMLControlHeadRule, { fromsubtree: true },
    { attr: 'title' }],
  HTMLElm: [...ARIARule, { attr: 'title' }],
  HTMLImg: [...ARIARule, { attr: 'alt', recreated: true }, { attr: 'title' }],
  HTMLImgEmptyAlt: [...ARIARule, { attr: 'title' }, { attr: 'alt' }],
  HTMLInputButton: [...HTMLControlHeadRule, { attr: 'value' },
    { attr: 'title' }],
  HTMLInputImage: [...HTMLControlHeadRule, { attr: 'alt', recreated: true },
    { attr: 'value', recreated: true }, { attr: 'title' }],
  HTMLInputImageNoValidSrc: [...HTMLControlHeadRule,
    { attr: 'alt', recreated: true }, { attr: 'value', recreated: true }],
  HTMLInputReset: [...HTMLControlHeadRule,
    { attr: 'value', textchanged: true }],
  HTMLInputSubmit: [...HTMLControlHeadRule,
    { attr: 'value', textchanged: true }],
  HTMLLink: [...ARIARule, { fromsubtree: true }, { attr: 'title' }],
  HTMLLinkImage: [...ARIARule, { elm: 'img' }, { attr: 'title' }],
  HTMLOption: [...ARIARule, { attr: 'label' }, { fromsubtree: true },
    { attr: 'title' }],
  HTMLTable: [...ARIARule, { elm: 'caption' }, { attr: 'summary' },
    { attr: 'title' }]
};

const markupTests = [{
  id: 'btn',
  ruleset: 'HTMLControl',
  markup: `
    <span id="l1">test2</span>
    <span id="l2">test3</span>
    <label for="btn">test4</label>
    <button id="btn"
            aria-label="test1"
            aria-labelledby="l1 l2"
            title="test5">press me</button>`,
  expected: ['test2 test3', 'test1', 'test4', 'press me', 'test5']
}, {
  id: 'btn',
  ruleset: 'HTMLInputButton',
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
  expected: ['test2 test3', 'test1', 'test4', 'name from value',
    'name from title']
}, {
  id: 'btn-submit',
  ruleset: 'HTMLInputSubmit',
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
  expected: ['test2 test3', 'test1', 'test4', 'name from value']
}, {
  id: 'btn-reset',
  ruleset: 'HTMLInputReset',
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
  expected: ['test2 test3', 'test1', 'test4', 'name from value']
}, {
  id: 'btn-image',
  ruleset: 'HTMLInputImage',
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
  expected: ['test2 test3', 'test1', 'test4', 'name from alt',
    'name from value', 'name from title']
}, {
  id: 'btn-image',
  ruleset: 'HTMLInputImageNoValidSrc',
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
  expected: ['test2 test3', 'test1', 'test4', 'name from alt',
    'name from value']
}, {
  id: 'opt',
  ruleset: 'HTMLOption',
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
  expected: ['test2 test3', 'test1', 'test4', 'option1', 'test5']
}, {
  id: 'img',
  ruleset: 'HTMLImg',
  markup: `
    <span id="l1">test2</span>
    <span id="l2">test3</span>
    <img id="img"
         aria-label="Logo of Mozilla"
         aria-labelledby="l1 l2"
         alt="Mozilla logo"
         title="This is a logo"
         src="http://example.com/a11y/accessible/tests/mochitest/moz.png"/>`,
  expected: ['test2 test3', 'Logo of Mozilla', 'Mozilla logo', 'This is a logo']
}, {
  id: 'imgemptyalt',
  ruleset: 'HTMLImgEmptyAlt',
  markup: `
    <span id="l1">test2</span>
    <span id="l2">test3</span>
    <img id="imgemptyalt"
         aria-label="Logo of Mozilla"
         aria-labelledby="l1 l2"
         title="This is a logo"
         alt=""
         src="http://example.com/a11y/accessible/tests/mochitest/moz.png"/>`,
  expected: ['test2 test3', 'Logo of Mozilla', 'This is a logo', '']
}, {
  id: 'tc',
  ruleset: 'HTMLElm',
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
  expected: ['test2 test3', 'test1', 'test5']
}, {
  id: 'gc',
  ruleset: 'HTMLARIAGridCell',
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
  expected: ['test2 test3', 'test1', 'This is a paragraph',
    'This is a paragraph This is a link This is a list']
}, {
  id: 't',
  ruleset: 'HTMLTable',
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
  expected: ['lby_tst6_1 lby_tst6_2', 'arialabel_tst6', 'caption_tst6',
    'summary_tst6', 'title_tst6']
}, {
  id: 'btn',
  ruleset: 'CSSContent',
  markup: `
    <style>
      button::before {
        content: "do not ";
      }
    </style>
    <button id="btn">press me</button>`,
  expected: ['do not press me', 'press me']
}, {
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
  id: 'a',
  ruleset: 'HTMLLink',
  markup: `
    <span id="l1">test2</span>
    <span id="l2">test3</span>
    <a id="a"
       aria-label="test1"
       aria-labelledby="l1 l2"
       title="test4">test5</a>`,
  expected: ['test2 test3', 'test1', 'test5', 'test4']
}, {
  id: 'a-img',
  ruleset: 'HTMLLinkImage',
  markup: `
    <span id="l1">test2</span>
    <span id="l2">test3</span>
    <a id="a-img"
       aria-label="test1"
       aria-labelledby="l1 l2"
       title="test4"><img alt="test5"/></a>`,
  expected: ['test2 test3', 'test1', 'test5', 'test4']
}];

/**
 * Wait for an accessible event to happen and, in case given accessible is
 * defunct, update it to one that is attached to the accessible event.
 * @param {Promise} onEvent      accessible event promise
 * @param {Object}  target       { acc, parent, id } structure that contains an
 *                                accessible, its parent and its content element
 *                                id.
 */
function* updateAccessibleIfNeeded(onEvent, target) {
  let event = yield onEvent;
  if (isDefunct(target.acc)) {
    target.acc = findAccessibleChildByID(event.accessible, target.id);
  }
}

/**
 * Test accessible name that is calculated from an attribute, remove the
 * attribute before proceeding to the next name test. If attribute removal
 * results in a reorder or text inserted event - wait for it. If accessible
 * becomes defunct, update its reference using the one that is attached to one
 * of the above events.
 * @param {Object} browser      current "tabbrowser" element
 * @param {Object} target       { acc, parent, id } structure that contains an
 *                               accessible, its parent and its content element
 *                               id.
 * @param {Object} rule         current attr rule for name calculation
 * @param {[type]} expected     expected name value
 */
function* testAttrRule(browser, target, rule, expected) {
  testName(target.acc, expected);
  let onEvent;
  if (rule.recreated) {
    onEvent = waitForEvent(EVENT_REORDER, target.parent);
  } else if (rule.textchanged) {
    onEvent = waitForEvent(EVENT_TEXT_INSERTED, target.id);
  }
  yield invokeSetAttribute(browser, target.id, rule.attr);
  if (onEvent) {
    yield updateAccessibleIfNeeded(onEvent, target);
  }
}

/**
 * Test accessible name that is calculated from an element name, remove the
 * element before proceeding to the next name test. If element removal results
 * in a reorder event - wait for it. If accessible becomes defunct, update its
 * reference using the one that is attached to a possible reorder event.
 * @param {Object} browser      current "tabbrowser" element
 * @param {Object} target       { acc, parent, id } structure that contains an
 *                               accessible, its parent and its content element
 *                               id.
 * @param {Object} rule         current elm rule for name calculation
 * @param {[type]} expected     expected name value
 */
function* testElmRule(browser, target, rule, expected) {
  testName(target.acc, expected);
  let onEvent = waitForEvent(EVENT_REORDER, rule.isSibling ?
    target.parent : target.id);
  yield ContentTask.spawn(browser, rule.elm, elm =>
    content.document.querySelector(`${elm}`).remove());
  yield updateAccessibleIfNeeded(onEvent, target);
}

/**
 * Test accessible name that is calculated from its subtree, remove the subtree
 * and wait for a reorder event before proceeding to the next name test. If
 * accessible becomes defunct, update its reference using the one that is
 * attached to a reorder event.
 * @param {Object} browser      current "tabbrowser" element
 * @param {Object} target       { acc, parent, id } structure that contains an
 *                               accessible, its parent and its content element
 *                               id.
 * @param {Object} rule         current subtree rule for name calculation
 * @param {[type]} expected     expected name value
 */
function* testSubtreeRule(browser, target, rule, expected) {
  testName(target.acc, expected);
  let onEvent = waitForEvent(EVENT_REORDER, target.id);
  yield ContentTask.spawn(browser, target.id, id => {
    let elm = content.document.getElementById(id);
    while (elm.firstChild) {
      elm.removeChild(elm.firstChild);
    }
  });
  yield updateAccessibleIfNeeded(onEvent, target);
}

/**
 * Iterate over a list of rules and test accessible names for each one of the
 * rules.
 * @param {Object} browser      current "tabbrowser" element
 * @param {Object} target       { acc, parent, id } structure that contains an
 *                               accessible, its parent and its content element
 *                               id.
 * @param {Array}  ruleset      A list of rules to test a target with
 * @param {Array}  expected     A list of expected name value for each rule
 */
function* testNameRule(browser, target, ruleset, expected) {
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
    yield testFn(browser, target, rule, expected[i]);
  }
}

markupTests.forEach(({ id, ruleset, markup, expected }) =>
  addAccessibleTask(markup, function*(browser, accDoc) {
    // Find a target accessible from an accessible subtree.
    let acc = findAccessibleChildByID(accDoc, id);
    // Find target's parent accessible from an accessible subtree.
    let parent = getAccessibleDOMNodeID(acc.parent);
    let target = { id, parent, acc };
    yield testNameRule(browser, target, rules[ruleset], expected);
  }));
