"use strict"

// This function is used to do remote procedure calls from the worker to the
// main thread. It is exposed as a built-in global to every module by the
// worker loader. To make sure the worker loader can access it, it needs to be
// defined before loading the worker loader script below.
this.rpc = function (method, ...params) {
  let id = nextId++;

  postMessage(JSON.stringify({
    type: "rpc",
    method: method,
    params: params,
    id: id
  }));

  let deferred = Promise.defer();
  rpcDeferreds[id] = deferred;
  return deferred.promise;
};

loadSubScript("resource://gre/modules/devtools/shared/worker/loader.js");

var Promise = worker.require("promise");
var { ActorPool } = worker.require("devtools/server/actors/common");
var { ThreadActor } = worker.require("devtools/server/actors/script");
var { TabSources } = worker.require("devtools/server/actors/utils/TabSources");
var makeDebugger = worker.require("devtools/server/actors/utils/make-debugger");
var { DebuggerServer } = worker.require("devtools/server/main");

DebuggerServer.init();
DebuggerServer.createRootActor = function () {
  throw new Error("Should never get here!");
};

var connections = Object.create(null);
var nextId = 0;
var rpcDeferreds = [];

this.addEventListener("message",  function (event) {
  let packet = JSON.parse(event.data);
  switch (packet.type) {
  case "connect":
    // Step 3: Create a connection to the parent.
    let connection = DebuggerServer.connectToParent(packet.id, this);
    connections[packet.id] = {
      connection : connection,
      rpcs: []
    };

    // Step 4: Create a thread actor for the connection to the parent.
    let pool = new ActorPool(connection);
    connection.addActorPool(pool);

    let sources = null;

    let actor = new ThreadActor({
      makeDebugger: makeDebugger.bind(null, {
        findDebuggees: () => {
          return [this.global];
        },

        shouldAddNewGlobalAsDebuggee: () => {
          return true;
        },
      }),

      get sources() {
        if (sources === null) {
          sources = new TabSources(actor);
        }
        return sources;
      }
    }, global);

    pool.addActor(actor);

    // Step 5: Attach to the thread actor.
    //
    // This will cause a packet to be sent over the connection to the parent.
    // Because this connection uses WorkerDebuggerTransport internally, this
    // packet will be sent using WorkerDebuggerGlobalScope.postMessage, causing
    // an onMessage event to be fired on the WorkerDebugger in the main thread.
    actor.onAttach({});
    break;

  case "disconnect":
    connections[packet.id].connection.close();
    break;

  case "rpc":
    let deferred = rpcDeferreds[packet.id];
    delete rpcDeferreds[packet.id];
    if (packet.error) {
        deferred.reject(packet.error);
    }
    deferred.resolve(packet.result);
    break;
  };
});
