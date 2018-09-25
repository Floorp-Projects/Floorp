/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function run_test() {
  // Allow incoming connections.
  DebuggerServer.init();
  DebuggerServer.registerAllActors();

  add_test(test_lazy_api);
  add_test(manual_remove);
  add_test(cleanup);
  run_next_test();
}

// Bug 988237: Test the new lazy actor loading
function test_lazy_api() {
  let isActorLoaded = false;
  let isActorInstantiated = false;
  function onActorEvent(subject, topic, data) {
    if (data == "loaded") {
      isActorLoaded = true;
    } else if (data == "instantiated") {
      isActorInstantiated = true;
    }
  }
  Services.obs.addObserver(onActorEvent, "actor");
  DebuggerServer.registerModule("xpcshell-test/registertestactors-lazy", {
    prefix: "lazy",
    constructor: "LazyActor",
    type: { global: true, target: true }
  });
  // The actor is immediatly registered, but not loaded
  Assert.ok(DebuggerServer.targetScopedActorFactories.hasOwnProperty("lazyActor"));
  Assert.ok(DebuggerServer.globalActorFactories.hasOwnProperty("lazyActor"));
  Assert.ok(!isActorLoaded);
  Assert.ok(!isActorInstantiated);

  const client = new DebuggerClient(DebuggerServer.connectPipe());
  client.connect().then(function onConnect() {
    client.listTabs().then(onListTabs);
  });
  function onListTabs(response) {
    // On listTabs, the actor is still not loaded,
    // but we can see its name in the list of available actors
    Assert.ok(!isActorLoaded);
    Assert.ok(!isActorInstantiated);
    Assert.ok("lazyActor" in response);

    const {LazyFront} = require("xpcshell-test/registertestactors-lazy");
    const front = LazyFront(client, response);
    front.hello().then(onRequest);
  }
  function onRequest(response) {
    Assert.equal(response, "world");

    // Finally, the actor is loaded on the first request being made to it
    Assert.ok(isActorLoaded);
    Assert.ok(isActorInstantiated);

    Services.obs.removeObserver(onActorEvent, "actor");
    client.close().then(() => run_next_test());
  }
}

function manual_remove() {
  Assert.ok(DebuggerServer.globalActorFactories.hasOwnProperty("lazyActor"));
  DebuggerServer.removeGlobalActor("lazyActor");
  Assert.ok(!DebuggerServer.globalActorFactories.hasOwnProperty("lazyActor"));

  run_next_test();
}

function cleanup() {
  DebuggerServer.destroy();

  // Check that all actors are unregistered on server destruction
  Assert.ok(!DebuggerServer.targetScopedActorFactories.hasOwnProperty("lazyActor"));
  Assert.ok(!DebuggerServer.globalActorFactories.hasOwnProperty("lazyActor"));

  run_next_test();
}

