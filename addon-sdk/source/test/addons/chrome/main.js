/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict'

const { Cu, Cc, Ci } = require('chrome');
const Request = require('sdk/request').Request;
const { WindowTracker } = require('sdk/deprecated/window-utils');
const { close, open } = require('sdk/window/helpers');
const { data } = require('sdk/self');
const { Panel } = require('sdk/panel');
const { setTimeout } = require("sdk/timers")

const XUL_URL = 'chrome://test/content/new-window.xul'

const { Services } = Cu.import('resource://gre/modules/Services.jsm', {});
const { NetUtil } = Cu.import('resource://gre/modules/NetUtil.jsm', {});

exports.testChromeSkin = function(assert, done) {
  let skinURL = 'chrome://test/skin/style.css';

  Request({
    url: skinURL,
    overrideMimeType: 'text/plain',
    onComplete: function (response) {
      assert.equal(response.text.trim(), 'test{}', 'chrome.manifest skin folder was registered!');
      done();
    }
  }).get();

  assert.pass('requesting ' + skinURL);
}

exports.testChromeContent = function(assert, done) {
  let wt = WindowTracker({
    onTrack: function(window) {
      if (window.document.documentElement.getAttribute('windowtype') === 'test:window') {
      	assert.pass('test xul window was opened');
        wt.unload();

      	close(window).then(done, assert.fail);
      }
    }
  });

  open(XUL_URL).then(
    assert.pass.bind(assert, 'opened ' + XUL_URL),
    assert.fail);

  assert.pass('opening ' + XUL_URL);
}

exports.testChromeLocale = function(assert) {
  let jpLocalePath = Cc['@mozilla.org/chrome/chrome-registry;1'].
                       getService(Ci.nsIChromeRegistry).
                       convertChromeURL(NetUtil.newURI('chrome://test/locale/description.properties')).
                       spec.replace(/(en\-US|ja\-JP)/, 'ja-JP');
  let enLocalePath = jpLocalePath.replace(/ja\-JP/, 'en-US');

  let jpStringBundle = Services.strings.createBundle(jpLocalePath);
  assert.equal(jpStringBundle.GetStringFromName('test'),
               'テスト',
               'locales ja-JP folder was copied correctly');

  let enStringBundle = Services.strings.createBundle(enLocalePath);
  assert.equal(enStringBundle.GetStringFromName('test'),
               'Test',
               'locales en-US folder was copied correctly');
}

exports.testChromeInPanel = function(assert, done) {
  let panel = Panel({
    contentURL: 'chrome://test/content/panel.html',
    contentScriptWhen: 'start',
    contentScriptFile: data.url('panel.js')
  });
  panel.once('show', _ => {
    assert.pass('panel shown');
    panel.port.once('echo', _ => {
      assert.pass('got echo');
      panel.once('hide', _ => {
        assert.pass('panel hidden');
        panel.destroy();
        assert.pass('panel is destroyed');
        done();
      });
      setTimeout(() => panel.hide());
    });
    panel.port.emit('echo');
  });
  panel.show();
}

require('sdk/test/runner').runTestsFromModule(module);
