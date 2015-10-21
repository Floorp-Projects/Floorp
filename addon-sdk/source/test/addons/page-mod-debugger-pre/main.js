/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Cu } = require('chrome');
const { PageMod } = require('sdk/page-mod');
const tabs = require('sdk/tabs');
const { closeTab } = require('sdk/tabs/utils');
const promise = require('sdk/core/promise')
const { getMostRecentBrowserWindow } = require('sdk/window/utils');
const { data } = require('sdk/self');
const { set } = require('sdk/preferences/service');

const { require: devtoolsRequire } = Cu.import("resource://devtools/shared/Loader.jsm", {});
const { DebuggerServer } = devtoolsRequire("devtools/server/main");
const { DebuggerClient } = devtoolsRequire("devtools/shared/client/main");

var gClient;
var ok;
var testName = 'testDebugger';
var iframeURL = 'data:text/html;charset=utf-8,' + testName;
var TAB_URL = 'data:text/html;charset=utf-8,' + encodeURIComponent('<iframe src="' + iframeURL + '" />');
TAB_URL = data.url('index.html');
var mod;

exports.testDebugger = function(assert, done) {
  ok = assert.ok.bind(assert);
  assert.pass('starting test');
  set('devtools.debugger.log', true);

  mod = PageMod({
    include: TAB_URL,
    attachTo: ['existing', 'top', 'frame'],
    contentScriptFile: data.url('script.js'),
  });
  ok(true, 'PageMod was created');

  if (!DebuggerServer.initialized) {
    DebuggerServer.init();
    DebuggerServer.addBrowserActors();
  }

  let transport = DebuggerServer.connectPipe();
  gClient = new DebuggerClient(transport);
  gClient.connect((aType, aTraits) => {
    tabs.open({
      url: TAB_URL,
      onLoad: function(tab) {
        assert.pass('tab loaded');

        attachTabActorForUrl(gClient, TAB_URL).
          then(_ => { assert.pass('attachTabActorForUrl called'); return _; }).
          then(attachThread).
          then(testDebuggerStatement).
          then(_ => { assert.pass('testDebuggerStatement called') }).
          then(closeConnection).
          then(_ => { assert.pass('closeConnection called') }).
          then(_ => { tab.close() }).
          then(done).
          then(null, aError => {
            ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
          });
      }
    });
  });
}

function attachThread([aGrip, aResponse]) {
  let deferred = promise.defer();

  // Now attach and resume...
  gClient.request({ to: aResponse.threadActor, type: "attach" }, () => {
    gClient.request({ to: aResponse.threadActor, type: "resume" }, () => {
      ok(true, "Pause wasn't called before we've attached.");
      deferred.resolve([aGrip, aResponse]);
    });
  });

  return deferred.promise;
}

function testDebuggerStatement([aGrip, aResponse]) {
  let deferred = promise.defer();
  ok(aGrip, 'aGrip existss')

  gClient.addListener("paused", (aEvent, aPacket) => {
    ok(true, 'there was a pause event');
    gClient.request({ to: aResponse.threadActor, type: "resume" }, () => {
      ok(true, "The pause handler was triggered on a debugger statement.");
      deferred.resolve();
    });
  });

  let debuggee = getMostRecentBrowserWindow().gBrowser.selectedBrowser.contentWindow.wrappedJSObject;
  debuggee.runDebuggerStatement();
  ok(true, 'called runDebuggerStatement');

  return deferred.promise;
}

function getTabActorForUrl(aClient, aUrl) {
  let deferred = promise.defer();

  aClient.listTabs(aResponse => {
    let tabActor = aResponse.tabs.filter(aGrip => aGrip.url == aUrl).pop();
    deferred.resolve(tabActor);
  });

  return deferred.promise;
}

function attachTabActorForUrl(aClient, aUrl) {
  let deferred = promise.defer();

  getTabActorForUrl(aClient, aUrl).then(aGrip => {
    aClient.attachTab(aGrip.actor, aResponse => {
      deferred.resolve([aGrip, aResponse]);
    });
  });

  return deferred.promise;
}

function closeConnection() {
  let deferred = promise.defer();
  gClient.close(deferred.resolve);
  return deferred.promise;
}

// bug 1042976 - temporary test disable
module.exports = {};

require('sdk/test/runner').runTestsFromModule(module);
