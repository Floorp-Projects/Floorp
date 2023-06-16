/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { Pool } = require("resource://devtools/shared/protocol/Pool.js");
const {
  DevToolsServerConnection,
} = require("resource://devtools/server/devtools-server-connection.js");
const {
  LocalDebuggerTransport,
} = require("resource://devtools/shared/transport/local-transport.js");

// Helper class to assert how many times a Pool was destroyed
class FakeActor extends Pool {
  constructor(...args) {
    super(...args);
    this.destroyedCount = 0;
  }

  destroy() {
    this.destroyedCount++;
    super.destroy();
  }
}

add_task(async function () {
  const transport = new LocalDebuggerTransport();
  const conn = new DevToolsServerConnection("prefix", transport);

  // Setup a flat pool hierarchy with multiple pools:
  //
  // - pool1
  //   |
  //   \- actor1
  //
  // - pool2
  //   |
  //   |- actor2a
  //   |
  //   \- actor2b
  //
  // From the point of view of the DevToolsServerConnection, the only pools
  // registered in _extraPools should be pool1 and pool2. Even though actor1,
  // actor2a and actor2b extend Pool, they don't manage other pools.
  const actor1 = new FakeActor(conn);
  const pool1 = new Pool(conn, "pool-1");
  pool1.manage(actor1);

  const actor2a = new FakeActor(conn);
  const actor2b = new FakeActor(conn);
  const pool2 = new Pool(conn, "pool-2");
  pool2.manage(actor2a);
  pool2.manage(actor2b);

  ok(!!actor1.actorID, "actor1 has a valid actorID");
  ok(!!actor2a.actorID, "actor2a has a valid actorID");
  ok(!!actor2b.actorID, "actor2b has a valid actorID");

  conn.close();

  equal(actor1.destroyedCount, 1, "actor1 was successfully destroyed");
  equal(actor2a.destroyedCount, 1, "actor2 was successfully destroyed");
  equal(actor2b.destroyedCount, 1, "actor2 was successfully destroyed");
});

add_task(async function () {
  const transport = new LocalDebuggerTransport();
  const conn = new DevToolsServerConnection("prefix", transport);

  // Setup a nested pool hierarchy:
  //
  // - pool
  //   |
  //   \- parentActor
  //      |
  //      \- childActor
  //
  // Since parentActor is also a Pool from the point of view of the
  // DevToolsServerConnection, it will attempt to destroy it when looping on
  // this._extraPools. But since `parentActor` is also a direct child of `pool`,
  // it has already been destroyed by the Pool destroy() mechanism.
  //
  // Here we check that we don't call destroy() too many times on a single Pool.
  // Even though Pool::destroy() is stable when called multiple times, we can't
  // guarantee the same for classes inheriting Pool.
  const childActor = new FakeActor(conn);
  const parentActor = new FakeActor(conn);
  const pool = new Pool(conn, "pool");
  pool.manage(parentActor);
  parentActor.manage(childActor);

  ok(!!parentActor.actorID, "customActor has a valid actorID");
  ok(!!childActor.actorID, "childActor has a valid actorID");

  conn.close();

  equal(parentActor.destroyedCount, 1, "parentActor was destroyed once");
  equal(parentActor.destroyedCount, 1, "customActor was destroyed once");
});
