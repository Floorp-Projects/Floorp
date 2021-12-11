/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { DevToolsServer } = require("devtools/server/devtools-server");
const { Pool } = require("devtools/shared/protocol");

// Test parameters
const ROOT_POOLS = 100;
const POOL_DEPTH = 10;
const POOLS_BY_LEVEL = 100;
// Number of Pools that will be added once the environment is set up.
const ADDITIONAL_POOLS = 5000;

add_task(async function() {
  DevToolsServer.init();
  DevToolsServer.registerAllActors();
  const conn = DevToolsServer.connectPipe()._serverConnection;

  info("Add multiple Pools to the connection");
  const pools = setupTestEnvironment(conn);

  let sumResult = 0;

  info("Test how long it takes to manage new Pools");
  let start = performance.now();
  let parentPool = pools[pools.length - 1];
  const newPools = [];
  for (let i = 0; i < ADDITIONAL_POOLS; i++) {
    const pool = new Pool(conn, `${parentPool.label}-${i}`);
    newPools.push(pool);
    parentPool.manage(pool);
  }
  const manageResult = performance.now() - start;
  sumResult += manageResult;

  info("Test how long it takes to manage Pools that were already managed");
  start = performance.now();
  parentPool = pools[pools.length - 2];
  for (const pool of newPools) {
    parentPool.manage(pool);
  }
  const manageAlreadyManagedResult = performance.now() - start;
  sumResult += manageAlreadyManagedResult;

  info("Test how long it takes to unmanage Pools");
  start = performance.now();
  for (const pool of newPools) {
    parentPool.unmanage(pool);
  }
  const unmanageResult = performance.now() - start;
  sumResult += unmanageResult;

  info("Test how long it takes to destroy all the Pools");
  start = performance.now();
  conn.onTransportClosed();
  const destroyResult = performance.now() - start;
  sumResult += destroyResult;

  const PERFHERDER_DATA = {
    framework: {
      name: "devtools",
    },
    suites: [
      {
        name: "server.pool",
        value: sumResult,
        subtests: [
          {
            name: "server.pool.manage",
            value: manageResult,
          },
          {
            name: "server.pool.manage-already-managed",
            value: manageAlreadyManagedResult,
          },
          {
            name: "server.pool.unmanage",
            value: unmanageResult,
          },
          {
            name: "server.pool.destroy",
            value: destroyResult,
          },
        ],
      },
    ],
  };
  info("PERFHERDER_DATA: " + JSON.stringify(PERFHERDER_DATA));
});

// Some Pool operations might be impacted by the number of existing pools in a connection,
// so it's important to have a sizeable number of Pools in order to assert Pool performances.
function setupTestEnvironment(conn) {
  const pools = [];
  for (let i = 0; i < ROOT_POOLS; i++) {
    const rootPool = new Pool(conn, "root-pool-" + i);
    pools.push(rootPool);
    let parent = rootPool;
    for (let j = 0; j < POOL_DEPTH; j++) {
      const intermediatePool = new Pool(conn, `pool-${i}-${j}`);
      pools.push(intermediatePool);
      parent.manage(intermediatePool);

      for (let k = 0; k < POOLS_BY_LEVEL; k++) {
        const pool = new Pool(conn, `pool-${i}-${j}-${k}`);
        pools.push(pool);
        intermediatePool.manage(pool);
      }

      parent = intermediatePool;
    }
  }
  return pools;
}
