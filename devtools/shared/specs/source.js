/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Arg, RetVal, generateActorSpec, types } = require("devtools/shared/protocol");

const longstringType = types.getType("longstring");
const arraybufferType = types.getType("arraybuffer");
// The sourcedata type needs some custom marshalling, because it is sometimes
// returned as an arraybuffer and sometimes as a longstring.
types.addType("sourcedata", {
  write: (value, context, detail) => {
    if (value.typeName === "arraybuffer") {
      return arraybufferType.write(value, context, detail);
    }
    return longstringType.write(value, context, detail);
  },
  read: (value, context, detail) => {
    // backward compatibility for FF67 or older: value might be an old style ArrayBuffer
    // actor grip with type="arrayBuffer". The content should be the same so it can be
    // translated to a regular ArrayBufferFront.
    if (value.typeName === "arraybuffer" || value.type === "arrayBuffer") {
      return arraybufferType.read(value, context, detail);
    }
    return longstringType.read(value, context, detail);
  },
});

types.addDictType("sourceposition", {
  line: "number",
  column: "number",
});
types.addDictType("nullablesourceposition", {
  line: "nullable:number",
  column: "nullable:number",
});
types.addDictType("breakpointquery", {
  start: "nullable:nullablesourceposition",
  end: "nullable:nullablesourceposition",
});

types.addDictType("source.onsource", {
  contentType: "nullable:string",
  source: "nullable:sourcedata",
});

const sourceSpec = generateActorSpec({
  typeName: "source",

  methods: {
    getBreakpointPositions: {
      request: {
        query: Arg(0, "nullable:breakpointquery"),
      },
      response: {
        positions: RetVal("array:sourceposition"),
      },
    },
    getBreakpointPositionsCompressed: {
      request: {
        query: Arg(0, "nullable:breakpointquery"),
      },
      response: {
        positions: RetVal("json"),
      },
    },
    getBreakableLines: {
      request: {},
      response: {
        lines: RetVal("json"),
      },
    },
    onSource: {
      // we are sending the type "source" to be compatible
      // with FF67 and older
      request: { type: "source" },
      response: RetVal("source.onsource"),
    },
    setPausePoints: {
      request: {
        pausePoints: Arg(0, "json"),
      },
    },
    blackbox: {
      request: { range: Arg(0, "nullable:json") },
      response: { pausedInSource: RetVal("boolean") },
    },
    unblackbox: {
      request: { range: Arg(0, "nullable:json") },
    },
  },
});

exports.sourceSpec = sourceSpec;
