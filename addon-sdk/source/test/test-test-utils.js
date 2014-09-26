/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict'

const { setTimeout } = require('sdk/timers');
const { waitUntil, cleanUI } = require('sdk/test/utils');
const tabs = require('sdk/tabs');

exports.testWaitUntil = function (assert, done) {
  let bool = false;
  let finished = false;
  waitUntil(() => {
    if (finished)
      assert.fail('interval should be cleared after predicate is truthy');
    return bool;
  }).then(function () {
    assert.ok(bool,
      'waitUntil shouldn\'t call until predicate is truthy');
    finished = true;
    done();
  });
  setTimeout(() => { bool = true; }, 20);
};

exports.testWaitUntilInterval = function (assert, done) {
  let bool = false;
  let finished = false;
  let counter = 0;
  waitUntil(() => {
    if (finished)
      assert.fail('interval should be cleared after predicate is truthy');
    counter++;
    return bool;
  }, 50).then(function () {
    assert.ok(bool,
      'waitUntil shouldn\'t call until predicate is truthy');
    assert.equal(counter, 1,
      'predicate should only be called once with a higher interval');
    finished = true;
    done();
  });
  setTimeout(() => { bool = true; }, 10);
};

exports.testCleanUIWithExtraTabAndWindow = function(assert, done) {
  tabs.open({
    url: "about:blank",
    inNewWindow: true,
    onOpen: () => {
      cleanUI().then(() => {
        assert.pass("the ui was cleaned");
        assert.equal(tabs.length, 1, 'there is only one tab open');
      }).then(done).catch(assert.fail);
    }
  });
}

exports.testCleanUIWithOnlyExtraTab = function(assert, done) {
  tabs.open({
    url: "about:blank",
    onOpen: () => {
      cleanUI().then(() => {
        assert.pass("the ui was cleaned");
        assert.equal(tabs.length, 1, 'there is only one tab open');
      }).then(done).catch(assert.fail);
    }
  });
}

require('sdk/test').run(exports);
