/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

const { Cu } = require('chrome');
const sp = require('sdk/simple-prefs');
const app = require('sdk/system/xul-app');
const self = require('sdk/self');
const tabs = require('sdk/tabs');
const { preferencesBranch } = require('sdk/self');

const { AddonManager } = Cu.import('resource://gre/modules/AddonManager.jsm', {});

exports.testAOMLocalization = function(assert, done) {
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
                                     'somePreference: getAttributes(unsafeWindow.document.querySelector("setting[data-jetpack-id=\'' + self.id + '\']"))\n' +
                                   '});\n' +
                               '}, 250);\n' +
                             '}, false);\n' +
                             'unsafeWindow.gViewController.commands.cmd_showItemDetails.doCommand(aAddon, true);\n' +
                           '});\n' +
                           'function getAttributes(ele) {\n' +
                             'if (!ele) return {};\n' +
                             'return {\n' +
                               'title: ele.getAttribute("title")\n' +
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
            assert.equal(msg.somePreference.title, 'A', 'somePreference title is correct');
            tab.close(done);
          }
        });
    	}
    });
}

require('sdk/test/runner').runTestsFromModule(module);
