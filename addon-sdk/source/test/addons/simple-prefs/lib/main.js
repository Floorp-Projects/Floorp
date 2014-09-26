/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

const { Cu } = require('chrome');
const sp = require('sdk/simple-prefs');
const app = require('sdk/system/xul-app');
const { id, preferencesBranch } = require('sdk/self');
const { open } = require('sdk/preferences/utils');
const { getTabForId } = require('sdk/tabs/utils');
const { Tab } = require('sdk/tabs/tab');
const { getAddonByID } = require('sdk/addon/manager');
const { AddonManager } = Cu.import('resource://gre/modules/AddonManager.jsm', {});
require('sdk/tabs');

exports.testDefaultValues = function (assert) {
  assert.equal(sp.prefs.myHiddenInt, 5, 'myHiddenInt default is 5');
  assert.equal(sp.prefs.myInteger, 8, 'myInteger default is 8');
  assert.equal(sp.prefs.somePreference, 'TEST', 'somePreference default is correct');
}

exports.testOptionsType = function*(assert) {
  let addon = yield getAddonByID(id);
  assert.equal(addon.optionsType, AddonManager.OPTIONS_TYPE_INLINE, 'options type is inline');
}

exports.testButton = function(assert, done) {
  open({ id: id }).then(({ tabId, document }) => {
    let tab = Tab({ tab: getTabForId(tabId) });
    sp.once('sayHello', _ => {
      assert.pass('The button was pressed!');
      tab.close(done);
    });

    tab.attach({
      contentScript: 'unsafeWindow.document.querySelector("button[label=\'Click me!\']").click();'
    });
  });
}

if (app.is('Firefox')) {
  exports.testAOM = function(assert, done) {
    open({ id: id }).then(({ tabId }) => {
      let tab = Tab({ tab: getTabForId(tabId) });
      assert.pass('the add-on prefs page was opened.');

      tab.attach({
        contentScriptWhen: "end",
        contentScript: 'self.postMessage({\n' +
                         'someCount: unsafeWindow.document.querySelectorAll("setting[title=\'some-title\']").length,\n' +
                         'somePreference: getAttributes(unsafeWindow.document.querySelector("setting[title=\'some-title\']")),\n' +
                         'myInteger: getAttributes(unsafeWindow.document.querySelector("setting[title=\'my-int\']")),\n' +
                         'myHiddenInt: getAttributes(unsafeWindow.document.querySelector("setting[title=\'hidden-int\']")),\n' +
                         'sayHello: getAttributes(unsafeWindow.document.querySelector("button[label=\'Click me!\']"))\n' +
                       '});\n' +
                       'function getAttributes(ele) {\n' +
                         'if (!ele) return {};\n' +
                         'return {\n' +
                           'pref: ele.getAttribute("pref"),\n' +
                           'type: ele.getAttribute("type"),\n' +
                           'title: ele.getAttribute("title"),\n' +
                           'desc: ele.getAttribute("desc"),\n' +
                           '"data-jetpack-id": ele.getAttribute(\'data-jetpack-id\')\n' +
                         '}\n' +
                       '}\n',
        onMessage: msg => {
          // test against doc caching
          assert.equal(msg.someCount, 1, 'there is exactly one <setting> node for somePreference');

          // test somePreference
          assert.equal(msg.somePreference.type, 'string', 'some pref is a string');
          assert.equal(msg.somePreference.pref, 'extensions.' + id + '.somePreference', 'somePreference path is correct');
          assert.equal(msg.somePreference.title, 'some-title', 'somePreference title is correct');
          assert.equal(msg.somePreference.desc, 'Some short description for the preference', 'somePreference description is correct');
          assert.equal(msg.somePreference['data-jetpack-id'], id, 'data-jetpack-id attribute value is correct');

          // test myInteger
          assert.equal(msg.myInteger.type, 'integer', 'myInteger is a int');
          assert.equal(msg.myInteger.pref, 'extensions.' + id + '.myInteger', 'extensions.test-simple-prefs.myInteger');
          assert.equal(msg.myInteger.title, 'my-int', 'myInteger title is correct');
          assert.equal(msg.myInteger.desc, 'How many of them we have.', 'myInteger desc is correct');
          assert.equal(msg.myInteger['data-jetpack-id'], id, 'data-jetpack-id attribute value is correct');

          // test myHiddenInt
          assert.equal(msg.myHiddenInt.type, undefined, 'myHiddenInt was not displayed');
          assert.equal(msg.myHiddenInt.pref, undefined, 'myHiddenInt was not displayed');
          assert.equal(msg.myHiddenInt.title, undefined, 'myHiddenInt was not displayed');
          assert.equal(msg.myHiddenInt.desc, undefined, 'myHiddenInt was not displayed');

          // test sayHello
          assert.equal(msg.sayHello['data-jetpack-id'], id, 'data-jetpack-id attribute value is correct');

          tab.close(done);
        }
      });
    })
  }

  // run it again, to test against inline options document caching
  // and duplication of <setting> nodes upon re-entry to about:addons
  exports.testAgainstDocCaching = exports.testAOM;
}

exports.testDefaultPreferencesBranch = function(assert) {
  assert.equal(preferencesBranch, id, 'preferencesBranch default the same as self.id');
}

require('sdk/test/runner').runTestsFromModule(module);
