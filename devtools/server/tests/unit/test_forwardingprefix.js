/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* Exercise prefix-based forwarding of packets to other transports. */

const { RootActor } = require("devtools/server/actors/root");

var gMainConnection, gMainTransport;
var gSubconnection1, gSubconnection2;
var gClient;

function run_test() {
  DebuggerServer.init();

  add_test(createMainConnection);
  add_test(TestNoForwardingYet);
  add_test(createSubconnection1);
  add_test(TestForwardPrefix1OnlyRoot);
  add_test(createSubconnection2);
  add_test(TestForwardPrefix12OnlyRoot);
  add_test(TestForwardPrefix12WithActor1);
  add_test(TestForwardPrefix12WithActor12);
  run_next_test();
}

/*
 * Create a pipe connection, and return an object |{ conn, transport }|,
 * where |conn| is the new DebuggerServerConnection instance, and
 * |transport| is the client side of the transport on which it communicates
 * (that is, packets sent on |transport| go to the new connection, and
 * |transport|'s hooks receive replies).
 *
 * |prefix| is optional; if present, it's the prefix (minus the '/') for
 * actors in the new connection.
 */
function newConnection(prefix) {
  let conn;
  DebuggerServer.createRootActor = function(connection) {
    conn = connection;
    return new RootActor(connection, {});
  };

  const transport = DebuggerServer.connectPipe(prefix);

  return { conn: conn, transport: transport };
}

/* Create the main connection for these tests. */
function createMainConnection() {
  ({ conn: gMainConnection, transport: gMainTransport } = newConnection());
  gClient = new DebuggerClient(gMainTransport);
  gClient.connect().then(([type, traits]) => run_next_test());
}

/*
 * Exchange 'echo' messages with five actors:
 * - root
 * - prefix1/root
 * - prefix1/actor
 * - prefix2/root
 * - prefix2/actor
 *
 * Expect proper echos from those named in |reachables|, and 'noSuchActor'
 * errors from the others. When we've gotten all our replies (errors or
 * otherwise), call |completed|.
 *
 * To avoid deep stacks, we call completed from the next tick.
 */
function tryActors(reachables, completed) {
  let count = 0;

  let outerActor;
  for (outerActor of [ "root",
                       "prefix1/root", "prefix1/actor",
                       "prefix2/root", "prefix2/actor" ]) {
    /*
     * Let each callback capture its own iteration's value; outerActor is
     * local to the whole loop, not to a single iteration.
     */
    const actor = outerActor;

    count++;

    // phone home
    gClient.request(
      { to: actor, type: "echo", value: "tango"},
      (response) => {
        if (reachables.has(actor)) {
          Assert.deepEqual({ from: actor, to: actor,
                             type: "echo", value: "tango" }, response);
        } else {
          Assert.deepEqual({ from: actor, error: "noSuchActor",
                             message: "No such actor for ID: " + actor }, response);
        }

        if (--count == 0) {
          executeSoon(completed, "tryActors callback " + completed.name);
        }
      });
  }
}

/*
 * With no forwarding established, sending messages to root should work,
 * but sending messages to prefixed actor names, or anyone else, should get
 * an error.
 */
function TestNoForwardingYet() {
  tryActors(new Set(["root"]), run_next_test);
}

/*
 * Create a new pipe connection which forwards its reply packets to
 * gMainConnection's client, and to which gMainConnection forwards packets
 * directed to actors whose names begin with |prefix + '/'|, and.
 *
 * Return an object { conn, transport }, as for newConnection.
 */
function newSubconnection(prefix) {
  const { conn, transport } = newConnection(prefix);
  transport.hooks = {
    onPacket: (packet) => gMainConnection.send(packet),
    onClosed: () => {}
  };
  gMainConnection.setForwarding(prefix, transport);

  return { conn: conn, transport: transport };
}

/* Create a second root actor, to which we can forward things. */
function createSubconnection1() {
  const { conn, transport } = newSubconnection("prefix1");
  gSubconnection1 = conn;
  transport.ready();
  gClient.expectReply("prefix1/root", (reply) => run_next_test());
}

// Establish forwarding, but don't put any actors in that server.
function TestForwardPrefix1OnlyRoot() {
  tryActors(new Set(["root", "prefix1/root"]), run_next_test);
}

/* Create a third root actor, to which we can forward things. */
function createSubconnection2() {
  const { conn, transport } = newSubconnection("prefix2");
  gSubconnection2 = conn;
  transport.ready();
  gClient.expectReply("prefix2/root", (reply) => run_next_test());
}

function TestForwardPrefix12OnlyRoot() {
  tryActors(new Set(["root", "prefix1/root", "prefix2/root"]), run_next_test);
}

// A dumb actor that implements 'echo'.
//
// It's okay that both subconnections' actors behave identically, because
// the reply-sending code attaches the replying actor's name to the packet,
// so simply matching the 'from' field in the reply ensures that we heard
// from the right actor.
function EchoActor(connection) {
  this.conn = connection;
}
EchoActor.prototype.actorPrefix = "EchoActor";
EchoActor.prototype.onEcho = function(request) {
  /*
   * Request packets are frozen. Copy request, so that
   * DebuggerServerConnection.onPacket can attach a 'from' property.
   */
  return JSON.parse(JSON.stringify(request));
};
EchoActor.prototype.requestTypes = {
  "echo": EchoActor.prototype.onEcho
};

function TestForwardPrefix12WithActor1() {
  const actor = new EchoActor(gSubconnection1);
  actor.actorID = "prefix1/actor";
  gSubconnection1.addActor(actor);

  tryActors(
    new Set(["root", "prefix1/root", "prefix1/actor", "prefix2/root"]),
    run_next_test
  );
}

function TestForwardPrefix12WithActor12() {
  const actor = new EchoActor(gSubconnection2);
  actor.actorID = "prefix2/actor";
  gSubconnection2.addActor(actor);

  tryActors(
    new Set(["root", "prefix1/root", "prefix1/actor", "prefix2/root", "prefix2/actor"]),
    run_next_test
  );
}
