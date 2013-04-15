/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
const { Loader } = require('sdk/content/loader');
const self = require("sdk/self");

exports['test:contentURL'] = function(test) {
  let loader = Loader(),
      value, emitted = 0, changes = 0;

  test.assertRaises(
    function() loader.contentURL = 4,
    'The `contentURL` option must be a valid URL.',
    'Must throw an exception if `contentURL` is not URL.'
  );
 test.assertRaises(
    function() loader.contentURL = { toString: function() 'Oops' },
    'The `contentURL` option must be a valid URL.',
    'Must throw an exception if `contentURL` is not URL.'
  );

  function listener(e) {
    emitted ++;
    test.assert(
      'contentURL' in e,
      'emitted event must contain "content" property'
    );
    test.assert(
      value,
      '' + e.contentURL,
      'content property of an event must match value'
    );
  }
  loader.on('propertyChange', listener);

  test.assertEqual(
    null,
    loader.contentURL,
    'default value is `null`'
  );
  loader.contentURL = value = 'data:text/html,<html><body>Hi</body><html>';
  test.assertEqual(
    value,
    '' + loader.contentURL,
    'data uri is ok'
  );
  test.assertEqual(
    ++changes,
    emitted,
    'had to emit `propertyChange`'
  );
  loader.contentURL = value;
  test.assertEqual(
    changes,
    emitted,
    'must not emit `propertyChange` if same value is set'
  );

  loader.contentURL = value = 'http://google.com/';
  test.assertEqual(
    value,
    '' + loader.contentURL,
    'value must be set'
  );
  test.assertEqual(
    ++ changes,
    emitted,
    'had to emit `propertyChange`'
  );
  loader.contentURL = value;
  test.assertEqual(
    changes,
    emitted,
    'must not emit `propertyChange` if same value is set'
  );

  loader.removeListener('propertyChange', listener);
  loader.contentURL = value = 'about:blank';
  test.assertEqual(
    value,
    '' + loader.contentURL,
    'contentURL must be an actual value'
  );
  test.assertEqual(
    changes,
    emitted,
    'listener had to be romeved'
  );
};

exports['test:contentScriptWhen'] = function(test) {
  let loader = Loader();
  test.assertEqual(
    'end',
    loader.contentScriptWhen,
    '`contentScriptWhen` defaults to "end"'
  );
  loader.contentScriptWhen = "end";
  test.assertEqual(
    "end",
    loader.contentScriptWhen
  );
  try {
    loader.contentScriptWhen = 'boom';
    test.fail('must throw when wrong value is set');
  } catch(e) {
    test.assertEqual(
      'The `contentScriptWhen` option must be either "start", "ready" or "end".',
      e.message
    );
  }
  loader.contentScriptWhen = null;
  test.assertEqual(
    'end',
    loader.contentScriptWhen,
    '`contentScriptWhen` defaults to "end"'
  );
  loader.contentScriptWhen = "ready";
  test.assertEqual(
    "ready",
    loader.contentScriptWhen
  );
  loader.contentScriptWhen = "start";
  test.assertEqual(
    'start',
    loader.contentScriptWhen
  );
};

exports['test:contentScript'] = function(test) {
  let loader = Loader(), value;
  test.assertEqual(
    null,
    loader.contentScript,
    '`contentScript` defaults to `null`'
  );
  loader.contentScript = value = 'let test = {};';
  test.assertEqual(
    value,
    loader.contentScript
  );
  try {
    loader.contentScript = { 1: value }
    test.fail('must throw when wrong value is set');
  } catch(e) {
    test.assertEqual(
      'The `contentScript` option must be a string or an array of strings.',
      e.message
    );
  }
  try {
    loader.contentScript = ['oue', 2]
    test.fail('must throw when wrong value is set');
  } catch(e) {
    test.assertEqual(
      'The `contentScript` option must be a string or an array of strings.',
      e.message
    );
  }
  loader.contentScript = undefined;
  test.assertEqual(
    null,
    loader.contentScript
  );
  loader.contentScript = value = ["1;", "2;"];
  test.assertEqual(
    value,
    loader.contentScript
  );
};

exports['test:contentScriptFile'] = function(test) {
  let loader = Loader(), value, uri = self.data.url("test-content-loader.js");
  test.assertEqual(
    null,
    loader.contentScriptFile,
    '`contentScriptFile` defaults to `null`'
  );
  loader.contentScriptFile = value = uri;
  test.assertEqual(
    value,
    loader.contentScriptFile
  );
  try {
    loader.contentScriptFile = { 1: uri }
    test.fail('must throw when wrong value is set');
  } catch(e) {
    test.assertEqual(
      'The `contentScriptFile` option must be a local URL or an array of URLs.',
      e.message
    );
  }

  try {
    loader.contentScriptFile = [ 'oue', uri ]
    test.fail('must throw when wrong value is set');
  } catch(e) {
    test.assertEqual(
      'The `contentScriptFile` option must be a local URL or an array of URLs.',
      e.message
    );
  }

  loader.contentScriptFile = undefined;
  test.assertEqual(
    null,
    loader.contentScriptFile
  );
  loader.contentScriptFile = value = [uri];
  test.assertEqual(
    value,
    loader.contentScriptFile
  );
};
