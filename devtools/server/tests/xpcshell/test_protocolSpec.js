"use strict";

const run_test = Test(async function() {
  initTestDebuggerServer();
  const connection = DebuggerServer.connectPipe();
  const client = new DebuggerClient(connection);

  await client.connect();

  const response = await client.mainRoot.protocolDescription();

  assert(response.from == "root", "response.from has expected value");
  assert(
    typeof response.types === "object",
    "response.types has expected type"
  );

  await client.close();
});
