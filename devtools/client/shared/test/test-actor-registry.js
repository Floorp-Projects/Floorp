/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

(function (exports) {

var Cu = Components.utils;
var Ci = Components.interfaces;
var Cc = Components.classes;
var CC = Components.Constructor;

var { require } = Cu.import("resource://gre/modules/devtools/shared/Loader.jsm", {});
var { NetUtil } = Cu.import("resource://gre/modules/NetUtil.jsm", {});
var promise = require("promise");

var TEST_URL_ROOT = "http://example.com/browser/browser/devtools/shared/test/";
var ACTOR_URL = TEST_URL_ROOT + "test-actor.js";

// Register a test actor that can operate on the remote document
exports.registerTestActor = Task.async(function* (client) {
  // First, instanciate ActorRegistryFront to be able to dynamically
  // register an actor
  let deferred = promise.defer();
  client.listTabs(deferred.resolve);
  let response = yield deferred.promise;
  let { ActorRegistryFront } = require("devtools/server/actors/actor-registry");
  let registryFront = ActorRegistryFront(client, response);

  // Then ask to register our test-actor to retrieve its front
  let options = {
    type: { tab: true },
    constructor: "TestActor",
    prefix: "testActor"
  };
  let testActorFront = yield registryFront.registerActor(ACTOR_URL, options);
  return testActorFront;
});

// Load the test actor in a custom sandbox
// as we can't use SDK module loader with URIs
var loadFront = Task.async(function* () {
  let sourceText = yield request(ACTOR_URL);
  const principal = CC("@mozilla.org/systemprincipal;1", "nsIPrincipal")();
  const sandbox = Cu.Sandbox(principal);
  const exports = sandbox.exports = {};
  sandbox.require = require;
  Cu.evalInSandbox(sourceText, sandbox, "1.8", ACTOR_URL, 1);
  return sandbox.exports;
});

// Ensure fetching a live TabActor form for the targeted app
// (helps fetching the test actor registered dynamically)
var getUpdatedForm = function (client, tab) {
  return client.getTab({tab: tab})
               .then(response => response.tab);
};

// Spawn an instance of the test actor for the given toolbox
exports.getTestActor = Task.async(function* (toolbox) {
  let client = toolbox.target.client;
  return getTestActor(client, toolbox.target.tab, toolbox);
});


// Sometimes, we need the test actor before opening or without a toolbox
// then just create a front for the given `tab`
exports.getTestActorWithoutToolbox = Task.async(function* (tab) {
  let { DebuggerServer } = require("devtools/server/main");
  let { DebuggerClient } = require("devtools/shared/client/main");

  // We need to spawn a client instance,
  // but for that we have to first ensure a server is running
  if (!DebuggerServer.initialized) {
    DebuggerServer.init();
    DebuggerServer.addBrowserActors();
  }
  let client = new DebuggerClient(DebuggerServer.connectPipe());

  let deferred = promise.defer();
  client.connect(deferred.resolve);
  yield deferred.promise;

  return getTestActor(client, tab);
});

// Fetch the content of a URI
var request = function (uri) {
  let deferred = promise.defer();
  try {
    uri = Services.io.newURI(uri, null, null);
  } catch (e) {
    deferred.reject(e);
  }

  NetUtil.asyncFetch(uri, (stream, status, req) => {
    if (!Components.isSuccessCode(status)) {
      deferred.reject(new Error("Request failed with status code = "
                       + status
                       + " after NetUtil.asyncFetch for url = "
                       + uri.spec));
      return;
    }

    let source = NetUtil.readInputStreamToString(stream, stream.available());
    stream.close();
    deferred.resolve(source);
  });
  return deferred.promise;
}

var getTestActor = Task.async(function* (client, tab, toolbox) {
  // We may have to update the form in order to get the dynamically registered
  // test actor.
  let form = yield getUpdatedForm(client, tab);

  let { TestActorFront } = yield loadFront();

  return new TestActorFront(client, form, toolbox);
});

})(this);
