"use strict";

const run_test = Test(async function() {
  initTestDebuggerServer();
  const connection = DebuggerServer.connectPipe();
  const client = Async(new DebuggerClient(connection));

  await client.connect();

  const response = await client.mainRoot.protocolDescription();

  assert(response.from == "root");
  assert(typeof response.types === "object");

  await client.close();
});
