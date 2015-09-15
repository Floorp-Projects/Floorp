/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

const { Cc, Ci } = require('chrome');
const { defer, all } = require('sdk/core/promise');
const { setTimeout } = require('sdk/timers');
const { request, response } = require('sdk/addon/host');
const { send } = require('sdk/addon/events');
const { filter } = require('sdk/event/utils');
const { on, emit, off } = require('sdk/event/core');

var stream = filter(request, (data) => /sdk-x-test/.test(data.event));

exports.testSend = function (assert, done) {
  on(stream, 'data', handle);
  send('sdk-x-test-simple', { title: 'my test data' }).then((data) => {
    assert.equal(data.title, 'my response', 'response is handled');
    off(stream, 'data', handle);
    done();
  }, (reason) => {
    assert.fail('should not call reject');
  });
  function handle (e) {
    assert.equal(e.event, 'sdk-x-test-simple', 'correct event name');
    assert.ok(e.id != null, 'message has an ID');
    assert.equal(e.data.title, 'my test data', 'serialized data passes');
    e.data.title = 'my response';
    emit(response, 'data', e);
  }
};

exports.testSendError = function (assert, done) {
  on(stream, 'data', handle);
  send('sdk-x-test-error', { title: 'my test data' }).then((data) => {
    assert.fail('should not call success');
  }, (reason) => {
    assert.equal(reason, 'ErrorInfo', 'should reject with error/reason');
    off(stream, 'data', handle);
    done();
  });
  function handle (e) {
    e.error = 'ErrorInfo';
    emit(response, 'data', e);
  }
};

exports.testMultipleSends = function (assert, done) {
  let count = 0;
  let ids = [];
  on(stream, 'data', handle);
  ['firefox', 'thunderbird', 'rust'].map(data =>
    send('sdk-x-test-multi', { data: data }).then(val => {
    assert.ok(val === 'firefox' || val === 'rust', 'successful calls resolve correctly');
    if (++count === 3) {
      off(stream, 'data', handle);
      done();
    }
  }, reason => {
    assert.equal(reason.error, 'too blue', 'rejected calls are rejected');
    if (++count === 3) {
      off(stream, 'data', handle);
      done();
    }
  }));

  function handle (e) {
    if (e.data !== 'firefox' || e.data !== 'rust')
      e.error = { data: e.data, error: 'too blue' };
    assert.ok(!~ids.indexOf(e.id), 'ID should be unique');
    assert.equal(e.event, 'sdk-x-test-multi', 'has event name');
    ids.push(e.id);
    emit(response, 'data', e);
  }
};

exports.testSerialization = function (assert, done) {
  on(stream, 'data', handle);
  let object = { title: 'my test data' };
  let resObject;
  send('sdk-x-test-serialize', object).then(data => {
    data.title = 'another title';
    assert.equal(object.title, 'my test data', 'original object not modified');
    assert.equal(resObject.title, 'new title', 'object passed by value from host');
    off(stream, 'data', handle);
    done();
  }, (reason) => {
    assert.fail('should not call reject');
  });
  function handle (e) {
    e.data.title = 'new title';
    assert.equal(object.title, 'my test data', 'object passed by value to host');
    resObject = e.data;
    emit(response, 'data', e);
  }
};

require('sdk/test').run(exports);
