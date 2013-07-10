/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { Cc, Ci, Cu } = require('chrome');
const { getFavicon } = require('sdk/places/favicon');
const tabs = require('sdk/tabs');
const open = tabs.open;
const port = 8099;
const host = 'http://localhost:' + port;
const { onFaviconChange, serve, binFavicon } = require('./favicon-helpers');
const { once } = require('sdk/system/events');
const { defer } = require('sdk/core/promise');
const { clearHistory } = require('./places-helper');
const faviconService = Cc["@mozilla.org/browser/favicon-service;1"].
                         getService(Ci.nsIFaviconService);

exports.testStringGetFaviconCallbackSuccess = function (assert, done) {
  let name = 'callbacksuccess'
  let srv = makeServer(name);
  let url = host + '/' + name + '.html';
  let favicon = host + '/' + name + '.ico';
  let tab;

  onFaviconChange(url, function (faviconUrl) {
    getFavicon(url, function (url) {
      assert.equal(favicon, url, 'Callback returns correct favicon url');
      complete(tab, srv, done);
    });
  });

  open({
    url: url,
    onOpen: function (newTab) tab = newTab,
    inBackground: true
  });
};

exports.testStringGetFaviconCallbackFailure = function (assert, done) {
  let name = 'callbackfailure';
  let srv = makeServer(name);
  let url = host + '/' + name + '.html';
  let tab;

  waitAndExpire(url).then(function () {
    getFavicon(url, function (url) {
      assert.equal(url, null, 'Callback returns null');
      complete(tab, srv, done);
    });
  });

  open({
    url: url,
    onOpen: function (newTab) tab = newTab,
    inBackground: true
  });
};

exports.testStringGetFaviconPromiseSuccess = function (assert, done) {
  let name = 'promisesuccess'
  let srv = makeServer(name);
  let url = host + '/' + name + '.html';
  let favicon = host + '/' + name + '.ico';
  let tab;

  onFaviconChange(url, function (faviconUrl) {
    getFavicon(url).then(function (url) {
      assert.equal(url, favicon, 'Callback returns null');
    }, function (err) {
      assert.fail('Reject should not be called');
    }).then(complete.bind(null, tab, srv, done));
  });

  open({
    url: url,
    onOpen: function (newTab) tab = newTab,
    inBackground: true
  });
};

exports.testStringGetFaviconPromiseFailure = function (assert, done) {
  let name = 'promisefailure'
  let srv = makeServer(name);
  let url = host + '/' + name + '.html';
  let tab;

  waitAndExpire(url).then(function () {
    getFavicon(url).then(invalidResolve(assert), validReject(assert, 'expired url'))
      .then(complete.bind(null, tab, srv, done));
  });

  open({
    url: url,
    onOpen: function (newTab) tab = newTab,
    inBackground: true
  });
};

exports.testTabsGetFaviconPromiseSuccess = function (assert, done) {
  let name = 'tabs-success'
  let srv = makeServer(name);
  let url = host + '/' + name + '.html';
  let favicon = host + '/' + name + '.ico';
  let tab;

  onFaviconChange(url, function () {
    getFavicon(tab).then(function (url) {
      assert.equal(url, favicon, "getFavicon should return url for tab");
      complete(tab, srv, done);
    });
  });

  open({
    url: url,
    onOpen: function (newTab) tab = newTab,
    inBackground: true
  });
};


exports.testTabsGetFaviconPromiseFailure = function (assert, done) {
  let name = 'tabs-failure'
  let srv = makeServer(name);
  let url = host + '/' + name + '.html';
  let tab;

  waitAndExpire(url).then(function () {
    getFavicon(tab).then(invalidResolve(assert), validReject(assert, 'expired tab'))
      .then(complete.bind(null, tab, srv, done));
  });

  open({
    url: url,
    onOpen: function (newTab) tab = newTab,
    inBackground: true
  });
};

exports.testRejects = function (assert, done) {
  getFavicon({})
    .then(invalidResolve(assert), validReject(assert, 'Object'))
  .then(getFavicon(null))
    .then(invalidResolve(assert), validReject(assert, 'null'))
  .then(getFavicon(undefined))
    .then(invalidResolve(assert), validReject(assert, 'undefined'))
  .then(getFavicon([]))
    .then(invalidResolve(assert), validReject(assert, 'Array'))
    .then(done);
};

function invalidResolve (assert) {
  return function () assert.fail('Promise should not be resolved successfully');
}

function validReject (assert, name) {
  return function () assert.pass(name + ' correctly rejected');
}

function makeServer (name) {
  return serve({name: name, favicon: binFavicon, port: port, host: host});
}

function waitAndExpire (url) {
  let deferred = defer();
  onFaviconChange(url, function (faviconUrl) {
    once('places-favicons-expired', function () {
      deferred.resolve();
    });
    faviconService.expireAllFavicons();
  });
  return deferred.promise;
}

function complete(tab, srv, done) {
  tab.close(function () {
    clearHistory(() => {
      srv.stop(done);
    });
  });
}

require("test").run(exports);
