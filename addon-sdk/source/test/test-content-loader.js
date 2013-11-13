/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
const { Loader } = require('sdk/content/loader');
const self = require("sdk/self");
const fixtures = require("./fixtures");

exports['test:contentURL'] = function(assert) {
  let loader = Loader(),
      value, emitted = 0, changes = 0;

  assert.throws(
    function() loader.contentURL = 4,
    /The `contentURL` option must be a valid URL./,
    'Must throw an exception if `contentURL` is not URL.'
  );
 assert.throws(
    function() loader.contentURL = { toString: function() 'Oops' },
    /The `contentURL` option must be a valid URL./,
    'Must throw an exception if `contentURL` is not URL.'
  );

  function listener(e) {
    emitted ++;
    assert.ok(
      'contentURL' in e,
      'emitted event must contain "content" property'
    );
    assert.ok(
      value,
      '' + e.contentURL,
      'content property of an event must match value'
    );
  }
  loader.on('propertyChange', listener);

  assert.equal(
    null,
    loader.contentURL,
    'default value is `null`'
  );
  loader.contentURL = value = 'data:text/html,<html><body>Hi</body><html>';
  assert.equal(
    value,
    '' + loader.contentURL,
    'data uri is ok'
  );
  assert.equal(
    ++changes,
    emitted,
    'had to emit `propertyChange`'
  );
  loader.contentURL = value;
  assert.equal(
    changes,
    emitted,
    'must not emit `propertyChange` if same value is set'
  );

  loader.contentURL = value = 'http://google.com/';
  assert.equal(
    value,
    '' + loader.contentURL,
    'value must be set'
  );
  assert.equal(
    ++ changes,
    emitted,
    'had to emit `propertyChange`'
  );
  loader.contentURL = value;
  assert.equal(
    changes,
    emitted,
    'must not emit `propertyChange` if same value is set'
  );

  loader.removeListener('propertyChange', listener);
  loader.contentURL = value = 'about:blank';
  assert.equal(
    value,
    '' + loader.contentURL,
    'contentURL must be an actual value'
  );
  assert.equal(
    changes,
    emitted,
    'listener had to be romeved'
  );
};

exports['test:contentScriptWhen'] = function(assert) {
  let loader = Loader();
  assert.equal(
    'end',
    loader.contentScriptWhen,
    '`contentScriptWhen` defaults to "end"'
  );
  loader.contentScriptWhen = "end";
  assert.equal(
    "end",
    loader.contentScriptWhen
  );
  try {
    loader.contentScriptWhen = 'boom';
    test.fail('must throw when wrong value is set');
  } catch(e) {
    assert.equal(
      'The `contentScriptWhen` option must be either "start", "ready" or "end".',
      e.message
    );
  }
  loader.contentScriptWhen = null;
  assert.equal(
    'end',
    loader.contentScriptWhen,
    '`contentScriptWhen` defaults to "end"'
  );
  loader.contentScriptWhen = "ready";
  assert.equal(
    "ready",
    loader.contentScriptWhen
  );
  loader.contentScriptWhen = "start";
  assert.equal(
    'start',
    loader.contentScriptWhen
  );
};

exports['test:contentScript'] = function(assert) {
  let loader = Loader(), value;
  assert.equal(
    null,
    loader.contentScript,
    '`contentScript` defaults to `null`'
  );
  loader.contentScript = value = 'let test = {};';
  assert.equal(
    value,
    loader.contentScript
  );
  try {
    loader.contentScript = { 1: value }
    test.fail('must throw when wrong value is set');
  } catch(e) {
    assert.equal(
      'The `contentScript` option must be a string or an array of strings.',
      e.message
    );
  }
  try {
    loader.contentScript = ['oue', 2]
    test.fail('must throw when wrong value is set');
  } catch(e) {
    assert.equal(
      'The `contentScript` option must be a string or an array of strings.',
      e.message
    );
  }
  loader.contentScript = undefined;
  assert.equal(
    null,
    loader.contentScript
  );
  loader.contentScript = value = ["1;", "2;"];
  assert.equal(
    value,
    loader.contentScript
  );
};

exports['test:contentScriptFile'] = function(assert) {
  let loader = Loader(), value, uri = fixtures.url("test-content-loader.js");
  assert.equal(
    null,
    loader.contentScriptFile,
    '`contentScriptFile` defaults to `null`'
  );
  loader.contentScriptFile = value = uri;
  assert.equal(
    value,
    loader.contentScriptFile
  );
  try {
    loader.contentScriptFile = { 1: uri }
    test.fail('must throw when wrong value is set');
  } catch(e) {
    assert.equal(
      'The `contentScriptFile` option must be a local URL or an array of URLs.',
      e.message
    );
  }

  try {
    loader.contentScriptFile = [ 'oue', uri ]
    test.fail('must throw when wrong value is set');
  } catch(e) {
    assert.equal(
      'The `contentScriptFile` option must be a local URL or an array of URLs.',
      e.message
    );
  }

  loader.contentScriptFile = undefined;
  assert.equal(
    null,
    loader.contentScriptFile
  );
  loader.contentScriptFile = value = [uri];
  assert.equal(
    value,
    loader.contentScriptFile
  );
};

require('sdk/test').run(exports);
