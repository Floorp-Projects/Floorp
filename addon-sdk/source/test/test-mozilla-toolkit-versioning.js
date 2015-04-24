/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {parse, increment} = require("mozilla-toolkit-versioning/index")

const TestParse = assert => (version, min, max) => {
  const actual = parse(version);
  assert.equal(actual.min, min);
  assert.equal(actual.max, max);
}

const TestInc = assert => (version, expected) => {
  assert.equal(increment(version), expected,
               `increment: ${version} should be equal ${expected}`)
}



exports['test parse(version) single value'] = assert => {
  const testParse = TestParse(assert)
  testParse('1.2.3', '1.2.3', '1.2.3');
  testParse('>=1.2.3', '1.2.3', undefined);
  testParse('<=1.2.3', undefined, '1.2.3');
  testParse('>1.2.3', '1.2.3.1', undefined);
  testParse('<1.2.3', undefined, '1.2.3.-1');
  testParse('*', undefined, undefined);
};

exports['test parse(version) range'] = assert => {
  const testParse = TestParse(assert);
  testParse('>=1.2.3 <=2.3.4', '1.2.3', '2.3.4');
  testParse('>1.2.3 <=2.3.4', '1.2.3.1', '2.3.4');
  testParse('>=1.2.3 <2.3.4', '1.2.3', '2.3.4.-1');
  testParse('>1.2.3 <2.3.4', '1.2.3.1', '2.3.4.-1');

  testParse('<=2.3.4 >=1.2.3', '1.2.3', '2.3.4');
  testParse('<=2.3.4 >1.2.3', '1.2.3.1', '2.3.4');
  testParse('<2.3.4 >=1.2.3', '1.2.3', '2.3.4.-1');
  testParse('<2.3.4 >1.2.3', '1.2.3.1', '2.3.4.-1');

  testParse('1.2.3pre1 - 2.3.4', '1.2.3pre1', '2.3.4');
};

exports['test increment(version)'] = assert => {
  const testInc = TestInc(assert);

  testInc('1.2.3', '1.2.3.1');
  testInc('1.2.3a', '1.2.3a1');
  testInc('1.2.3pre', '1.2.3pre1');
  testInc('1.2.3pre1', '1.2.3pre2');
  testInc('1.2', '1.2.1');
  testInc('1.2pre1a', '1.2pre1b');
  testInc('1.2pre1pre', '1.2pre1prf');
};


require("sdk/test").run(exports);
