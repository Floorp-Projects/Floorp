/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  Arg,
  Option,
  RetVal,
  generateActorSpec,
  types,
} = require("devtools/shared/protocol");

exports.NODE_CREATION_METHODS = [
  "createBufferSource", "createMediaElementSource", "createMediaStreamSource",
  "createMediaStreamDestination", "createScriptProcessor", "createAnalyser",
  "createGain", "createDelay", "createBiquadFilter", "createWaveShaper",
  "createPanner", "createConvolver", "createChannelSplitter", "createChannelMerger",
  "createDynamicsCompressor", "createOscillator", "createStereoPanner"
];

exports.AUTOMATION_METHODS = [
  "setValueAtTime", "linearRampToValueAtTime", "exponentialRampToValueAtTime",
  "setTargetAtTime", "setValueCurveAtTime", "cancelScheduledValues"
];

exports.NODE_ROUTING_METHODS = [
  "connect", "disconnect"
];

types.addActorType("audionode");
const audionodeSpec = generateActorSpec({
  typeName: "audionode",

  methods: {
    getType: { response: { type: RetVal("string") }},
    isBypassed: {
      response: { bypassed: RetVal("boolean") }
    },
    bypass: {
      request: { enable: Arg(0, "boolean") },
      response: { bypassed: RetVal("boolean") }
    },
    setParam: {
      request: {
        param: Arg(0, "string"),
        value: Arg(1, "nullable:primitive")
      },
      response: { error: RetVal("nullable:json") }
    },
    getParam: {
      request: {
        param: Arg(0, "string")
      },
      response: { text: RetVal("nullable:primitive") }
    },
    getParamFlags: {
      request: { param: Arg(0, "string") },
      response: { flags: RetVal("nullable:primitive") }
    },
    getParams: {
      response: { params: RetVal("json") }
    },
    connectParam: {
      request: {
        destActor: Arg(0, "audionode"),
        paramName: Arg(1, "string"),
        output: Arg(2, "nullable:number")
      },
      response: { error: RetVal("nullable:json") }
    },
    connectNode: {
      request: {
        destActor: Arg(0, "audionode"),
        output: Arg(1, "nullable:number"),
        input: Arg(2, "nullable:number")
      },
      response: { error: RetVal("nullable:json") }
    },
    disconnect: {
      request: { output: Arg(0, "nullable:number") },
      response: { error: RetVal("nullable:json") }
    },
    getAutomationData: {
      request: { paramName: Arg(0, "string") },
      response: { values: RetVal("nullable:json") }
    },
    addAutomationEvent: {
      request: {
        paramName: Arg(0, "string"),
        eventName: Arg(1, "string"),
        args: Arg(2, "nullable:json")
      },
      response: { error: RetVal("nullable:json") }
    },
  }
});

exports.audionodeSpec = audionodeSpec;

const webAudioSpec = generateActorSpec({
  typeName: "webaudio",

  /**
   * Events emitted by this actor.
   */
  events: {
    "start-context": {
      type: "startContext"
    },
    "connect-node": {
      type: "connectNode",
      source: Option(0, "audionode"),
      dest: Option(0, "audionode")
    },
    "disconnect-node": {
      type: "disconnectNode",
      source: Arg(0, "audionode")
    },
    "connect-param": {
      type: "connectParam",
      source: Option(0, "audionode"),
      dest: Option(0, "audionode"),
      param: Option(0, "string")
    },
    "change-param": {
      type: "changeParam",
      source: Option(0, "audionode"),
      param: Option(0, "string"),
      value: Option(0, "string")
    },
    "create-node": {
      type: "createNode",
      source: Arg(0, "audionode")
    },
    "destroy-node": {
      type: "destroyNode",
      source: Arg(0, "audionode")
    },
    "automation-event": {
      type: "automationEvent",
      node: Option(0, "audionode"),
      paramName: Option(0, "string"),
      eventName: Option(0, "string"),
      args: Option(0, "json")
    }
  },

  methods: {
    getDefinition: {
      response: { definition: RetVal("json") }
    },
    setup: {
      request: { reload: Option(0, "boolean") },
      oneway: true
    },
    finalize: {
      oneway: true
    }
  }
});

exports.webAudioSpec = webAudioSpec;
