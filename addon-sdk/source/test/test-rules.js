/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

const { Rules } = require('sdk/util/rules');
const { on, off, emit } = require('sdk/event/core');

exports.testAdd = function (test, done) {
  let rules = Rules();
  let urls = [
    'http://www.firefox.com',
    '*.mozilla.org',
    '*.html5audio.org'
  ];
  let count = 0;
  on(rules, 'add', function (rule) {
    if (count < urls.length) {
      test.ok(rules.get(rule), 'rule added to internal registry');
      test.equal(rule, urls[count], 'add event fired with proper params');
      if (++count < urls.length) rules.add(urls[count]);
      else done();
    }
  });
  rules.add(urls[0]);
};

exports.testRemove = function (test, done) {
  let rules = Rules();
  let urls = [
    'http://www.firefox.com',
    '*.mozilla.org',
    '*.html5audio.org'
  ];
  let count = 0;
  on(rules, 'remove', function (rule) {
    if (count < urls.length) {
      test.ok(!rules.get(rule), 'rule removed to internal registry');
      test.equal(rule, urls[count], 'remove event fired with proper params');
      if (++count < urls.length) rules.remove(urls[count]);
      else done();
    }
  });
  urls.forEach(function (url) rules.add(url));
  rules.remove(urls[0]);
};

exports.testMatchesAny = function(test) {
  let rules = Rules();
  rules.add('*.mozilla.org');
  rules.add('data:*');
  matchTest('http://mozilla.org', true);
  matchTest('http://www.mozilla.org', true);
  matchTest('http://www.google.com', false);
  matchTest('data:text/html;charset=utf-8,', true);

  function matchTest(string, expected) {
    test.equal(rules.matchesAny(string), expected,
      'Expected to find ' + string + ' in rules');
  }
};

exports.testIterable = function(test) {
  let rules = Rules();
  rules.add('*.mozilla.org');
  rules.add('data:*');
  rules.add('http://google.com');
  rules.add('http://addons.mozilla.org');
  rules.remove('http://google.com');

  test.equal(rules.length, 3, 'has correct length of keys');
  Array.forEach(rules, function (rule, i) {
    test.equal(rule, ['*.mozilla.org', 'data:*', 'http://addons.mozilla.org'][i]);
  });
  for (let i in rules)
    test.equal(rules[i], ['*.mozilla.org', 'data:*', 'http://addons.mozilla.org'][i]);
};

require('test').run(exports);
