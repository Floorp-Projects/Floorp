/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

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

exports['test compositions with missing required properties'] = function(assert) {
  assert.throws(
    function() WindowLoader.compose({})(),
    /Missing required property: _onLoad/,
    'should throw missing required property exception'
  );
  assert.throws(
    function() WindowLoader.compose({ _onLoad: null, _tabOptions: null })(),
    /Missing required property: _onUnload/,
    'should throw missing required property `_onUnload`'
  );
  assert.throws(
    function() WindowLoader.compose({ _onUnload: null, _tabOptions: null })(),
    /Missing required property: _onLoad/,
    'should throw missing required property `_onLoad`'
  );
  assert.throws(
    function() WindowLoader.compose({ _onUnload: null, _onLoad: null })(),
    /Missing required property: _tabOptions/,
    'should throw missing required property `_tabOptions`'
  );
};

exports['test `load` events'] = function(assert, done) {
  let onLoadCalled = false;
  Loader({
    onLoad: function(window) {
      onLoadCalled = true;
      assert.equal(window, this._window, 'windows should match');
      assert.equal(
        window.document.readyState, 'complete', 'window must be fully loaded'
      );
      window.close();
    },
    onUnload: function(window) {
      assert.equal(window, this._window, 'windows should match');
      assert.equal(
        window.document.readyState, 'complete', 'window must be fully loaded'
      );
      assert.ok(onLoadCalled, 'load callback is supposed to be called');
      done();
    }
  });
};

exports['test removing listeners'] = function(assert, done) {
  Loader({
    onLoad: function(window) {
      assert.equal(window, this._window, 'windows should match');
      window.close();
    },
    onUnload: done
  });
};

exports['test create loader from opened window'] = function(assert, done) {
  let onUnloadCalled = false;
  Loader({
    onLoad: function(window) {
      assert.equal(window, this._window, 'windows should match');
      assert.equal(window.document.readyState, 'complete', 'window must be fully loaded');
      Loader({
        window: window,
        onLoad: function(win) {
          assert.equal(win, window, 'windows should match');
          window.close();
        },
        onUnload: function(window) {
          assert.ok(onUnloadCalled, 'first handler should be called already');
          done();
        }
      });
    },
    onUnload: function(window) {
      onUnloadCalled = true;
    }
  });
};

require('sdk/test').run(exports);
