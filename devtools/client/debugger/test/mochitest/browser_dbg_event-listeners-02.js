/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the eventListeners request works when bound functions are used as
 * event listeners.
 */

const TAB_URL = EXAMPLE_URL + "doc_event-listeners-03.html";

var gClient;
var gTab;

function test() {
  DebuggerServer.init();
  DebuggerServer.registerAllActors();

  let transport = DebuggerServer.connectPipe();
  gClient = new DebuggerClient(transport);
  gClient.connect().then(([aType, aTraits]) => {
    is(aType, "browser",
      "Root actor should identify itself as a browser.");

    addTab(TAB_URL)
      .then((aTab) => {
        gTab = aTab;
        return attachThreadActorForUrl(gClient, TAB_URL);
      })
      .then(pauseDebuggee)
      .then(testEventListeners)
      .then(() => gClient.close())
      .then(finish)
      .catch(aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });
  });
}

function pauseDebuggee(aThreadClient) {
  let deferred = promise.defer();

  gClient.addOneTimeListener("paused", (aEvent, aPacket) => {
    is(aPacket.type, "paused",
      "We should now be paused.");
    is(aPacket.why.type, "debuggerStatement",
      "The debugger statement was hit.");

    deferred.resolve(aThreadClient);
  });

  generateMouseClickInTab(gTab, "content.document.querySelector('button')");

  return deferred.promise;
}

function testEventListeners(aThreadClient) {
  let deferred = promise.defer();

  aThreadClient.eventListeners(aPacket => {
    if (aPacket.error) {
      let msg = "Error getting event listeners: " + aPacket.message;
      ok(false, msg);
      deferred.reject(msg);
      return;
    }

    is(aPacket.listeners.length, 3,
      "Found all event listeners.");

    promise.all(aPacket.listeners.map(listener => {
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
    })).then(listeners => {
      is(listeners.length, 3, "Found three event listeners.");
      for (let l of listeners) {
        let node = l.node;
        ok(node, "There is a node property.");
        ok(node.object, "There is a node object property.");
        ok(node.selector == "window" ||
          content.document.querySelectorAll(node.selector).length == 1,
          "The node property is a unique CSS selector.");

        let func = l.function;
        ok(func, "There is a function property.");
        is(func.type, "object", "The function form is of type 'object'.");
        is(func.class, "Function", "The function form is of class 'Function'.");
        is(func.url, TAB_URL, "The function url is correct.");

        is(l.type, "click", "This is a click event listener.");
        is(l.allowsUntrusted, true,
          "'allowsUntrusted' property has the right value.");
        is(l.inSystemEventGroup, false,
          "'inSystemEventGroup' property has the right value.");
        is(l.isEventHandler, false,
          "'isEventHandler' property has the right value.");
        is(l.capturing, false,
          "Capturing property has the right value.");
      }

      aThreadClient.resume(deferred.resolve);
    });
  });

  return deferred.promise;
}

registerCleanupFunction(function () {
  gClient = null;
});
