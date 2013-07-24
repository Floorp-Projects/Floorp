/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests that the eventListeners request works.

var gClient = null;
var gTab = null;
var gThreadClient = null;
const DEBUGGER_TAB_URL = EXAMPLE_URL + "test-event-listeners.html";

function test()
{
  let transport = DebuggerServer.connectPipe();
  gClient = new DebuggerClient(transport);
  gClient.connect(function(aType, aTraits) {
    gTab = addTab(DEBUGGER_TAB_URL, function() {
      attach_thread_actor_for_url(gClient,
                                  DEBUGGER_TAB_URL,
                                  function(threadClient) {
        gThreadClient = threadClient;
        testEventListeners();
      });
    });
  });
}

function testEventListeners()
{
  gClient.addOneTimeListener("paused", function(aEvent, aPacket) {
    is(aPacket.why.type, "debuggerStatement", "debugger statement was hit.");
    gThreadClient.eventListeners(function(aPacket) {
      is(aPacket.listeners.length, 3, "Found all event listeners.");
      let types = [];
      for (let l of aPacket.listeners) {
        let node = l.node;
        ok(node, "There is a node property.");
        ok(node.object, "There is a node object property.");
        ok(node.selector == "window" ||
           content.document.querySelectorAll(node.selector).length == 1,
           "The node property is a unique CSS selector");
        ok(l.function, "There is a function property.");
        is(l.function.type, "object", "The function form is of type 'object'.");
        is(l.function.class, "Function", "The function form is of class 'Function'.");
        is(l.function.url, DEBUGGER_TAB_URL, "The function url is correct.");
        is(l.allowsUntrusted, true,
           "allowsUntrusted property has the right value.");
        is(l.inSystemEventGroup, false,
           "inSystemEventGroup property has the right value.");

        types.push(l.type);

        if (l.type == "keyup") {
          is(l.capturing, true, "Capturing property has the right value.");
          is(l.isEventHandler, false,
             "isEventHandler property has the right value.");
        } else if (l.type == "load") {
          is(l.capturing, false, "Capturing property has the right value.");
          is(l.isEventHandler, false,
             "isEventHandler property has the right value.");
        } else {
          is(l.capturing, false, "Capturing property has the right value.");
          is(l.isEventHandler, true,
             "isEventHandler property has the right value.");
        }
      }
      ok(types.indexOf("click") != -1, "Found the click handler.");
      ok(types.indexOf("change") != -1, "Found the change handler.");
      ok(types.indexOf("keyup") != -1, "Found the keyup handler.");
      finish_test();
    });
  });

  EventUtils.sendMouseEvent({ type: "click" },
    content.document.querySelector("button"));
}

function finish_test()
{
  gThreadClient.resume(function() {
    gClient.close(finish);
  });
}

registerCleanupFunction(function() {
  removeTab(gTab);
  gTab = null;
  gClient = null;
  gThreadClient = null;
});
