/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
 'use strict'

const { XMLHttpRequest } = require('sdk/net/xhr');
const { LoaderWithHookedConsole } = require('sdk/test/loader');
const { set: setPref } = require("sdk/preferences/service");
const data = require("./fixtures");

const DEPRECATE_PREF = "devtools.errorconsole.deprecation_warnings";

exports.testAPIExtension = function(assert) {
  let { loader, messages } = LoaderWithHookedConsole(module);
  let { XMLHttpRequest } = loader.require("sdk/net/xhr");
  setPref(DEPRECATE_PREF, true);

  let xhr = new XMLHttpRequest();
  assert.equal(typeof(xhr.forceAllowThirdPartyCookie), "function",
               "forceAllowThirdPartyCookie is defined");
  assert.equal(xhr.forceAllowThirdPartyCookie(), undefined,
               "function can be called");

  assert.ok(messages[0].msg.indexOf("`xhr.forceAllowThirdPartyCookie()` is deprecated") >= 0,
            "deprecation warning was dumped");
  assert.ok(xhr.mozBackgroundRequest, "is background request");

  loader.unload();
};

exports.testAbortedXhr = function(assert, done) {
  let req = new XMLHttpRequest();
  req.open('GET', data.url('testLocalXhr.json'));
  req.addEventListener("abort", function() {
    assert.pass("request was aborted");
    done();
  });
  req.send(null);
  req.abort();
};

exports.testLocalXhr = function(assert, done) {
  let req = new XMLHttpRequest();
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
    done();
  });
  req.send(null);
};


exports.testResponseHeaders = function(assert, done) {
  let req = new XMLHttpRequest();

  req.overrideMimeType('text/plain');
  req.open('GET', module.uri);
  req.onreadystatechange = function() {
    if (req.readyState == 4 && (req.status == 0 || req.status == 200)) {
      var headers = req.getAllResponseHeaders();
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

      done();
    }
  };
  req.send(null);
}

require('test').run(exports);
