/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { Cc, Ci, Cu } = require('chrome');
const { getFavicon, getFaviconURIForLocation } = require('sdk/io/data');
const tabs = require('sdk/tabs');
const open = tabs.open;
const port = 8099;
const host = 'http://localhost:' + port;
const { onFaviconChange, serve, binFavicon } = require('./favicon-helpers');
const { once } = require('sdk/system/events');
const faviconService = Cc["@mozilla.org/browser/favicon-service;1"].
                         getService(Ci.nsIFaviconService);

exports.testGetFaviconCallbackSuccess = function (assert, done) {
  let name = 'callbacksuccess'
  let srv = serve({name: name, favicon: binFavicon, port: port, host: host});
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

exports.testGetFaviconCallbackFailure = function (assert, done) {
  let name = 'callbackfailure';
  let srv = serve({name: name, favicon: binFavicon, port: port, host: host});
  let url = host + '/' + name + '.html';
  let tab;

  onFaviconChange(url, function (faviconUrl) {
    once('places-favicons-expired', function () {
      getFavicon(url, function (url) {
        assert.equal(url, null, 'Callback returns null');
        complete(tab, srv, done);
      });
    });
    faviconService.expireAllFavicons();
  });

  open({
    url: url,
    onOpen: function (newTab) tab = newTab,
    inBackground: true
  });
};

exports.testGetFaviconPromiseSuccess = function (assert, done) {
  let name = 'promisesuccess'
  let srv = serve({name: name, favicon: binFavicon, port: port, host: host});
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

exports.testGetFaviconPromiseFailure = function (assert, done) {
  let name = 'promisefailure'
  let srv = serve({name: name, favicon: binFavicon, port: port, host: host});
  let url = host + '/' + name + '.html';
  let tab;

  onFaviconChange(url, function (faviconUrl) {
    once('places-favicons-expired', function () {
      getFavicon(url).then(function (url) {
        assert.fail('success should not be called');
      }, function (err) {
        assert.equal(err, null, 'should call reject');
      }).then(complete.bind(null, tab, srv, done));
    });
    faviconService.expireAllFavicons();
  });

  open({
    url: url,
    onOpen: function (newTab) tab = newTab,
    inBackground: true
  });
};

function complete(tab, srv, done) {
  tab.close(function () {
    srv.stop(done);
  })
}

require("test").run(exports);
