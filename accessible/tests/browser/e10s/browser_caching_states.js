/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

/* global EVENT_STATE_CHANGE, STATE_CHECKED, STATE_BUSY, STATE_REQUIRED,
          STATE_INVALID, EXT_STATE_ENABLED */

loadScripts({ name: 'role.js', dir: MOCHITESTS_DIR },
            { name: 'states.js', dir: MOCHITESTS_DIR });

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
const attributeTests = [{
  desc: 'Checkbox with @checked attribute set to true should have checked ' +
        'state',
  attrs: [{
    attr: 'checked',
    value: 'true'
  }],
  expected: [STATE_CHECKED, 0]
}, {
  desc: 'Checkbox with no @checked attribute should not have checked state',
  attrs: [{
    attr: 'checked'
  }],
  expected: [0, 0, STATE_CHECKED]
}];

// State caching tests for ARIA changes
const ariaTests = [{
  desc: 'File input has busy state when @aria-busy attribute is set to true',
  attrs: [{
    attr: 'aria-busy',
    value: 'true'
  }],
  expected: [STATE_BUSY, 0, STATE_REQUIRED | STATE_INVALID]
}, {
  desc: 'File input has required state when @aria-required attribute is set ' +
        'to true',
  attrs: [{
    attr: 'aria-required',
    value: 'true'
  }],
  expected: [STATE_REQUIRED, 0, STATE_INVALID]
}, {
  desc: 'File input has invalid state when @aria-invalid attribute is set to ' +
        'true',
  attrs: [{
    attr: 'aria-invalid',
    value: 'true'
  }],
  expected: [STATE_INVALID, 0]
}];

// Extra state caching tests
const extraStateTests = [{
  desc: 'Input has no extra enabled state when aria and native disabled ' +
        'attributes are set at once',
  attrs: [{
    attr: 'aria-disabled',
    value: 'true'
  }, {
    attr: 'disabled',
    value: 'true'
  }],
  expected: [0, 0, 0, EXT_STATE_ENABLED]
}, {
  desc: 'Input has an extra enabled state when aria and native disabled ' +
        'attributes are unset at once',
  attrs: [{
    attr: 'aria-disabled'
  }, {
    attr: 'disabled'
  }],
  expected: [0, EXT_STATE_ENABLED]
}];

function* runStateTests(browser, accDoc, id, tests) {
  let acc = findAccessibleChildByID(accDoc, id);
  for (let { desc, attrs, expected } of tests) {
    info(desc);
    let onUpdate = waitForEvent(EVENT_STATE_CHANGE, id);
    for (let { attr, value } of attrs) {
      yield invokeSetAttribute(browser, id, attr, value);
    }
    yield onUpdate;
    testStates(acc, ...expected);
  }
}

/**
 * Test caching of accessible object states
 */
addAccessibleTask(`
  <input id="checkbox" type="checkbox">
  <input id="file" type="file">
  <input id="text">`,
  function* (browser, accDoc) {
    yield runStateTests(browser, accDoc, 'checkbox', attributeTests);
    yield runStateTests(browser, accDoc, 'file', ariaTests);
    yield runStateTests(browser, accDoc, 'text', extraStateTests);
  }
);
