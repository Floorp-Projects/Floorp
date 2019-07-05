/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const protocol = require("devtools/shared/protocol");
const { FrontClassWithSpec } = protocol;
const { DebuggerServerConnection } = require("devtools/server/main");
const Services = require("Services");

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
  initialize: function(conn) {
    protocol.Actor.prototype.initialize.call(this, conn);
  },

  isInContent: function() {
    return (
      Services.appinfo.processType == Services.appinfo.PROCESS_TYPE_CONTENT
    );
  },

  spawnInParent: async function(url) {
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
  initialize: function(conn, a1, a2, a3, mm) {
    protocol.Actor.prototype.initialize.call(this, conn);
    // We save all arguments to later assert them in `test` request
    this.conn = conn;
    this.args = [a1, a2, a3];
    this.mm = mm;
  },

  test: function() {
    return {
      args: this.args,
      isInParent:
        Services.appinfo.processType == Services.appinfo.PROCESS_TYPE_DEFAULT,
      conn: this.conn instanceof DebuggerServerConnection,
      // We don't have access to MessageListenerManager in Sandboxes,
      // so fallback to constructor name checks...
      mm: Object.getPrototypeOf(this.mm).constructor.name,
    };
  },
});

class InParentFront extends FrontClassWithSpec(inParentSpec) {}
exports.InParentFront = InParentFront;
