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

types.addDictType("netevent.headers-cookies", {
  name: "string",
  value: "longstring",
});

types.addDictType("netevent.headers", {
  headers: "array:netevent.headers-cookies",
  headersSize: "number",
  rawHeaders: "nullable:longstring",
});

types.addDictType("netevent.cookies", {
  cookies: "array:netevent.headers-cookies",
});

types.addDictType("netevent.postdata.text", {
  text: "longstring",
});

types.addDictType("netevent.postdata", {
  postData: "netevent.postdata.text",
  postDataDiscarded: "boolean",
});

types.addDictType("netevent.cache", {
  content: "json",
});

types.addDictType("netevent.content.content", {
  text: "longstring",
});

types.addDictType("netevent.content", {
  content: "netevent.content.content",
  contentDiscarded: "boolean",
});

types.addDictType("netevent.timings.data", {
  blocked: "number",
  dns: "number",
  ssl: "number",
  connect: "number",
  send: "number",
  wait: "number",
  receive: "number",
});

types.addDictType("netevent.timings", {
  timings: "netevent.timings.data",
  totalTime: "number",
  offsets: "netevent.timings.data",
});

// See NetworkHelper.parseCertificateInfo for more details
types.addDictType("netevent.cert", {
  subject: "json",
  issuer: "json",
  validity: "json",
  fingerprint: "json",
});

types.addDictType("netevent.secinfo", {
  state: "string",
  weaknessReasons: "array:string",
  cipherSuite: "string",
  keaGroupName: "string",
  signatureSchemeName: "string",
  protocolVersion: "string",
  cert: "nullable:netevent.cert",
  certificateTransparency: "nullable:string",
  hsts: "boolean",
  hpkp: "boolean",
  errorMessage: "nullable:string",
});

const networkEventSpec = generateActorSpec({
  typeName: "netEvent",

  events: {
    // All these events end up emitting a `networkEventUpdate` RDP message
    // `updateType` attribute allows to identify which kind of event is emitted.
    // We use individual event at protocol.js level to workaround performance issue
    // with `Option` types. (See bug 1449162)
    "network-event-update:headers": {
      type: "networkEventUpdate",
      updateType: Arg(0, "string"),

      headers: Option(1, "number"),
      headersSize: Option(1, "number"),
    },

    "network-event-update:cookies": {
      type: "networkEventUpdate",
      updateType: Arg(0, "string"),

      cookies: Option(1, "number"),
    },

    "network-event-update:post-data": {
      type: "networkEventUpdate",
      updateType: Arg(0, "string"),

      dataSize: Option(1, "number"),
      discardRequestBody: Option(1, "boolean"),
    },

    "network-event-update:response-start": {
      type: "networkEventUpdate",
      updateType: Arg(0, "string"),

      response: Option(1, "json"),
    },

    "network-event-update:security-info": {
      type: "networkEventUpdate",
      updateType: Arg(0, "string"),

      state: Option(1, "string"),
    },

    "network-event-update:response-content": {
      type: "networkEventUpdate",
      updateType: Arg(0, "string"),

      mimeType: Option(1, "string"),
      contentSize: Option(1, "number"),
      encoding: Option(1, "string"),
      transferredSize: Option(1, "number"),
      discardResponseBody: Option(1, "boolean"),
    },

    "network-event-update:event-timings": {
      type: "networkEventUpdate",
      updateType: Arg(0, "string"),

      totalTime: Option(1, "number"),
    },

    "network-event-update:response-cache": {
      type: "networkEventUpdate",
      updateType: Arg(0, "string"),
    },
  },

  methods: {
    release: {
      // This makes protocol.js call destroy method
      release: true
    },
    getRequestHeaders: {
      request: {},
      response: RetVal("json")
    },
    getRequestCookies: {
      request: {},
      response: RetVal("json")
    },
    getRequestPostData: {
      request: {},
      response: RetVal("json")
    },
    getResponseHeaders: {
      request: {},
      response: RetVal("json")
    },
    getResponseCookies: {
      request: {},
      response: RetVal("json")
    },
    getResponseCache: {
      request: {},
      response: RetVal("json")
    },
    getResponseContent: {
      request: {},
      response: RetVal("json")
    },
    getEventTimings: {
      request: {},
      response: RetVal("json")
    },
    getSecurityInfo: {
      request: {},
      response: RetVal("json")
    },
    getStackTrace: {
      request: {},
      // stacktrace is an "array:string", but not always.
      response: RetVal("json")
    },
  }
});

exports.networkEventSpec = networkEventSpec;
