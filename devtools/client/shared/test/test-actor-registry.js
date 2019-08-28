/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

(function(exports) {
  const CC = Components.Constructor;

  const { require } = ChromeUtils.import(
    "resource://devtools/shared/Loader.jsm"
  );
  const { fetch } = require("devtools/shared/DevToolsUtils");

  const TEST_URL_ROOT =
    "http://example.com/browser/devtools/client/shared/test/";
  const ACTOR_URL = TEST_URL_ROOT + "test-actor.js";

  // Register a test actor that can operate on the remote document
  exports.registerTestActor = async function(client) {
    // First, instanciate ActorRegistryFront to be able to dynamically register an actor
    const registryFront = await client.mainRoot.getFront("actorRegistry");

    // Then ask to register our test-actor to retrieve its front
    const options = {
      type: { target: true },
      constructor: "TestActor",
      prefix: "testActor",
    };
    const testActorFront = await registryFront.registerActor(
      ACTOR_URL,
      options
    );
    return testActorFront;
  };

  // Load the test actor in a custom sandbox as we can't use SDK module loader with URIs
  const loadFront = async function() {
    const sourceText = await request(ACTOR_URL);
    const principal = CC("@mozilla.org/systemprincipal;1", "nsIPrincipal")();
    const sandbox = Cu.Sandbox(principal);
    sandbox.exports = {};
    sandbox.require = require;
    Cu.evalInSandbox(sourceText, sandbox, "1.8", ACTOR_URL, 1);
    return sandbox.exports;
  };

  // Ensure fetching a live target actor form
  // (helps fetching the test actor registered dynamically)
  const getUpdatedForm = function(client, tab) {
    return client.mainRoot.getTab({ tab: tab }).then(front => front.targetForm);
  };

  // Spawn an instance of the test actor for the given toolbox
  exports.getTestActor = async function(toolbox) {
    const client = toolbox.target.client;
    return getTestActor(client, toolbox.target.tab, toolbox);
  };

  // Sometimes, we need the test actor before opening or without a toolbox then just
  // create a front for the given `tab`
  exports.getTestActorWithoutToolbox = async function(tab) {
    const { DebuggerServer } = require("devtools/server/debugger-server");
    const {
      DebuggerClient,
    } = require("devtools/shared/client/debugger-client");

    // We need to spawn a client instance,
    // but for that we have to first ensure a server is running
    DebuggerServer.init();
    DebuggerServer.registerAllActors();
    const client = new DebuggerClient(DebuggerServer.connectPipe());

    await client.connect();

    // Force connecting to the tab so that the actor is registered in the tab.
    // Calling `getTab` will spawn a DebuggerServer and ActorRegistry in the content
    // process.
    await client.mainRoot.getTab({ tab });

    // We also need to make sure the test actor is registered on the server.
    await exports.registerTestActor(client);

    return getTestActor(client, tab);
  };

  // Fetch the content of a URI
  const request = function(uri) {
    return fetch(uri).then(({ content }) => content);
  };

  const getTestActor = async function(client, tab, toolbox = null) {
    // We may have to update the form in order to get the dynamically registered
    // test actor.
    const form = await getUpdatedForm(client, tab);
    const { TestActorFront } = await loadFront();

    let highlighter;
    if (toolbox) {
      highlighter = (await toolbox.target.getFront("inspector")).highlighter;
    }

    const front = new TestActorFront(client, highlighter);
    // Since we manually instantiate this front instead of going through protocol.js,
    // we have to manually set its actor ID and manage it.
    front.actorID = form.testActor;
    front.manage(front);
    return front;
  };
})(this);
