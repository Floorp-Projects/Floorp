/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Cc, Ci, Cu } = require('chrome');
const { Loader } = require("sdk/test/loader");
const { setTimeout } = require("sdk/timers");
const { emit } = require("sdk/system/events");
const simplePrefs = require("sdk/simple-prefs");
const { prefs: sp } = simplePrefs;
const { defer, resolve, reject, all } = require("sdk/core/promise");
const AddonInstaller = require("sdk/addon/installer");
const fixtures = require("./fixtures");
const { pathFor } = require("sdk/system");
const file = require("sdk/io/file");
const { install, uninstall } = require("sdk/addon/installer");
const { open } = require('sdk/preferences/utils');
const { toFilename } = require('sdk/url');
const { getAddonByID } = require('sdk/addon/manager');
const { ZipWriter } = require('./zip/utils');
const { getTabForId } = require('sdk/tabs/utils');
const { preferencesBranch, id } = require('sdk/self');
const { modelFor } = require('sdk/model/core');
require('sdk/tabs');

const prefsrv = Cc['@mozilla.org/preferences-service;1'].
                    getService(Ci.nsIPrefService);

const specialChars = "!@#$%^&*()_-=+[]{}~`\'\"<>,./?;:";

exports.testIterations = function(assert) {
  sp["test"] = true;
  sp["test.test"] = true;
  let prefAry = [];
  for (var name in sp ) {
    prefAry.push(name);
  }
  assert.ok("test" in sp);
  assert.ok(!sp.getPropertyDescriptor);
  assert.ok(Object.prototype.hasOwnProperty.call(sp, "test"));
  assert.equal(["test", "test.test"].toString(), prefAry.filter((i) => !/^sdk\./.test(i)).sort().toString(), "for (x in y) part 1/2 works");
  assert.equal(["test", "test.test"].toString(), Object.keys(sp).filter((i) => !/^sdk\./.test(i)).sort().toString(), "Object.keys works");

  delete sp["test"];
  delete sp["test.test"];
  prefAry = [];
  for (var name in sp ) {
    prefAry.push(name);
  }
  assert.equal([].toString(), prefAry.filter((i) => !/^sdk\./.test(i)).toString(), "for (x in y) part 2/2 works");
}

exports.testSetGetBool = function(assert) {
  assert.equal(sp.test, undefined, "Value should not exist");
  sp.test = true;
  assert.ok(sp.test, "Value read should be the value previously set");
  delete sp.test;
};

// TEST: setting and getting preferences with special characters work
exports.testSpecialChars = function(assert, done) {
  let chars = specialChars.split("");
  let len = chars.length;

  let count = 0;
  chars.forEach(function(char) {
    let rand = Math.random() + "";
    simplePrefs.on(char, function onPrefChanged() {
      simplePrefs.removeListener(char, onPrefChanged);
      assert.equal(sp[char], rand, "setting pref with a name that is a special char, " + char + ", worked!");
      delete sp[char];

      // end test
      if (++count == len)
        done();
    })
    sp[char] = rand;
  });
};

exports.testSetGetInt = function(assert) {
  assert.equal(sp["test-int"], undefined, "Value should not exist");
  sp["test-int"] = 1;
  assert.equal(sp["test-int"], 1, "Value read should be the value previously set");
  delete sp["test-int"];
};

exports.testSetComplex = function(assert) {
  try {
    sp["test-complex"] = {test: true};
    assert.fail("Complex values are not allowed");
  }
  catch (e) {
    assert.pass("Complex values are not allowed");
  }
};

exports.testSetGetString = function(assert) {
  assert.equal(sp["test-string"], undefined, "Value should not exist");
  sp["test-string"] = "test";
  assert.equal(sp["test-string"], "test", "Value read should be the value previously set");
  delete sp["test-string"];
};

exports.testHasAndRemove = function(assert) {
  sp.test = true;
  assert.ok(("test" in sp), "Value exists");
  delete sp.test;
  assert.equal(sp.test, undefined, "Value should be undefined");
};

exports.testPrefListener = function(assert, done) {
  let listener = function(prefName) {
    simplePrefs.removeListener('test-listener', listener);
    assert.equal(prefName, "test-listen", "The prefs listener heard the right event");
    delete sp["test-listen"];
    done();
  };

  simplePrefs.on("test-listen", listener);

  sp["test-listen"] = true;

  // Wildcard listen
  let toSet = ['wildcard1','wildcard.pref2','wildcard.even.longer.test'];
  let observed = [];

  let wildlistener = function(prefName) {
    if (toSet.indexOf(prefName) > -1) observed.push(prefName);
  };

  simplePrefs.on('',wildlistener);

  toSet.forEach(function(pref) {
    sp[pref] = true;
    delete sp[pref];
  });

  assert.ok((observed.length === 6 && toSet.length === 3),
      "Wildcard lengths inconsistent" + JSON.stringify([observed.length, toSet.length]));

  toSet.forEach(function(pref,ii) {
    assert.equal(observed[2*ii], pref, "Wildcard observed " + pref);
  });

  simplePrefs.removeListener('',wildlistener);

};

exports.testBtnListener = function(assert, done) {
  let name = "test-btn-listen";
  simplePrefs.on(name, function listener() {
    simplePrefs.removeListener(name, listener);
    assert.pass("Button press event was heard");
    done();
  });
  emit((id + "-cmdPressed"), { subject: "", data: name });
};

exports.testPrefRemoveListener = function(assert, done) {
  let counter = 0;

  let listener = function() {
    assert.pass("The prefs listener was not removed yet");

    if (++counter > 1)
      assert.fail("The prefs listener was not removed");

    simplePrefs.removeListener("test-listen2", listener);

    sp["test-listen2"] = false;

    setTimeout(function() {
      assert.pass("The prefs listener was removed");
      delete sp["test-listen2"];
      done();
    }, 250);
  };

  simplePrefs.on("test-listen2", listener);

  // emit change
  sp["test-listen2"] = true;
};

// Bug 710117: Test that simple-pref listeners are removed on unload
exports.testPrefUnloadListener = function(assert, done) {
  let loader = Loader(module);
  let sp = loader.require("sdk/simple-prefs");
  let counter = 0;

  let listener = function() {
    assert.equal(++counter, 1, "This listener should only be called once");

    loader.unload();

    // this may not execute after unload, but definitely shouldn't fire listener
    sp.prefs["test-listen3"] = false;
    // this should execute, but also definitely shouldn't fire listener
    require("sdk/simple-prefs").prefs["test-listen3"] = false;

    delete sp.prefs["test-listen3"];
    done();
  };

  sp.on("test-listen3", listener);

  // emit change
  sp.prefs["test-listen3"] = true;
};


// Bug 710117: Test that simple-pref listeners are removed on unload
exports.testPrefUnloadWildcardListener = function(assert, done) {
  let testpref = "test-wildcard-unload-listener";
  let loader = Loader(module);
  let sp = loader.require("sdk/simple-prefs");
  let counter = 0;

  let listener = function() {
    assert.equal(++counter, 1, "This listener should only be called once");

    loader.unload();

    // this may not execute after unload, but definitely shouldn't fire listener
    sp.prefs[testpref] = false;
    // this should execute, but also definitely shouldn't fire listener
    require("sdk/simple-prefs").prefs[testpref] = false;

    delete sp.prefs[testpref];
    done();
  };

  sp.on("", listener);
  // emit change
  sp.prefs[testpref] = true;
};


// Bug 732919 - JSON.stringify() fails on simple-prefs.prefs
exports.testPrefJSONStringification = function(assert) {
  var sp = require("sdk/simple-prefs").prefs;
  assert.equal(
      Object.keys(sp).join(),
      Object.keys(JSON.parse(JSON.stringify(sp))).join(),
      "JSON stringification should work.");
};

exports.testUnloadOfDynamicPrefGeneration = function*(assert) {
  let loader = Loader(module);
  let branch = prefsrv.getDefaultBranch('extensions.' + preferencesBranch);

  let { enable } = loader.require("sdk/preferences/native-options");

  let addon_id = "test-bootstrap-addon@mozilla.com";
  let xpi_path = file.join(pathFor("ProfD"), addon_id + ".xpi");

  // zip the add-on
  let zip = new ZipWriter(xpi_path);
  assert.pass("start creating the blank xpi");
  zip.addFile("", toFilename(fixtures.url("bootstrap-addon/")));
  yield zip.close();

  // insatll the add-on
  let id = yield install(xpi_path);
  assert.pass('installed');

  // get the addon
  let addon = yield getAddonByID(id);
  assert.equal(id, addon.id, 'addon id: ' + id);

  addon.userDisabled = false;
  assert.ok(!addon.userDisabled, 'the add-on is enabled');
  assert.ok(addon.isActive, 'the add-on is active');

  // setup dynamic prefs
  yield enable({
    id: addon.id,
    preferences: [{
      "name": "test",
      "description": "test",
      "title": "Test",
      "type": "string",
      "value": "default"
    }, {
      "name": "test-int",
      "description": "test",
      "type": "integer",
      "value": 5,
      "title": "How Many?"
    }]
  });

  assert.pass('enabled prefs');

  // show inline prefs
  let { tabId, document } = yield open(addon);
  assert.pass('opened');

  // confirm dynamic pref generation did occur
  let results = document.querySelectorAll("*[data-jetpack-id=\"" + id + "\"]");
  assert.ok(results.length > 0, "the prefs were setup");

  // unload dynamic prefs
  loader.unload();
  assert.pass('unload');

  // hide and show the inline prefs
  let { promise, resolve } = defer();
  modelFor(getTabForId(tabId)).close(resolve);
  yield promise;

  // reopen the add-on prefs page
  ({ tabId, document } = yield open(addon));

  // confirm dynamic pref generation did not occur
  ({ promise, resolve } = defer());
  results = document.querySelectorAll("*[data-jetpack-id=\"" + id + "\"]");
  assert.equal(0, results.length, "the prefs were not setup after unload");
  modelFor(getTabForId(tabId)).close(resolve);
  yield promise;

  // uninstall the add-on
  yield uninstall(id);

  // delete the pref branch
  branch.deleteBranch('');
}

require("sdk/test").run(exports);
