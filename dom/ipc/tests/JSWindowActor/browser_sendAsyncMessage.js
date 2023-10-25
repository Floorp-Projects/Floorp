/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

declTest("asyncMessage testing", {
  async test(browser) {
    let parent = browser.browsingContext.currentWindowGlobal;
    let actorParent = parent.getActor("TestWindow");
    ok(actorParent, "JSWindowActorParent should have value.");

    await ContentTask.spawn(browser, {}, async function () {
      let child = content.windowGlobalChild;
      let actorChild = child.getActor("TestWindow");
      ok(actorChild, "JSWindowActorChild should have value.");

      let promise = new Promise(resolve => {
        actorChild.sendAsyncMessage("init", {});
        actorChild.done = data => resolve(data);
      }).then(data => {
        ok(data.initial, "Initial should be true.");
        ok(data.toParent, "ToParent should be true.");
        ok(data.toChild, "ToChild should be true.");
      });

      await promise;
    });
  },
});

declTest("asyncMessage without both sides", {
  async test(browser) {
    // If we don't create a parent actor, make sure the parent actor
    // gets created by having sent the message.
    await ContentTask.spawn(browser, {}, async function () {
      let child = content.windowGlobalChild;
      let actorChild = child.getActor("TestWindow");
      ok(actorChild, "JSWindowActorChild should have value.");

      let promise = new Promise(resolve => {
        actorChild.sendAsyncMessage("init", {});
        actorChild.done = data => resolve(data);
      }).then(data => {
        ok(data.initial, "Initial should be true.");
        ok(data.toParent, "ToParent should be true.");
        ok(data.toChild, "ToChild should be true.");
      });

      await promise;
    });
  },
});

declTest("asyncMessage can transfer MessagePorts", {
  async test(browser) {
    await ContentTask.spawn(browser, {}, async function () {
      let child = content.windowGlobalChild;
      let actorChild = child.getActor("TestWindow");

      let { port1, port2 } = new MessageChannel();
      actorChild.sendAsyncMessage("messagePort", { port: port2 }, [port2]);
      let reply = await new Promise(resolve => {
        port1.onmessage = message => {
          resolve(message.data);
          port1.close();
        };
      });

      is(reply, "Message sent from parent over a MessagePort.");
    });
  },
});
