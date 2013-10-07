/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const observers = require("sdk/deprecated/observer-service");
const { Cc, Ci } = require("chrome");
const { LoaderWithHookedConsole2 } = require("sdk/test/loader");

exports.testUnloadAndErrorLogging = function(assert) {
  let { loader, messages } = LoaderWithHookedConsole2(module);
  var sbobsvc = loader.require("sdk/deprecated/observer-service");

  var timesCalled = 0;
  var cb = function(subject, data) {
    timesCalled++;
  };
  var badCb = function(subject, data) {
    throw new Error("foo");
  };
  sbobsvc.add("blarg", cb);
  observers.notify("blarg", "yo yo");
  assert.equal(timesCalled, 1);
  sbobsvc.add("narg", badCb);
  observers.notify("narg", "yo yo");

  assert.equal(messages[0], "console.error: " + require("sdk/self").name + ": \n");
  var lines = messages[1].split("\n");
  assert.equal(lines[0], "  Message: Error: foo");
  assert.equal(lines[1], "  Stack:");
  // Keep in mind to update "18" to the line of "throw new Error("foo")"
  assert.ok(lines[2].indexOf(module.uri + ":18") != -1);

  loader.unload();
  observers.notify("blarg", "yo yo");
  assert.equal(timesCalled, 1);
};

exports.testObserverService = function(assert) {
  var ios = Cc['@mozilla.org/network/io-service;1']
            .getService(Ci.nsIIOService);
  var service = Cc["@mozilla.org/observer-service;1"].
                getService(Ci.nsIObserverService);
  var uri = ios.newURI("http://www.foo.com", null, null);
  var timesCalled = 0;
  var lastSubject = null;
  var lastData = null;

  var cb = function(subject, data) {
    timesCalled++;
    lastSubject = subject;
    lastData = data;
  };

  observers.add("blarg", cb);
  service.notifyObservers(uri, "blarg", "some data");
  assert.equal(timesCalled, 1,
                   "observer-service.add() should call callback");
  assert.equal(lastSubject, uri,
                   "observer-service.add() should pass subject");
  assert.equal(lastData, "some data",
                   "observer-service.add() should pass data");

  function customSubject() {}
  function customData() {}
  observers.notify("blarg", customSubject, customData);
  assert.equal(timesCalled, 2,
                   "observer-service.notify() should work");
  assert.equal(lastSubject, customSubject,
                   "observer-service.notify() should pass+wrap subject");
  assert.equal(lastData, customData,
                   "observer-service.notify() should pass data");

  observers.remove("blarg", cb);
  service.notifyObservers(null, "blarg", "some data");
  assert.equal(timesCalled, 2,
                   "observer-service.remove() should work");
};

require('sdk/test').run(exports);
