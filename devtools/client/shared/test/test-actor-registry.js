/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

(function(exports) {
  const { require } = ChromeUtils.import(
    "resource://devtools/shared/Loader.jsm"
  );

  // Spawn an instance of the test actor for the given toolbox
  exports.getTestActor = async function(toolbox) {
    const client = toolbox.target.client;
    return getTestActor(client, toolbox.target.localTab, toolbox);
  };

  // Sometimes, we need the test actor before opening or without a toolbox then just
  // create a front for the given `tab`
  exports.getTestActorWithoutToolbox = async function(tab) {
    const { DevToolsServer } = require("devtools/server/devtools-server");
    const {
      DevToolsClient,
    } = require("devtools/shared/client/devtools-client");

    // We need to spawn a client instance,
    // but for that we have to first ensure a server is running
    DevToolsServer.init();
    DevToolsServer.registerAllActors();
    const client = new DevToolsClient(DevToolsServer.connectPipe());

    await client.connect();

    // Force connecting to the tab so that the actor is registered in the tab.
    // Calling `getTab` will spawn a DevToolsServer and ActorRegistry in the content
    // process.
    await client.mainRoot.getTab({ tab });

    return getTestActor(client, tab);
  };

  const getTestActor = async function(client, tab, toolbox = null) {
    // We may have to update the form in order to get the dynamically registered
    // test actor.
    const targetFront = await client.mainRoot.getTab({ tab });
    const testActorFront = await targetFront.getFront("test");

    let highlighter;
    if (toolbox) {
      highlighter = (await toolbox.target.getFront("inspector")).highlighter;
      testActorFront.setHighlighter(highlighter);
    }

    return testActorFront;
  };
})(this);
