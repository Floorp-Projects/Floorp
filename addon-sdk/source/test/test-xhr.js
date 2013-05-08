/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
 'use strict'

const xhr = require('sdk/net/xhr');
const { Loader } = require('sdk/test/loader');
const xulApp = require('sdk/system/xul-app');
const { data } = require('sdk/self');

// TODO: rewrite test below
/* Test is intentionally disabled until platform bug 707256 is fixed.
exports.testAbortedXhr = function(test) {
  var req = new xhr.XMLHttpRequest();
  test.assertEqual(xhr.getRequestCount(), 1);
  req.abort();
  test.assertEqual(xhr.getRequestCount(), 0);
};
*/

exports.testLocalXhr = function(assert, done) {
  var req = new xhr.XMLHttpRequest();
  let ready = false;

  req.overrideMimeType('text/plain');
  req.open('GET', data.url('testLocalXhr.json'));
  req.onreadystatechange = function() {
    if (req.readyState == 4 && (req.status == 0 || req.status == 200)) {
      ready = true;
      assert.equal(req.responseText, '{}\n', 'XMLHttpRequest should get local files');
    }
  };
  req.addEventListener('load', function onload() {
    req.removeEventListener('load', onload);
    assert.pass('addEventListener for load event worked');
    assert.ok(ready, 'onreadystatechange listener worked');
    assert.equal(xhr.getRequestCount(), 0, 'request count is 0');
    done();
  });
  req.send(null);

  assert.equal(xhr.getRequestCount(), 1, 'request count is 1');
};

exports.testUnload = function(assert) {
  var loader = Loader(module);
  var sbxhr = loader.require('sdk/net/xhr');
  var req = new sbxhr.XMLHttpRequest();

  req.overrideMimeType('text/plain');
  req.open("GET", module.uri);
  req.send(null);

  assert.equal(sbxhr.getRequestCount(), 1, 'request count is 1');
  loader.unload();
  assert.equal(sbxhr.getRequestCount(), 0, 'request count is 0');
};

exports.testResponseHeaders = function(assert, done) {
  var req = new xhr.XMLHttpRequest();

  req.overrideMimeType('text/plain');
  req.open('GET', module.uri);
  req.onreadystatechange = function() {
    if (req.readyState == 4 && (req.status == 0 || req.status == 200)) {
      var headers = req.getAllResponseHeaders();
      if (xulApp.satisfiesVersion(xulApp.platformVersion, '>=13.0a1')) {
        headers = headers.split("\r\n");
        if (headers.length == 1) {
          headers = headers[0].split("\n");
        }
        for (let i in headers) {
          if (headers[i] && headers[i].search('Content-Type') >= 0) {
            assert.equal(headers[i], 'Content-Type: text/plain',
                         'XHR\'s headers are valid');
          }
        }
      }
      else {
        assert.ok(headers === null || headers === '',
                  'XHR\'s headers are empty');
      }

      done();
    }
  };
  req.send(null);

  assert.equal(xhr.getRequestCount(), 1, 'request count is 1');
}

require('test').run(exports);
