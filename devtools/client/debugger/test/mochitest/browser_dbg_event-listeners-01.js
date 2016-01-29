/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the eventListeners request works.
 */

const TAB_URL = EXAMPLE_URL + "doc_event-listeners-01.html";

var gClient;
var gTab;

function test() {
  if (!DebuggerServer.initialized) {
    DebuggerServer.init();
    DebuggerServer.addBrowserActors();
  }

  let transport = DebuggerServer.connectPipe();
  gClient = new DebuggerClient(transport);
  gClient.connect().then(([aType, aTraits]) => {
    is(aType, "browser",
      "Root actor should identify itself as a browser.");

    addTab(TAB_URL)
      .then((aTab) => {
        gTab = aTab;
        return attachThreadActorForUrl(gClient, TAB_URL)
      })
      .then(pauseDebuggee)
      .then(testEventListeners)
      .then(closeConnection)
      .then(finish)
      .then(null, aError => {
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
      let types = [];

      for (let l of listeners) {
        info("Listener for the "+l.type+" event.");
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

        // The onchange handler is an inline string that doesn't have
        // a URL because it's basically eval'ed
        if (l.type !== 'change') {
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

      ok(types.indexOf("click") != -1, "Found the click handler.");
      ok(types.indexOf("change") != -1, "Found the change handler.");
      ok(types.indexOf("keyup") != -1, "Found the keyup handler.");

      aThreadClient.resume(deferred.resolve);
    });
  });

  return deferred.promise;
}

function closeConnection() {
  let deferred = promise.defer();
  gClient.close(deferred.resolve);
  return deferred.promise;
}

registerCleanupFunction(function() {
  gClient = null;
});
