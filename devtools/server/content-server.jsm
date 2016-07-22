/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cu = Components.utils;

const { Services } = Cu.import("resource://gre/modules/Services.jsm", {});
const { DevToolsLoader } = Cu.import("resource://devtools/shared/Loader.jsm", {});

this.EXPORTED_SYMBOLS = ["init"];

function init(msg) {
  // Init a custom, invisible DebuggerServer, in order to not pollute
  // the debugger with all devtools modules, nor break the debugger itself with using it
  // in the same process.
  let devtools = new DevToolsLoader();
  devtools.invisibleToDebugger = true;
  let { DebuggerServer, ActorPool } = devtools.require("devtools/server/main");

  if (!DebuggerServer.initialized) {
    DebuggerServer.init();
  }

  // In case of apps being loaded in parent process, DebuggerServer is already
  // initialized, but child specific actors are not registered.
  // Otherwise, for child process, we need to load actors the first
  // time we load child.js
  DebuggerServer.addChildActors();

  let mm = msg.target;
  mm.QueryInterface(Ci.nsISyncMessageSender);
  let prefix = msg.data.prefix;

  // Connect both parent/child processes debugger servers RDP via message managers
  let conn = DebuggerServer.connectToParent(prefix, mm);
  conn.parentMessageManager = mm;

  let { ChildProcessActor } = devtools.require("devtools/server/actors/child-process");
  let actor = new ChildProcessActor(conn);
  let actorPool = new ActorPool(conn);
  actorPool.addActor(actor);
  conn.addActorPool(actorPool);

  let response = {actor: actor.form()};
  mm.sendAsyncMessage("debug:content-process-actor", response);

  mm.addMessageListener("debug:content-process-destroy", function onDestroy() {
    mm.removeMessageListener("debug:content-process-destroy", onDestroy);

    DebuggerServer.destroy();
  });
}
