/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Cc, Ci, Cu } = require("chrome");
const { PageMod } = require("sdk/page-mod");
const { testPageMod, handleReadyState, openNewTab,
        contentScriptWhenServer, createLoader } = require("./page-mod/helpers");
const { cleanUI, after } = require("sdk/test/utils");
const { open, getFrames, getMostRecentBrowserWindow, getInnerId } = require("sdk/window/utils");

const { devtools } = Cu.import("resource://gre/modules/devtools/Loader.jsm", {});
const { require: devtoolsRequire } = devtools;
const contentGlobals = devtoolsRequire("devtools/server/content-globals");

// The following adds Debugger constructor to the global namespace.
const { addDebuggerToGlobal } = require('resource://gre/modules/jsdebugger.jsm');
addDebuggerToGlobal(this);

exports.testDebugMetadata = function(assert, done) {
  let dbg = new Debugger;
  let globalDebuggees = [];
  dbg.onNewGlobalObject = function(global) {
    globalDebuggees.push(global);
  }

  let mods = testPageMod(assert, done, "about:", [{
      include: "about:",
      contentScriptWhen: "start",
      contentScript: "null;",
    }], function(win, done) {
      assert.ok(globalDebuggees.some(function(global) {
        try {
          let metadata = Cu.getSandboxMetadata(global.unsafeDereference());
          return metadata && metadata.addonID && metadata.SDKContentScript &&
                 metadata['inner-window-id'] == getInnerId(win);
        } catch(e) {
          // Some of the globals might not be Sandbox instances and thus
          // will cause getSandboxMetadata to fail.
          return false;
        }
      }), "one of the globals is a content script");
      done();
    }
  );
};

exports.testDevToolsExtensionsGetContentGlobals = function(assert, done) {
  let mods = testPageMod(assert, done, "about:", [{
      include: "about:",
      contentScriptWhen: "start",
      contentScript: "null;",
    }], function(win, done) {
      assert.equal(contentGlobals.getContentGlobals({ 'inner-window-id': getInnerId(win) }).length, 1);
      done();
    }
  );
};

after(exports, function*(name, assert) {
  assert.pass("cleaning ui.");
  yield cleanUI();
});

require('sdk/test').run(exports);
