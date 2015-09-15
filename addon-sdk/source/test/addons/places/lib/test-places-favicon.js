/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

module.metadata = {
  'engines': {
    'Firefox': '*'
  }
};

const { Cc, Ci, Cu } = require('chrome');
const { getFavicon } = require('sdk/places/favicon');
const tabs = require('sdk/tabs');
const open = tabs.open;
const port = 8099;
const host = 'http://localhost:' + port;
const { onFaviconChange, serve, binFavicon } = require('./favicon-helpers');
const { once } = require('sdk/system/events');
const { resetPlaces } = require('./places-helper');
const faviconService = Cc["@mozilla.org/browser/favicon-service;1"].
                         getService(Ci.nsIFaviconService);

exports.testStringGetFaviconCallbackSuccess = function*(assert) {
  let name = 'callbacksuccess'
  let srv = yield makeServer(name);
  let url = host + '/' + name + '.html';
  let favicon = host + '/' + name + '.ico';
  let tab;

  let wait = new Promise(resolve => {
    onFaviconChange(url).then((faviconUrl) => {
      getFavicon(url, (url) => {
        assert.equal(favicon, url, 'Callback returns correct favicon url');
        resolve();
      });
    });
  });

  assert.pass("Opening tab");

  open({
    url: url,
    onOpen: (newTab) => tab = newTab,
    inBackground: true
  });

  yield wait;

  assert.pass("Complete");

  yield complete(tab, srv);
};

exports.testStringGetFaviconCallbackFailure = function*(assert) {
  let name = 'callbackfailure';
  let srv = yield makeServer(name);
  let url = host + '/' + name + '.html';
  let tab;

  let wait = waitAndExpire(url);

  assert.pass("Opening tab");

  open({
    url: url,
    onOpen: (newTab) => tab = newTab,
    inBackground: true
  });

  yield wait;

  assert.pass("Getting favicon");

  yield new Promise(resolve => {
    getFavicon(url, (url) => {
      assert.equal(url, null, 'Callback returns null');
      resolve();
    });
  });

  assert.pass("Complete");

  yield complete(tab, srv);
};

exports.testStringGetFaviconPromiseSuccess = function*(assert) {
  let name = 'promisesuccess'
  let srv = yield makeServer(name);
  let url = host + '/' + name + '.html';
  let favicon = host + '/' + name + '.ico';
  let tab;

  let wait = onFaviconChange(url);

  assert.pass("Opening tab");

  open({
    url: url,
    onOpen: (newTab) => tab = newTab,
    inBackground: true
  });

  yield wait;

  assert.pass("Getting favicon");

  yield getFavicon(url).then((url) => {
    assert.equal(url, favicon, 'Callback returns null');
  }, () => {
    assert.fail('Reject should not be called');
  });

  assert.pass("Complete");

  yield complete(tab, srv);
};

exports.testStringGetFaviconPromiseFailure = function*(assert) {
  let name = 'promisefailure'
  let srv = yield makeServer(name);
  let url = host + '/' + name + '.html';
  let tab;

  let wait = waitAndExpire(url);

  assert.pass("Opening tab");

  open({
    url: url,
    onOpen: (newTab) => tab = newTab,
    inBackground: true
  });

  yield wait;

  assert.pass("Getting favicon");

  yield getFavicon(url).then(invalidResolve(assert), validReject(assert, 'expired url'));

  assert.pass("Complete");

  yield complete(tab, srv);
};

exports.testTabsGetFaviconPromiseSuccess = function*(assert) {
  let name = 'tabs-success'
  let srv = yield makeServer(name);
  let url = host + '/' + name + '.html';
  let favicon = host + '/' + name + '.ico';
  let tab;

  let iconPromise = onFaviconChange(url);

  assert.pass("Opening tab");

  open({
    url: url,
    onOpen: (newTab) => tab = newTab,
    inBackground: true
  });

  yield iconPromise;

  assert.pass("Getting favicon");

  yield getFavicon(tab).then((url) => {
    assert.equal(url, favicon, "getFavicon should return url for tab");
  });

  assert.pass("Complete");

  yield complete(tab, srv);
};


exports.testTabsGetFaviconPromiseFailure = function*(assert) {
  let name = 'tabs-failure'
  let srv = yield makeServer(name);
  let url = host + '/' + name + '.html';
  let tab;

  let wait = waitAndExpire(url);

  assert.pass("Opening tab");

  open({
    url: url,
    onOpen: (newTab) => tab = newTab,
    inBackground: true
  });

  yield wait;

  assert.pass("Getting favicon");

  yield getFavicon(tab).then(invalidResolve(assert), validReject(assert, 'expired tab'));

  assert.pass("Complete");

  yield complete(tab, srv);
};

exports.testRejects = function*(assert) {
  yield getFavicon({})
    .then(invalidResolve(assert), validReject(assert, 'Object'));

  yield getFavicon(null)
    .then(invalidResolve(assert), validReject(assert, 'null'));

  yield getFavicon(undefined)
    .then(invalidResolve(assert), validReject(assert, 'undefined'));

  yield getFavicon([])
    .then(invalidResolve(assert), validReject(assert, 'Array'));
};

var invalidResolve = (assert) => () => assert.fail('Promise should not be resolved successfully');
var validReject = (assert, name) => () => assert.pass(name + ' correctly rejected');

var makeServer = (name) => serve({
  name: name,
  favicon: binFavicon,
  port: port,
  host: host
});

var waitAndExpire = (url) => new Promise(resolve => {
  onFaviconChange(url).then(() => {
    once('places-favicons-expired', resolve);
    faviconService.expireAllFavicons();
  });
});

var complete = (tab, srv) => new Promise(resolve => {
  tab.close(() => {
    resetPlaces(() => {
      srv.stop(resolve);
    });
  });
});
