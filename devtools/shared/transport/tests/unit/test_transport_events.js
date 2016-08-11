/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function run_test() {
  initTestDebuggerServer();

  add_task(function* () {
    yield test_transport_events("socket", socket_transport);
    yield test_transport_events("local", local_transport);
    DebuggerServer.destroy();
  });

  run_next_test();
}

function* test_transport_events(name, transportFactory) {
  do_print(`Started testing of transport: ${name}`);

  do_check_eq(Object.keys(DebuggerServer._connections).length, 0);

  let transport = yield transportFactory();

  // Transport expects the hooks to be not null
  transport.hooks = {
    onPacket: () => {},
    onClosed: () => {},
  };

  let rootReceived = transport.once("packet", (event, packet) => {
    do_print(`Packet event: ${event} ${JSON.stringify(packet)}`);
    do_check_eq(event, "packet");
    do_check_eq(packet.from, "root");
  });

  transport.ready();
  yield rootReceived;

  let echoSent = transport.once("send", (event, packet) => {
    do_print(`Send event: ${event} ${JSON.stringify(packet)}`);
    do_check_eq(event, "send");
    do_check_eq(packet.to, "root");
    do_check_eq(packet.type, "echo");
  });

  let echoReceived = transport.once("packet", (event, packet) => {
    do_print(`Packet event: ${event} ${JSON.stringify(packet)}`);
    do_check_eq(event, "packet");
    do_check_eq(packet.from, "root");
    do_check_eq(packet.type, "echo");
  });

  transport.send({ to: "root", type: "echo" });
  yield echoSent;
  yield echoReceived;

  let clientClosed = transport.once("close", (event) => {
    do_print(`Close event: ${event}`);
    do_check_eq(event, "close");
  });

  let serverClosed = DebuggerServer.once("connectionchange", (event, type) => {
    do_print(`Server closed`);
    do_check_eq(event, "connectionchange");
    do_check_eq(type, "closed");
  });

  transport.close();

  yield clientClosed;
  yield serverClosed;

  do_print(`Finished testing of transport: ${name}`);
}
