/*
 * Copyright (c) 2009 Panagiotis Astithas
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

// define(function(require, exports, module) {

// <INJECTED SOURCE:START>

// THIS FILE IS GENERATED FROM SOURCE IN THE GCLI PROJECT
// DO NOT EDIT IT DIRECTLY

var exports = {};

const TEST_URI = "data:text/html;charset=utf-8,<p id='gcli-input'>gcli-testSpell.js</p>";

function test() {
  helpers.addTabWithToolbar(TEST_URI, function(options) {
    return helpers.runTests(options, exports);
  }).then(finish);
}

// <INJECTED SOURCE:END>

'use strict';

// var assert = require('test/assert');
var spell = require('util/spell');

exports.testSpellerSimple = function(options) {
  var alternatives = Object.keys(options.window);

  assert.is(spell.correct('document', alternatives), 'document');
  assert.is(spell.correct('documen', alternatives), 'document');

  if (options.isJsdom) {
    assert.log('jsdom is weird, skipping some tests');
  }
  else {
    assert.is(spell.correct('ocument', alternatives), 'document');
  }
  assert.is(spell.correct('odcument', alternatives), 'document');

  assert.is(spell.correct('=========', alternatives), undefined);
};

exports.testRank = function(options) {
  var distances = spell.rank('fred', [ 'banana', 'fred', 'ed', 'red', 'FRED' ]);

  assert.is(distances.length, 5, 'rank length');

  assert.is(distances[0].name, 'fred', 'fred name #0');
  assert.is(distances[1].name, 'FRED', 'FRED name #1');
  assert.is(distances[2].name, 'red', 'red name #2');
  assert.is(distances[3].name, 'ed', 'ed name #3');
  assert.is(distances[4].name, 'banana', 'banana name #4');

  assert.is(distances[0].dist, 0, 'fred dist 0');
  assert.is(distances[1].dist, 4, 'FRED dist 4');
  assert.is(distances[2].dist, 10, 'red dist 10');
  assert.is(distances[3].dist, 20, 'ed dist 20');
  assert.is(distances[4].dist, 100, 'banana dist 100');
};

exports.testRank2 = function(options) {
  var distances = spell.rank('caps', [ 'CAPS', 'false' ]);
  assert.is(JSON.stringify(distances),
            '[{"name":"CAPS","dist":4},{"name":"false","dist":50}]',
            'spell.rank("caps", [ "CAPS", "false" ]');
};

exports.testDistancePrefix = function(options) {
  assert.is(spell.distancePrefix('fred', 'freddy'), 0, 'distancePrefix fred');
  assert.is(spell.distancePrefix('FRED', 'freddy'), 4, 'distancePrefix FRED');
};

// });
