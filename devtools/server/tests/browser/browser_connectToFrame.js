/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test `connectToFrame` method
 */

"use strict";

const {
  connectToFrame,
} = require("devtools/server/connectors/frame-connector");

add_task(async function() {
  // Create a minimal browser with a message manager
  const browser = document.createXULElement("browser");
  browser.setAttribute("type", "content");
  document.body.appendChild(browser);

  const mm = browser.messageManager;

  // Register a test actor in the child process so that we can know if and when
  // this fake actor is destroyed.
  mm.loadFrameScript(
    "data:text/javascript,new " +
      function FrameScriptScope() {
        /* global sendAsyncMessage */
        /* eslint-disable no-shadow */
        const { require } = ChromeUtils.import(
          "resource://devtools/shared/Loader.jsm"
        );
        const { DevToolsServer } = require("devtools/server/devtools-server");
        const {
          ActorRegistry,
        } = require("devtools/server/actors/utils/actor-registry");
        /* eslint-enable no-shadow */

        DevToolsServer.init();

        const { Actor } = require("devtools/shared/protocol/Actor");
        class ConnectToFrameTestActor extends Actor {
          constructor(conn, tab) {
            super(conn);
            dump("instantiate test actor\n");
            this.typeName = "connectToFrameTest";
            this.requestTypes = {
              hello: this.hello,
            };
          }
          hello() {
            return { msg: "world" };
          }

          destroy() {
            sendAsyncMessage("test-actor-destroyed", null);
          }
        }

        ActorRegistry.addTargetScopedActor(
          {
            constructorName: "ConnectToFrameTestActor",
            constructorFun: ConnectToFrameTestActor,
          },
          "connectToFrameTestActor"
        );
      },
    false
  );

  // Instantiate a minimal server
  DevToolsServer.init();
  if (!DevToolsServer.createRootActor) {
    DevToolsServer.registerAllActors();
  }

  async function initAndCloseFirstClient() {
    // Fake a first connection to a browser
    const transport = DevToolsServer.connectPipe();
    const conn = transport._serverConnection;
    const client = new DevToolsClient(transport);
    const actor = await connectToFrame(conn, browser);
    ok(actor.connectToFrameTestActor, "Got the test actor");

    // Ensure sending at least one request to our actor,
    // otherwise it won't be instantiated, nor be destroyed...
    await client.request({
      to: actor.connectToFrameTestActor,
      type: "hello",
    });

    // Connect a second client in parallel to assert that it received a distinct set of
    // target actors
    await initAndCloseSecondClient(actor.connectToFrameTestActor);

    ok(
      DevToolsServer.initialized,
      "DevToolsServer isn't destroyed until all clients are disconnected"
    );

    // Ensure that our test actor got cleaned up;
    // its destroy method should be called
    const onActorDestroyed = new Promise(resolve => {
      mm.addMessageListener("test-actor-destroyed", function listener() {
        mm.removeMessageListener("test-actor-destroyed", listener);
        ok(true, "Actor is cleaned up");
        resolve();
      });
    });

    // Then close the client. That should end up cleaning our test actor
    await client.close();

    await onActorDestroyed;

    // This test loads a frame in the parent process, so that we end up sharing the same
    // DevToolsServer instance
    ok(
      !DevToolsServer.initialized,
      "DevToolsServer is destroyed when all clients are disconnected"
    );
  }

  async function initAndCloseSecondClient(firstActor) {
    // Then fake a second one, that should spawn a new set of target-scoped actors
    const transport = DevToolsServer.connectPipe();
    const conn = transport._serverConnection;
    const client = new DevToolsClient(transport);
    const actor = await connectToFrame(conn, browser);
    ok(
      actor.connectToFrameTestActor,
      "Got a test actor for the second connection"
    );
    isnot(
      actor.connectToFrameTestActor,
      firstActor,
      "We get different actor instances between two connections"
    );
    return client.close();
  }

  await initAndCloseFirstClient();

  DevToolsServer.destroy();
  browser.remove();
});
