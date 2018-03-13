/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function run_test() {
  initTestDebuggerServer();

  add_task(async function() {
    await test_transport_events("socket", socket_transport);
    await test_transport_events("local", local_transport);
    DebuggerServer.destroy();
  });

  run_next_test();
}

async function test_transport_events(name, transportFactory) {
  info(`Started testing of transport: ${name}`);

  Assert.equal(Object.keys(DebuggerServer._connections).length, 0);

  let transport = await transportFactory();

  // Transport expects the hooks to be not null
  transport.hooks = {
    onPacket: () => {},
    onClosed: () => {},
  };

  let rootReceived = transport.once("packet", packet => {
    info(`Packet event: ${JSON.stringify(packet)}`);
    Assert.equal(packet.from, "root");
  });

  transport.ready();
  await rootReceived;

  let echoSent = transport.once("send", packet => {
    info(`Send event: ${JSON.stringify(packet)}`);
    Assert.equal(packet.to, "root");
    Assert.equal(packet.type, "echo");
  });

  let echoReceived = transport.once("packet", packet => {
    info(`Packet event: ${JSON.stringify(packet)}`);
    Assert.equal(packet.from, "root");
    Assert.equal(packet.type, "echo");
  });

  transport.send({ to: "root", type: "echo" });
  await echoSent;
  await echoReceived;

  let clientClosed = transport.once("close", () => {
    info(`Close event`);
  });

  let serverClosed = DebuggerServer.once("connectionchange", type => {
    info(`Server closed`);
    Assert.equal(type, "closed");
  });

  transport.close();

  await clientClosed;
  await serverClosed;

  info(`Finished testing of transport: ${name}`);
}
