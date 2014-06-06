/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Cc, Ci } = require('chrome');
const { Symbiont } = require('sdk/deprecated/symbiont');
const self = require('sdk/self');
const fixtures = require("./fixtures");
const { close } = require('sdk/window/helpers');
const app = require("sdk/system/xul-app");
const { LoaderWithHookedConsole } = require('sdk/test/loader');
const { set: setPref, get: getPref } = require("sdk/preferences/service");

const DEPRECATE_PREF = "devtools.errorconsole.deprecation_warnings";

function makeWindow() {
  let content =
    '<?xml version="1.0"?>' +
    '<window ' +
    'xmlns="http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul">' +
    '<iframe id="content" type="content"/>' +
    '</window>';
  var url = "data:application/vnd.mozilla.xul+xml;charset=utf-8," +
            encodeURIComponent(content);
  var features = ["chrome", "width=10", "height=10"];

  return Cc["@mozilla.org/embedcomp/window-watcher;1"].
         getService(Ci.nsIWindowWatcher).
         openWindow(null, url, null, features.join(","), null);
}

exports['test:constructing symbiont && validating API'] = function(assert) {
  let contentScript = ["1;", "2;"];
  let contentScriptFile = fixtures.url("test-content-symbiont.js");

  // We can avoid passing a `frame` argument. Symbiont will create one
  // by using HiddenFrame module
  let contentSymbiont = Symbiont({
    contentScriptFile: contentScriptFile,
    contentScript: contentScript,
    contentScriptWhen: "start"
  });

  assert.equal(
    contentScriptFile,
    contentSymbiont.contentScriptFile,
    "There is one contentScriptFile, as specified in options."
  );
  assert.equal(
    contentScript.length,
    contentSymbiont.contentScript.length,
    "There are two contentScripts, as specified in options."
  );
  assert.equal(
    contentScript[0],
    contentSymbiont.contentScript[0],
    "There are two contentScripts, as specified in options."
  );
  assert.equal(
    contentScript[1],
    contentSymbiont.contentScript[1],
    "There are two contentScripts, as specified in options."
  )
  assert.equal(
    contentSymbiont.contentScriptWhen,
    "start",
    "contentScriptWhen is as specified in options."
  );

  contentSymbiont.destroy();
};

exports["test:communication with worker global scope"] = function(assert, done) {
  if (app.is('Fennec')) {
    assert.pass('Test skipped on Fennec');
    done();
  }

  let window = makeWindow();
  let contentSymbiont;

  assert.ok(!!window, 'there is a window');

  function onMessage1(message) {
    assert.equal(message, 1, "Program gets message via onMessage.");
    contentSymbiont.removeListener('message', onMessage1);
    contentSymbiont.on('message', onMessage2);
    contentSymbiont.postMessage(2);
  };

  function onMessage2(message) {
    if (5 == message) {
      close(window).then(done);
    }
    else {
      assert.equal(message, 3, "Program gets message via onMessage2.");
      contentSymbiont.postMessage(4)
    }
  }

  window.addEventListener("load", function onLoad() {
    window.removeEventListener("load", onLoad, false);
    let frame = window.document.getElementById("content");
    contentSymbiont = Symbiont({
      frame: frame,
      contentScript: 'new ' + function() {
        self.postMessage(1);
        self.on("message", function onMessage(message) {
          if (message === 2)
            self.postMessage(3);
          if (message === 4)
            self.postMessage(5);
        });
      } + '()',
      contentScriptWhen: 'ready',
      onMessage: onMessage1
    });

    frame.setAttribute("src", "data:text/html;charset=utf-8,<html><body></body></html>");
  }, false);
};

exports['test:pageWorker'] = function(assert, done) {
  let worker =  Symbiont({
    contentURL: 'about:buildconfig',
    contentScript: 'new ' + function WorkerScope() {
      self.on('message', function(data) {
        if (data.valid)
          self.postMessage('bye!');
      })
      self.postMessage(window.location.toString());
    },
    onMessage: function(msg) {
      if (msg == 'bye!') {
        done()
      } else {
        assert.equal(
          worker.contentURL + '',
          msg
        );
        worker.postMessage({ valid: true });
      }
    }
  });
};

exports["test:document element present on 'start'"] = function(assert, done) {
  let xulApp = require("sdk/system/xul-app");
  let worker = Symbiont({
    contentURL: "about:buildconfig",
    contentScript: "self.postMessage(!!document.documentElement)",
    contentScriptWhen: "start",
    onMessage: function(message) {
      if (xulApp.versionInRange(xulApp.platformVersion, "2.0b6", "*"))
        assert.ok(message, "document element present on 'start'");
      else
        assert.pass("document element not necessarily present on 'start'");
      done();
    }
  });
};

exports["test:content/content deprecation"] = function(assert) {
  let pref = getPref(DEPRECATE_PREF, false);
  setPref(DEPRECATE_PREF, true);

  const { loader, messages } = LoaderWithHookedConsole(module);
  const { Loader, Symbiont, Worker } = loader.require("sdk/content/content");

  assert.equal(messages.length, 3, "Should see three warnings");

  assert.strictEqual(Loader, loader.require('sdk/content/loader').Loader,
    "Loader from content/content is the exact same object as the one from content/loader");

  assert.strictEqual(Symbiont, loader.require('sdk/deprecated/symbiont').Symbiont,
    "Symbiont from content/content is the exact same object as the one from deprecated/symbiont");

  assert.strictEqual(Worker, loader.require('sdk/content/worker').Worker,
    "Worker from content/content is the exact same object as the one from content/worker");

  setPref(DEPRECATE_PREF, pref);
}

require("test").run(exports);
