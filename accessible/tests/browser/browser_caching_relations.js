/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

/* global RELATION_LABELLED_BY, RELATION_LABEL_FOR, RELATION_DESCRIBED_BY,
          RELATION_DESCRIPTION_FOR, RELATION_CONTROLLER_FOR,
          RELATION_CONTROLLED_BY, RELATION_FLOWS_TO, RELATION_FLOWS_FROM */

loadScripts({ name: 'relations.js', dir: MOCHITESTS_DIR });

/**
 * A test specification that has the following format:
 * [
 *   attr                 relevant aria attribute
 *   hostRelation         corresponding host relation type
 *   dependantRelation    corresponding dependant relation type
 * ]
 */
const attrRelationsSpec = [
  ['aria-labelledby', RELATION_LABELLED_BY, RELATION_LABEL_FOR],
  ['aria-describedby', RELATION_DESCRIBED_BY, RELATION_DESCRIPTION_FOR],
  ['aria-controls', RELATION_CONTROLLER_FOR, RELATION_CONTROLLED_BY],
  ['aria-flowto', RELATION_FLOWS_TO, RELATION_FLOWS_FROM]
];

function* testRelated(browser, accDoc, attr, hostRelation, dependantRelation) {
  let host = findAccessibleChildByID(accDoc, 'host');
  let dependant1 = findAccessibleChildByID(accDoc, 'dependant1');
  let dependant2 = findAccessibleChildByID(accDoc, 'dependant2');

  /**
   * Test data has the format of:
   * {
   *   desc      {String}   description for better logging
   *   attrs     {?Array}   an optional list of attributes to update
   *   expected  {Array}    expected relation values for dependant1, dependant2
   *                        and host respectively.
   * }
   */
  const tests = [{
    desc: 'No attribute',
    expected: [ null, null, null ]
  }, {
    desc: 'Set attribute',
    attrs: [{ key: attr, value: 'dependant1' }],
    expected: [ host, null, dependant1 ]
  }, {
    desc: 'Change attribute',
    attrs: [{ key: attr, value: 'dependant2' }],
    expected: [ null, host, dependant2 ]
  }, {
    desc: 'Remove attribute',
    attrs: [{ key: attr }],
    expected: [ null, null, null ]
  }];

  for (let { desc, attrs, expected } of tests) {
    info(desc);

    if (attrs) {
      for (let { key, value } of attrs) {
        yield invokeSetAttribute(browser, 'host', key, value);
      }
    }

    testRelation(dependant1, dependantRelation, expected[0]);
    testRelation(dependant2, dependantRelation, expected[1]);
    testRelation(host, hostRelation, expected[2]);
  }
}

/**
 * Test caching of relations between accessible objects.
 */
addAccessibleTask(`
  <div id="dependant1">label</div>
  <div id="dependant2">label2</div>
  <div role="checkbox" id="host"></div>`,
  function* (browser, accDoc) {
    for (let spec of attrRelationsSpec) {
      yield testRelated(browser, accDoc, ...spec);
    }
  }
);
