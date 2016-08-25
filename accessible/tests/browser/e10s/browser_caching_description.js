/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

/* global EVENT_DESCRIPTION_CHANGE, EVENT_NAME_CHANGE, EVENT_REORDER */

loadScripts({ name: 'name.js', dir: MOCHITESTS_DIR });

/**
 * Test data has the format of:
 * {
 *   desc      {String}   description for better logging
 *   expected  {String}   expected description value for a given accessible
 *   attrs     {?Array}   an optional list of attributes to update
 *   waitFor   {?Array}   an optional list of accessible events to wait for when
 *                        attributes are updated
 * }
 */
const tests = [{
  desc: 'No description when there are no @alt, @title and @aria-describedby',
  expected: ''
}, {
  desc: 'Description from @aria-describedby attribute',
  attrs: [{
    attr: 'aria-describedby',
    value: 'description'
  }],
  waitFor: [{ eventType: EVENT_DESCRIPTION_CHANGE, id: 'image' }],
  expected: 'aria description'
}, {
  desc: 'No description from @aria-describedby since it is the same as the ' +
        '@alt attribute which is used as the name',
  attrs: [{
    attr: 'alt',
    value: 'aria description'
  }],
  waitFor: [{ eventType: EVENT_REORDER, id: 'body' }],
  expected: ''
}, {
  desc: 'Description from @aria-describedby attribute when @alt and ' +
        '@aria-describedby are not the same',
  attrs: [{
    attr: 'aria-describedby',
    value: 'description2'
  }],
  waitFor: [{ eventType: EVENT_DESCRIPTION_CHANGE, id: 'image' }],
  expected: 'another description'
}, {
  desc: 'Description from @aria-describedby attribute when @title (used for ' +
        'name) and @aria-describedby are not the same',
  attrs: [{
    attr: 'alt'
  }, {
    attr: 'title',
    value: 'title'
  }],
  waitFor: [{ eventType: EVENT_REORDER, id: 'body' }],
  expected: 'another description'
}, {
  desc: 'No description from @aria-describedby since it is the same as the ' +
        '@title attribute which is used as the name',
  attrs: [{
    attr: 'title',
    value: 'another description'
  }],
  waitFor: [{ eventType: EVENT_NAME_CHANGE, id: 'image' }],
  expected: ''
}, {
  desc: 'No description with only @title attribute which is used as the name',
  attrs: [{
    attr: 'aria-describedby'
  }],
  waitFor: [{ eventType: EVENT_DESCRIPTION_CHANGE, id: 'image' }],
  expected: ''
}, {
  desc: 'Description from @title attribute when @alt and @atitle are not the ' +
        'same',
  attrs: [{
    attr: 'alt',
    value: 'aria description'
  }],
  waitFor: [{ eventType: EVENT_REORDER, id: 'body' }],
  expected: 'another description'
}, {
  desc: 'No description from @title since it is the same as the @alt ' +
        'attribute which is used as the name',
  attrs: [{
    attr: 'alt',
    value: 'another description'
  }],
  waitFor: [{ eventType: EVENT_NAME_CHANGE, id: 'image' }],
  expected: ''
}, {
  desc: 'No description from @aria-describedby since it is the same as the ' +
        '@alt (used for name) and @title attributes',
  attrs: [{
    attr: 'aria-describedby',
    value: 'description2'
  }],
  waitFor: [{ eventType: EVENT_DESCRIPTION_CHANGE, id: 'image' }],
  expected: ''
}, {
  desc: 'Description from @aria-describedby attribute when it is different ' +
        'from @alt (used for name) and @title attributes',
  attrs: [{
    attr: 'aria-describedby',
    value: 'description'
  }],
  waitFor: [{ eventType: EVENT_DESCRIPTION_CHANGE, id: 'image' }],
  expected: 'aria description'
}, {
  desc: 'No description from @aria-describedby since it is the same as the ' +
        '@alt attribute (used for name) but different from title',
  attrs: [{
    attr: 'alt',
    value: 'aria description'
  }],
  waitFor: [{ eventType: EVENT_NAME_CHANGE, id: 'image' }],
  expected: ''
}, {
  desc: 'Description from @aria-describedby attribute when @alt (used for ' +
        'name) and @aria-describedby are not the same but @title and ' +
        'aria-describedby are',
  attrs: [{
    attr: 'aria-describedby',
    value: 'description2'
  }],
  waitFor: [{ eventType: EVENT_DESCRIPTION_CHANGE, id: 'image' }],
  expected: 'another description'
}];

/**
 * Test caching of accessible object description
 */
addAccessibleTask(`
  <p id="description">aria description</p>
  <p id="description2">another description</p>
  <img id="image" />`,
  function*(browser, accDoc) {
    let imgAcc = findAccessibleChildByID(accDoc, 'image');

    for (let { desc, waitFor, attrs, expected } of tests) {
      info(desc);
      let onUpdate;
      if (waitFor) {
        onUpdate = waitForMultipleEvents(waitFor);
      }
      if (attrs) {
        for (let { attr, value } of attrs) {
          yield invokeSetAttribute(browser, 'image', attr, value);
        }
      }
      yield onUpdate;
      // When attribute change (alt) triggers reorder event, accessible will
      // become defunct.
      if (isDefunct(imgAcc)) {
        imgAcc = findAccessibleChildByID(accDoc, 'image');
      }
      testDescr(imgAcc, expected);
    }
  }
);
