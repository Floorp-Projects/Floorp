/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function check_actors(expect) {
  Assert.equal(expect,
               DebuggerServer.tabActorFactories.hasOwnProperty("registeredActor1"));
  Assert.equal(expect,
               DebuggerServer.tabActorFactories.hasOwnProperty("registeredActor2"));

  Assert.equal(expect,
               DebuggerServer.globalActorFactories.hasOwnProperty("registeredActor2"));
  Assert.equal(expect,
               DebuggerServer.globalActorFactories.hasOwnProperty("registeredActor1"));
}

function run_test() {
  // Allow incoming connections.
  DebuggerServer.init();
  DebuggerServer.registerAllActors();

  add_test(test_deprecated_api);
  add_test(test_lazy_api);
  add_test(cleanup);
  run_next_test();
}

function test_deprecated_api() {
  // The xpcshell-test/ path maps to resource://test/
  DebuggerServer.registerModule("xpcshell-test/registertestactors-01");
  DebuggerServer.registerModule("xpcshell-test/registertestactors-02");

  check_actors(true);

  // Calling registerModule again is just a no-op and doesn't throw
  DebuggerServer.registerModule("xpcshell-test/registertestactors-01");
  DebuggerServer.registerModule("xpcshell-test/registertestactors-02");

  DebuggerServer.unregisterModule("xpcshell-test/registertestactors-01");
  DebuggerServer.unregisterModule("xpcshell-test/registertestactors-02");
  check_actors(false);

  DebuggerServer.registerModule("xpcshell-test/registertestactors-01");
  DebuggerServer.registerModule("xpcshell-test/registertestactors-02");
  check_actors(true);

  run_next_test();
}

// Bug 988237: Test the new lazy actor loading
function test_lazy_api() {
  let isActorLoaded = false;
  let isActorInstanciated = false;
  function onActorEvent(subject, topic, data) {
    if (data == "loaded") {
      isActorLoaded = true;
    } else if (data == "instantiated") {
      isActorInstanciated = true;
    }
  }
  Services.obs.addObserver(onActorEvent, "actor");
  DebuggerServer.registerModule("xpcshell-test/registertestactors-03", {
    prefix: "lazy",
    constructor: "LazyActor",
    type: { global: true, tab: true }
  });
  // The actor is immediatly registered, but not loaded
  Assert.ok(DebuggerServer.tabActorFactories.hasOwnProperty("lazyActor"));
  Assert.ok(DebuggerServer.globalActorFactories.hasOwnProperty("lazyActor"));
  Assert.ok(!isActorLoaded);
  Assert.ok(!isActorInstanciated);

  const client = new DebuggerClient(DebuggerServer.connectPipe());
  client.connect().then(function onConnect() {
    client.listTabs().then(onListTabs);
  });
  function onListTabs(response) {
    // On listTabs, the actor is still not loaded,
    // but we can see its name in the list of available actors
    Assert.ok(!isActorLoaded);
    Assert.ok(!isActorInstanciated);
    Assert.ok("lazyActor" in response);

    const {LazyFront} = require("xpcshell-test/registertestactors-03");
    const front = LazyFront(client, response);
    front.hello().then(onRequest);
  }
  function onRequest(response) {
    Assert.equal(response, "world");

    // Finally, the actor is loaded on the first request being made to it
    Assert.ok(isActorLoaded);
    Assert.ok(isActorInstanciated);

    Services.obs.removeObserver(onActorEvent, "actor");
    client.close().then(() => run_next_test());
  }
}

function cleanup() {
  DebuggerServer.destroy();

  // Check that all actors are unregistered on server destruction
  check_actors(false);
  Assert.ok(!DebuggerServer.tabActorFactories.hasOwnProperty("lazyActor"));
  Assert.ok(!DebuggerServer.globalActorFactories.hasOwnProperty("lazyActor"));

  run_next_test();
}

