/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  Arg,
  Option,
  RetVal,
  generateActorSpec,
  types
} = require("devtools/shared/protocol");

types.addType("profiler-data", {
  // On Fx42+, the profile is only deserialized on the front; older
  // servers will get the profiler data as an object from nsIProfiler,
  // causing one parse/stringify cycle, then again implicitly in a packet.
  read: (v) => {
    if (typeof v.profile === "string") {
      // Create a new response object since `profile` is read only.
      let newValue = Object.create(null);
      newValue.profile = JSON.parse(v.profile);
      newValue.currentTime = v.currentTime;
      return newValue;
    }
    return v;
  }
});

const profilerSpec = generateActorSpec({
  typeName: "profiler",

  /**
   * The set of events the ProfilerActor emits over RDP.
   */
  events: {
    "console-api-profiler": {
      data: Arg(0, "json"),
    },
    "profiler-started": {
      data: Arg(0, "json"),
    },
    "profiler-stopped": {
      data: Arg(0, "json"),
    },
    "profiler-status": {
      data: Arg(0, "json"),
    },

    // Only for older geckos, pre-protocol.js ProfilerActor (<Fx42).
    // Emitted on other events as a transition from older profiler events
    // to newer ones.
    "eventNotification": {
      subject: Option(0, "json"),
      topic: Option(0, "string"),
      details: Option(0, "json")
    }
  },

  methods: {
    startProfiler: {
      // Write out every property in the request, since we want all these options to be
      // on the packet's top-level for backwards compatibility, when the profiler actor
      // was not using protocol.js (<Fx42)
      request: {
        entries: Option(0, "nullable:number"),
        interval: Option(0, "nullable:number"),
        features: Option(0, "nullable:array:string"),
        threadFilters: Option(0, "nullable:array:string"),
      },
      response: RetVal("json"),
    },
    stopProfiler: {
      response: RetVal("json"),
    },
    getProfile: {
      request: {
        startTime: Option(0, "nullable:number"),
        stringify: Option(0, "nullable:boolean")
      },
      response: RetVal("profiler-data")
    },
    getFeatures: {
      response: RetVal("json")
    },
    getBufferInfo: {
      response: RetVal("json")
    },
    getStartOptions: {
      response: RetVal("json")
    },
    isActive: {
      response: RetVal("json")
    },
    getSharedLibraryInformation: {
      response: RetVal("json")
    },
    registerEventNotifications: {
      // Explicitly enumerate the arguments
      // @see ProfilerActor#startProfiler
      request: {
        events: Option(0, "nullable:array:string"),
      },
      response: RetVal("json")
    },
    unregisterEventNotifications: {
      // Explicitly enumerate the arguments
      // @see ProfilerActor#startProfiler
      request: {
        events: Option(0, "nullable:array:string"),
      },
      response: RetVal("json")
    },
    setProfilerStatusInterval: {
      request: { interval: Arg(0, "number") },
      oneway: true
    }
  }
});

exports.profilerSpec = profilerSpec;
