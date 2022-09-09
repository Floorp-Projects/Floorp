/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const protocol = require("devtools/shared/protocol");
const { FrontClassWithSpec } = protocol;
const {
  DevToolsServerConnection,
} = require("devtools/server/devtools-server-connection");

const inContentSpec = protocol.generateActorSpec({
  typeName: "inContent",

  methods: {
    isInContent: {
      request: {},
      response: {
        isInContent: protocol.RetVal("boolean"),
      },
    },
    spawnInParent: {
      request: {
        url: protocol.Arg(0),
      },
      response: protocol.RetVal("json"),
    },
  },
});

exports.InContentActor = protocol.ActorClassWithSpec(inContentSpec, {
  initialize(conn) {
    protocol.Actor.prototype.initialize.call(this, conn);
  },

  isInContent() {
    return (
      Services.appinfo.processType == Services.appinfo.PROCESS_TYPE_CONTENT
    );
  },

  async spawnInParent(url) {
    const actorID = await this.conn.spawnActorInParentProcess(this.actorID, {
      module: url,
      constructor: "InParentActor",
      // In the browser mochitest script, we are asserting these arguments passed to
      // InParentActor constructor
      args: [1, 2, 3],
    });
    return {
      inParentActor: actorID,
    };
  },
});

class InContentFront extends FrontClassWithSpec(inContentSpec) {}
exports.InContentFront = InContentFront;

const inParentSpec = protocol.generateActorSpec({
  typeName: "inParent",

  methods: {
    test: {
      request: {},
      response: protocol.RetVal("json"),
    },
  },
});

exports.InParentActor = protocol.ActorClassWithSpec(inParentSpec, {
  initialize(conn, a1, a2, a3, mm) {
    protocol.Actor.prototype.initialize.call(this, conn);
    // We save all arguments to later assert them in `test` request
    this.conn = conn;
    this.args = [a1, a2, a3];
    this.mm = mm;
  },

  test() {
    return {
      args: this.args,
      isInParent:
        Services.appinfo.processType == Services.appinfo.PROCESS_TYPE_DEFAULT,
      conn: this.conn instanceof DevToolsServerConnection,
      // We don't have access to MessageListenerManager in Sandboxes,
      // so fallback to constructor name checks...
      mm: Object.getPrototypeOf(this.mm).constructor.name,
    };
  },
});

class InParentFront extends FrontClassWithSpec(inParentSpec) {}
exports.InParentFront = InParentFront;
