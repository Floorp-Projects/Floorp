/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

const { Cu } = require('chrome');
const sp = require('sdk/simple-prefs');
const app = require('sdk/system/xul-app');
const self = require('sdk/self');
const tabs = require('sdk/tabs');

const { AddonManager } = Cu.import('resource://gre/modules/AddonManager.jsm', {});

exports.testDefaultValues = function (assert) {
  assert.equal(sp.prefs.myHiddenInt, 5, 'myHiddenInt default is 5');
  assert.equal(sp.prefs.myInteger, 8, 'myInteger default is 8');
  assert.equal(sp.prefs.somePreference, 'TEST', 'somePreference default is correct');
}

exports.testOptionsType = function(assert, done) {
  AddonManager.getAddonByID(self.id, function(aAddon) {
    assert.equal(aAddon.optionsType, AddonManager.OPTIONS_TYPE_INLINE, 'options type is inline');
    done();
  });
}

if (app.is('Firefox')) {
  exports.testAOM = function(assert, done) {
      tabs.open({
      	url: 'about:addons',
      	onReady: function(tab) {
          tab.attach({
            contentScriptWhen: 'end',
          	contentScript: 'function onLoad() {\n' +
                             'unsafeWindow.removeEventListener("load", onLoad, false);\n' +
                             'AddonManager.getAddonByID("' + self.id + '", function(aAddon) {\n' +
                               'unsafeWindow.gViewController.viewObjects.detail.node.addEventListener("ViewChanged", function whenViewChanges() {\n' +
                                 'unsafeWindow.gViewController.viewObjects.detail.node.removeEventListener("ViewChanged", whenViewChanges, false);\n' +
                                 'setTimeout(function() {\n' + // TODO: figure out why this is necessary..
                                     'self.postMessage({\n' +
                                       'somePreference: getAttributes(unsafeWindow.document.querySelector("setting[title=\'some-title\']")),\n' +
                                       'myInteger: getAttributes(unsafeWindow.document.querySelector("setting[title=\'my-int\']")),\n' +
                                       'myHiddenInt: getAttributes(unsafeWindow.document.querySelector("setting[title=\'hidden-int\']"))\n' +
                                     '});\n' +
                                 '}, 250);\n' +
                               '}, false);\n' +
                               'unsafeWindow.gViewController.commands.cmd_showItemDetails.doCommand(aAddon, true);\n' +
                             '});\n' +
                             'function getAttributes(ele) {\n' +
                               'if (!ele) return {};\n' +
                               'return {\n' +
                                 'pref: ele.getAttribute("pref"),\n' +
                                 'type: ele.getAttribute("type"),\n' +
                                 'title: ele.getAttribute("title"),\n' +
                                 'desc: ele.getAttribute("desc")\n' +
                               '}\n' +
                             '}\n' +
                           '}\n' +
                           // Wait for the load event ?
                           'if (document.readyState == "complete") {\n' +
                             'onLoad()\n' +
                           '} else {\n' +
                             'unsafeWindow.addEventListener("load", onLoad, false);\n' +
                           '}\n',
            onMessage: function(msg) {
              // test somePreference
              assert.equal(msg.somePreference.type, 'string', 'some pref is a string');
              assert.equal(msg.somePreference.pref, 'extensions.'+self.id+'.somePreference', 'somePreference path is correct');
              assert.equal(msg.somePreference.title, 'some-title', 'somePreference title is correct');
              assert.equal(msg.somePreference.desc, 'Some short description for the preference', 'somePreference description is correct');

              // test myInteger
              assert.equal(msg.myInteger.type, 'integer', 'myInteger is a int');
              assert.equal(msg.myInteger.pref, 'extensions.'+self.id+'.myInteger', 'extensions.test-simple-prefs.myInteger');
              assert.equal(msg.myInteger.title, 'my-int', 'myInteger title is correct');
              assert.equal(msg.myInteger.desc, 'How many of them we have.', 'myInteger desc is correct');

              // test myHiddenInt
              assert.equal(msg.myHiddenInt.type, undefined, 'myHiddenInt was not displayed');
              assert.equal(msg.myHiddenInt.pref, undefined, 'myHiddenInt was not displayed');
              assert.equal(msg.myHiddenInt.title, undefined, 'myHiddenInt was not displayed');
              assert.equal(msg.myHiddenInt.desc, undefined, 'myHiddenInt was not displayed');

              tab.close(done);
            }
          });
      	}
      });
  }
}

require('sdk/test/runner').runTestsFromModule(module);
