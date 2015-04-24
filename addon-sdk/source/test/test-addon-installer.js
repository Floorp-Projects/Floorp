/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Cc, Ci, Cu } = require("chrome");
const { pathFor } = require("sdk/system");
const AddonInstaller = require("sdk/addon/installer");
const { on, off } = require("sdk/system/events");
const { setTimeout } = require("sdk/timers");
const fs = require("sdk/io/fs");
const path = require("sdk/fs/path");
const { OS } = require("resource://gre/modules/osfile.jsm");
const { toFilename } = require("sdk/url");

// Retrieve the path to the OS temporary directory:
const tmpDir = pathFor("TmpD");

const profilePath = pathFor("ProfD");
const corruptXPIPath = path.join(profilePath, "sdk-corrupt.xpi");
const testFolderURL = module.uri.split('test-addon-installer.js')[0];
const ADDON_URL = toFilename(testFolderURL + "fixtures/addon-install-unit-test@mozilla.com.xpi");

exports["test Install"] = function*(assert) {
  var ADDON_PATH = OS.Path.join(OS.Constants.Path.tmpDir, "install-test.xpi");

  assert.pass("Copying test add-on " + ADDON_URL + " to " + ADDON_PATH);

  yield OS.File.copy(ADDON_URL, ADDON_PATH);

  assert.pass("Copied test add-on to " + ADDON_PATH);

  // Save all events distpatched by bootstrap.js of the installed addon
  let events = [];
  function eventsObserver({ data }) {
    events.push(data);
  }
  on("addon-install-unit-test", eventsObserver);

  // Install the test addon
  yield AddonInstaller.install(ADDON_PATH).then((id) => {
    assert.equal(id, "addon-install-unit-test@mozilla.com", "`id` is valid");

    // Now uninstall it
    return AddonInstaller.uninstall(id).then(function () {
      // Ensure that bootstrap.js methods of the addon have been called
      // successfully and in the right order
      let expectedEvents = ["install", "startup", "shutdown", "uninstall"];
      assert.equal(JSON.stringify(events),
                   JSON.stringify(expectedEvents),
                   "addon's bootstrap.js functions have been called");

      off("addon-install-unit-test", eventsObserver);
    });
  }, (code) => {
    assert.fail("Install failed: "+code);
    off("addon-install-unit-test", eventsObserver);
  });

  assert.pass("Add-on was uninstalled.");

  yield OS.File.remove(ADDON_PATH);

  assert.pass("Removed the temp file");
};

exports["test Failing Install With Invalid Path"] = function (assert, done) {
  AddonInstaller.install("invalid-path").then(
    function onInstalled(id) {
      assert.fail("Unexpected success");
      done();
    },
    function onFailure(code) {
      assert.equal(code, AddonInstaller.ERROR_FILE_ACCESS,
                       "Got expected error code");
      done();
    }
  );
};

exports["test Failing Install With Invalid File"] = function (assert, done) {
  const content = "bad xpi";
  const path = corruptXPIPath;

  fs.writeFile(path, content, (error) => {
    assert.equal(fs.readFileSync(path).toString(),
                 content,
                 "contet was written");

    AddonInstaller.install(path).then(
      () => {
        assert.fail("Unexpected success");
        fs.unlink(path, done);
      },
      (code) => {
        assert.equal(code, AddonInstaller.ERROR_CORRUPT_FILE,
                         "Got expected error code");
        fs.unlink(path, done);
      }
    );
  });
}

exports["test Update"] = function*(assert) {
  var ADDON_PATH = OS.Path.join(OS.Constants.Path.tmpDir, "update-test.xpi");

  assert.pass("Copying test add-on " + ADDON_URL + " to " + ADDON_PATH);

  yield OS.File.copy(ADDON_URL, ADDON_PATH);

  assert.pass("Copied test add-on to " + ADDON_PATH);

  // Save all events distpatched by bootstrap.js of the installed addon
  let events = [];
  let iteration = 1;
  let eventsObserver = ({data}) => events.push(data);
  on("addon-install-unit-test", eventsObserver);

  yield new Promise(resolve => {
    function onInstalled(id) {
      let prefix = "[" + iteration + "] ";
      assert.equal(id, "addon-install-unit-test@mozilla.com",
                       prefix + "`id` is valid");

      // On 2nd and 3rd iteration, we receive uninstall events from the last
      // previously installed addon
      let expectedEvents =
        iteration == 1
        ? ["install", "startup"]
        : ["shutdown", "uninstall", "install", "startup"];
      assert.equal(JSON.stringify(events),
                       JSON.stringify(expectedEvents),
                       prefix + "addon's bootstrap.js functions have been called");

      if (iteration++ < 3) {
        next();
      }
      else {
        events = [];
        AddonInstaller.uninstall(id).then(function() {
          let expectedEvents = ["shutdown", "uninstall"];
          assert.equal(JSON.stringify(events),
                       JSON.stringify(expectedEvents),
                       prefix + "addon's bootstrap.js functions have been called");

          off("addon-install-unit-test", eventsObserver);
          resolve();
        });
      }
    }
    function onFailure(code) {
      assert.fail("Install failed: "+code);
      off("addon-install-unit-test", eventsObserver);
      resolve();
    }

    function next() {
      events = [];
      AddonInstaller.install(ADDON_PATH).then(onInstalled, onFailure);
    }

    next();
  });

  assert.pass("Add-on was uninstalled.");

  yield OS.File.remove(ADDON_PATH);

  assert.pass("Removed the temp file");
};

exports['test Uninstall failure'] = function (assert, done) {
  AddonInstaller.uninstall('invalid-addon-path').then(
    () => assert.fail('Addon uninstall should not resolve successfully'),
    () => assert.pass('Addon correctly rejected invalid uninstall')
  ).then(done, assert.fail);
};

exports['test Addon Disable and Enable'] = function*(assert) {
  var ADDON_PATH = OS.Path.join(OS.Constants.Path.tmpDir, "disable-enable-test.xpi");

  assert.pass("Copying test add-on " + ADDON_URL + " to " + ADDON_PATH);

  yield OS.File.copy(ADDON_URL, ADDON_PATH);

  assert.pass("Copied test add-on to " + ADDON_PATH);

  let ensureActive = (addonId) => AddonInstaller.isActive(addonId).then(state => {
    assert.equal(state, true, 'Addon should be enabled by default');
    return addonId;
  });
  let ensureInactive = (addonId) => AddonInstaller.isActive(addonId).then(state => {
    assert.equal(state, false, 'Addon should be disabled after disabling');
    return addonId;
  });

  yield AddonInstaller.install(ADDON_PATH)
    .then(ensureActive)
    .then(AddonInstaller.enable) // should do nothing, yet not fail
    .then(ensureActive)
    .then(AddonInstaller.disable)
    .then(ensureInactive)
    .then(AddonInstaller.disable) // should do nothing, yet not fail
    .then(ensureInactive)
    .then(AddonInstaller.enable)
    .then(ensureActive)
    .then(AddonInstaller.uninstall);

  assert.pass("Add-on was uninstalled.");

  yield OS.File.remove(ADDON_PATH);

  assert.pass("Removed the temp file");
};

exports['test Disable failure'] = function (assert, done) {
  AddonInstaller.disable('not-an-id').then(
    () => assert.fail('Addon disable should not resolve successfully'),
    () => assert.pass('Addon correctly rejected invalid disable')
  ).then(done, assert.fail);
};

exports['test Enable failure'] = function (assert, done) {
  AddonInstaller.enable('not-an-id').then(
    () => assert.fail('Addon enable should not resolve successfully'),
    () => assert.pass('Addon correctly rejected invalid enable')
  ).then(done, assert.fail);
};

require("sdk/test").run(exports);
