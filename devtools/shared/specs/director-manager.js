/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  types,
  Arg,
  Option,
  RetVal,
  generateActorSpec,
} = require("devtools/shared/protocol");

/**
 * Type describing a messageport event
 */
types.addDictType("messageportevent", {
  isTrusted: "boolean",
  data: "nullable:primitive",
  origin: "nullable:string",
  lastEventId: "nullable:string",
  source: "messageport",
  ports: "nullable:array:messageport"
});

const messagePortSpec = generateActorSpec({
  typeName: "messageport",

  /**
   * Events emitted by this actor.
   */
  events: {
    message: {
      type: "message",
      msg: Arg(0, "nullable:messageportevent")
    }
  },

  methods: {
    postMessage: {
      oneway: true,
      request: {
        msg: Arg(0, "nullable:json")
      }
    },
    start: {
      oneway: true,
      request: {}
    },
    close: {
      oneway: true,
      request: {}
    },
    finalize: {
      oneway: true
    },
  },
});

exports.messagePortSpec = messagePortSpec;

/**
 * Type describing a director-script error
 */
types.addDictType("director-script-error", {
  directorScriptId: "string",
  message: "nullable:string",
  stack: "nullable:string",
  fileName: "nullable:string",
  lineNumber: "nullable:number",
  columnNumber: "nullable:number"
});

/**
 * Type describing a director-script attach event
 */
types.addDictType("director-script-attach", {
  directorScriptId: "string",
  url: "string",
  innerId: "number",
  port: "nullable:messageport"
});

/**
 * Type describing a director-script detach event
 */
types.addDictType("director-script-detach", {
  directorScriptId: "string",
  innerId: "number"
});

const directorScriptSpec = generateActorSpec({
  typeName: "director-script",

  /**
   * Events emitted by this actor.
   */
  events: {
    error: {
      type: "error",
      data: Arg(0, "director-script-error")
    },
    attach: {
      type: "attach",
      data: Arg(0, "director-script-attach")
    },
    detach: {
      type: "detach",
      data: Arg(0, "director-script-detach")
    }
  },

  methods: {
    setup: {
      request: {
        reload: Option(0, "boolean"),
        skipAttach: Option(0, "boolean")
      },
      oneway: true
    },
    getMessagePort: {
      request: { },
      response: {
        port: RetVal("nullable:messageport")
      }
    },
    finalize: {
      oneway: true
    },
  },
});

exports.directorScriptSpec = directorScriptSpec;

const directorManagerSpec = generateActorSpec({
  typeName: "director-manager",

  /**
   * Events emitted by this actor.
   */
  events: {
    "director-script-error": {
      type: "error",
      data: Arg(0, "director-script-error")
    },
    "director-script-attach": {
      type: "attach",
      data: Arg(0, "director-script-attach")
    },
    "director-script-detach": {
      type: "detach",
      data: Arg(0, "director-script-detach")
    }
  },

  methods: {
    list: {
      response: {
        directorScripts: RetVal("json")
      }
    },
    enableByScriptIds: {
      oneway: true,
      request: {
        selectedIds: Arg(0, "array:string"),
        reload: Option(1, "boolean")
      }
    },
    disableByScriptIds: {
      oneway: true,
      request: {
        selectedIds: Arg(0, "array:string"),
        reload: Option(1, "boolean")
      }
    },
    getByScriptId: {
      request: {
        scriptId: Arg(0, "string")
      },
      response: {
        directorScript: RetVal("director-script")
      }
    },
    finalize: {
      oneway: true
    },
  },
});

exports.directorManagerSpec = directorManagerSpec;
