/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the eventListeners request works.
 */

const TAB_URL = EXAMPLE_URL + "doc_event-listeners-01.html";

add_task(async function() {
  DebuggerServer.init();
  DebuggerServer.registerAllActors();

  let transport = DebuggerServer.connectPipe();
  let client = new DebuggerClient(transport);
  let [type, traits] = await client.connect();

  Assert.equal(type, "browser",
    "Root actor should identify itself as a browser.");

  let tab = await addTab(TAB_URL);
  let threadClient = await attachThreadActorForUrl(client, TAB_URL);
  await pauseDebuggee(tab, client, threadClient);
  await testEventListeners(client, threadClient);

  await client.close();
});

function pauseDebuggee(aTab, aClient, aThreadClient) {
  let deferred = promise.defer();

  aClient.addOneTimeListener("paused", (aEvent, aPacket) => {
    is(aPacket.type, "paused",
      "We should now be paused.");
    is(aPacket.why.type, "debuggerStatement",
      "The debugger statement was hit.");

    deferred.resolve(aThreadClient);
  });

  generateMouseClickInTab(aTab, "content.document.querySelector('button')");

  return deferred.promise;
}

async function testEventListeners(aClient, aThreadClient) {
  let packet = await aThreadClient.eventListeners();

  if (packet.error) {
    let msg = "Error getting event listeners: " + packet.message;
    Assert.ok(false, msg);
    return;
  }

  is(packet.listeners.length, 3, "Found all event listeners.");

  let listeners = await promise.all(packet.listeners.map(listener => {
    const lDeferred = promise.defer();
    aThreadClient.pauseGrip(listener.function).getDefinitionSite(aResponse => {
      if (aResponse.error) {
        const msg = "Error getting function definition site: " + aResponse.message;
        ok(false, msg);
        lDeferred.reject(msg);
        return;
      }
      listener.function.url = aResponse.source.url;
      lDeferred.resolve(listener);
    });
    return lDeferred.promise;
  }));

  let types = [];

  for (let l of listeners) {
    info("Listener for the " + l.type + " event.");
    let node = l.node;
    ok(node, "There is a node property.");
    ok(node.object, "There is a node object property.");
    if (node.selector != "window") {
      let nodeCount =
        await ContentTask.spawn(gBrowser.selectedBrowser, node.selector, async (selector) => {
          return content.document.querySelectorAll(selector).length;
        });
      Assert.equal(nodeCount, 1, "The node property is a unique CSS selector.");
    } else {
      Assert.ok(true, "The node property is a unique CSS selector.");
    }

    let func = l.function;
    ok(func, "There is a function property.");
    is(func.type, "object", "The function form is of type 'object'.");
    is(func.class, "Function", "The function form is of class 'Function'.");

    // The onchange handler is an inline string that doesn't have
    // a URL because it's basically eval'ed
    if (l.type !== "change") {
      is(func.url, TAB_URL, "The function url is correct.");
    }

    is(l.allowsUntrusted, true,
      "'allowsUntrusted' property has the right value.");
    is(l.inSystemEventGroup, false,
      "'inSystemEventGroup' property has the right value.");

    types.push(l.type);

    if (l.type == "keyup") {
      is(l.capturing, true,
        "Capturing property has the right value.");
      is(l.isEventHandler, false,
        "'isEventHandler' property has the right value.");
    } else if (l.type == "load") {
      is(l.capturing, false,
        "Capturing property has the right value.");
      is(l.isEventHandler, false,
        "'isEventHandler' property has the right value.");
    } else {
      is(l.capturing, false,
        "Capturing property has the right value.");
      is(l.isEventHandler, true,
        "'isEventHandler' property has the right value.");
    }
  }

  Assert.ok(types.includes("click"), "Found the click handler.");
  Assert.ok(types.includes("change"), "Found the change handler.");
  Assert.ok(types.includes("keyup"), "Found the keyup handler.");

  await aThreadClient.resume();
}
