/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var gClient;
var gActors;

/**
 * The purpose of these tests is to verify that it's possible to add actors
 * both before and after the DebuggerServer has been initialized, so addons
 * that add actors don't have to poll the object for its initialization state
 * in order to add actors after initialization but rather can add actors anytime
 * regardless of the object's state.
 */
function run_test() {
  DebuggerServer.addActors("resource://test/pre_init_global_actors.js");
  DebuggerServer.addActors("resource://test/pre_init_tab_actors.js");

  DebuggerServer.init();
  DebuggerServer.registerAllActors();

  DebuggerServer.addActors("resource://test/post_init_global_actors.js");
  DebuggerServer.addActors("resource://test/post_init_tab_actors.js");

  add_test(init);
  add_test(test_pre_init_global_actor);
  add_test(test_pre_init_tab_actor);
  add_test(test_post_init_global_actor);
  add_test(test_post_init_tab_actor);
  add_test(test_stable_global_actor_instances);
  add_test(close_client);
  run_next_test();
}

function init() {
  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient.connect()
    .then(() => gClient.listTabs())
    .then(response => {
      gActors = response;
      run_next_test();
    });
}

function test_pre_init_global_actor() {
  gClient.request({ to: gActors.preInitGlobalActor, type: "ping" },
    function onResponse(response) {
      Assert.equal(response.message, "pong");
      run_next_test();
    }
  );
}

function test_pre_init_tab_actor() {
  gClient.request({ to: gActors.preInitTabActor, type: "ping" },
    function onResponse(response) {
      Assert.equal(response.message, "pong");
      run_next_test();
    }
  );
}

function test_post_init_global_actor() {
  gClient.request({ to: gActors.postInitGlobalActor, type: "ping" },
    function onResponse(response) {
      Assert.equal(response.message, "pong");
      run_next_test();
    }
  );
}

function test_post_init_tab_actor() {
  gClient.request({ to: gActors.postInitTabActor, type: "ping" },
    function onResponse(response) {
      Assert.equal(response.message, "pong");
      run_next_test();
    }
  );
}

// Get the object object, from the server side, for a given actor ID
function getActorInstance(connID, actorID) {
  return DebuggerServer._connections[connID].getActor(actorID);
}

function test_stable_global_actor_instances() {
  // Consider that there is only one connection,
  // and the first one is ours
  let connID = Object.keys(DebuggerServer._connections)[0];
  let postInitGlobalActor = getActorInstance(connID, gActors.postInitGlobalActor);
  let preInitGlobalActor = getActorInstance(connID, gActors.preInitGlobalActor);
  gClient.listTabs(function onListTabs(response) {
    Assert.equal(postInitGlobalActor,
                 getActorInstance(connID, response.postInitGlobalActor));
    Assert.equal(preInitGlobalActor,
                 getActorInstance(connID, response.preInitGlobalActor));
    run_next_test();
  });
}

function close_client() {
  gClient.close().then(() => run_next_test());
}
