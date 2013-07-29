/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// Opening new windows in Fennec causes issues
module.metadata = {
  engines: {
    'Firefox': '*'
  }
};

const { WindowLoader } = require('sdk/windows/loader'),
      { Trait } = require('sdk/deprecated/traits');

const Loader = Trait.compose(
  WindowLoader,
  {
    constructor: function Loader(options) {
      this._onLoad = options.onLoad;
      this._onUnload = options.onUnload;
      if ('window' in options)
        this._window = options.window;
      this._load();
      this.window = this._window;
    },
    window: null,
    _onLoad: null,
    _onUnload: null,
    _tabOptions: []
  }
);

exports['test compositions with missing required properties'] = function(test) {
  test.assertRaises(
    function() WindowLoader.compose({})(),
    'Missing required property: _onLoad',
    'should throw missing required property exception'
  );
  test.assertRaises(
    function() WindowLoader.compose({ _onLoad: null, _tabOptions: null })(),
    'Missing required property: _onUnload',
    'should throw missing required property `_onUnload`'
  );
  test.assertRaises(
    function() WindowLoader.compose({ _onUnload: null, _tabOptions: null })(),
    'Missing required property: _onLoad',
    'should throw missing required property `_onLoad`'
  );
  test.assertRaises(
    function() WindowLoader.compose({ _onUnload: null, _onLoad: null })(),
    'Missing required property: _tabOptions',
    'should throw missing required property `_tabOptions`'
  );
};

exports['test `load` events'] = function(test) {
  test.waitUntilDone();
  let onLoadCalled = false;
  Loader({
    onLoad: function(window) {
      onLoadCalled = true;
      test.assertEqual(
        window, this._window, 'windows should match'
      );
      test.assertEqual(
        window.document.readyState, 'complete', 'window must be fully loaded'
      );
      window.close();
    },
    onUnload: function(window) {
      test.assertEqual(
        window, this._window, 'windows should match'
      );
      test.assertEqual(
        window.document.readyState, 'complete', 'window must be fully loaded'
      );
      test.assert(onLoadCalled, 'load callback is supposed to be called');
      test.done();
    }
  });
};

exports['test removeing listeners'] = function(test) {
  test.waitUntilDone();
  Loader({
    onLoad: function(window) {
      test.assertEqual(
        window, this._window, 'windows should match'
      );
      window.close();
    },
    onUnload: function(window) {
      test.done();
    }
  });
};

exports['test create loader from opened window'] = function(test) {
  test.waitUntilDone();
  let onUnloadCalled = false;
  Loader({
    onLoad: function(window) {
      test.assertEqual(
        window, this._window, 'windows should match'
      );
      test.assertEqual(
        window.document.readyState, 'complete', 'window must be fully loaded'
      );
      Loader({
        window: window,
        onLoad: function(win) {
          test.assertEqual(win, window, 'windows should match');
          window.close();
        },
        onUnload: function(window) {
          test.assert(onUnloadCalled, 'first handler should be called already');
          test.done();
        }
      });
    },
    onUnload: function(window) {
      onUnloadCalled = true;
    }
  });
};
