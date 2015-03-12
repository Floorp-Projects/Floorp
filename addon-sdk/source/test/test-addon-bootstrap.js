/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Cu, Cc, Ci } = require("chrome");
const { create, evaluate } = require("./fixtures/bootstrap/utils");

const ROOT = require.resolve("sdk/base64").replace("/sdk/base64.js", "");

// Note: much of this test code is from
// http://dxr.mozilla.org/mozilla-central/source/toolkit/mozapps/extensions/internal/XPIProvider.jsm
const BOOTSTRAP_REASONS = {
  APP_STARTUP     : 1,
  APP_SHUTDOWN    : 2,
  ADDON_ENABLE    : 3,
  ADDON_DISABLE   : 4,
  ADDON_INSTALL   : 5,
  ADDON_UNINSTALL : 6,
  ADDON_UPGRADE   : 7,
  ADDON_DOWNGRADE : 8
};

exports["test install/startup/shutdown/uninstall all return a promise"] = function(assert) {
  let uri = require.resolve("./fixtures/addon/bootstrap.js");
  let id = "test-min-boot@jetpack";
  let bootstrapScope = create({
    id: id,
    uri: uri
  });

  // As we don't want our caller to control the JS version used for the
  // bootstrap file, we run loadSubScript within the context of the
  // sandbox with the latest JS version set explicitly.
  bootstrapScope.ROOT = ROOT;

  evaluate({
    uri: uri,
    scope: bootstrapScope
  });

  let addon = {
    id: id,
    version: "0.0.1",
    resourceURI: {
      spec: uri.replace("bootstrap.js", "")
    }
  };

  let install = bootstrapScope.install(addon, BOOTSTRAP_REASONS.ADDON_INSTALL);
  yield install.then(() => assert.pass("install returns a promise"));

  let startup = bootstrapScope.startup(addon, BOOTSTRAP_REASONS.ADDON_INSTALL);
  yield startup.then(() => assert.pass("startup returns a promise"));

  let shutdown = bootstrapScope.shutdown(addon, BOOTSTRAP_REASONS.ADDON_DISABLE);
  yield shutdown.then(() => assert.pass("shutdown returns a promise"));

  // calling shutdown multiple times is fine
  shutdown = bootstrapScope.shutdown(addon, BOOTSTRAP_REASONS.ADDON_DISABLE);
  yield shutdown.then(() => assert.pass("shutdown returns working promise on multiple calls"));

  let uninstall = bootstrapScope.uninstall(addon, BOOTSTRAP_REASONS.ADDON_UNINSTALL);
  yield uninstall.then(() => assert.pass("uninstall returns a promise"));
}

exports["test minimal bootstrap.js"] = function*(assert) {
  let uri = require.resolve("./fixtures/addon/bootstrap.js");
  let bootstrapScope = create({
    id: "test-min-boot@jetpack",
    uri: uri
  });

  // As we don't want our caller to control the JS version used for the
  // bootstrap file, we run loadSubScript within the context of the
  // sandbox with the latest JS version set explicitly.
  bootstrapScope.ROOT = ROOT;

  assert.equal(typeof bootstrapScope.install, "undefined", "install DNE");
  assert.equal(typeof bootstrapScope.startup, "undefined", "startup DNE");
  assert.equal(typeof bootstrapScope.shutdown, "undefined", "shutdown DNE");
  assert.equal(typeof bootstrapScope.uninstall, "undefined", "uninstall DNE");

  evaluate({
    uri: uri,
    scope: bootstrapScope
  });

  assert.equal(typeof bootstrapScope.install, "function", "install exists");
  assert.equal(typeof bootstrapScope.startup, "function", "startup exists");
  assert.equal(typeof bootstrapScope.shutdown, "function", "shutdown exists");
  assert.equal(typeof bootstrapScope.uninstall, "function", "uninstall exists");

  bootstrapScope.shutdown(null, BOOTSTRAP_REASONS.ADDON_DISABLE);
}

require("sdk/test").run(exports);
