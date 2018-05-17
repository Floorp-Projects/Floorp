/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {types, generateActorSpec, RetVal, Option} = require("devtools/shared/protocol");

types.addDictType("console.traits", {
  evaluateJSAsync: "boolean",
  transferredResponseSize: "boolean",
  selectedObjectActor: "boolean",
  fetchCacheDescriptor: "boolean",
});

types.addDictType("console.startlisteners", {
  startedListeners: "array:string",
  nativeConsoleAPI: "boolean",
  traits: "console.traits"
});

types.addDictType("console.autocomplete", {
  matches: "array:string",
  matchProp: "string"
});

types.addDictType("console.evaluatejsasync", {
  resultID: "string",
});

types.addDictType("console.cachedmessages", {
  // this type is a union of two potential return types:
  // { error, message } and { _type, message, timeStamp }
  error: "nullable:string",
  message: "longstring",
  _type: "nullable:string",
  timeStamp: "nullable:string"
});

const webconsoleSpec = generateActorSpec({
  typeName: "console",

  methods: {
    startListeners: {
      request: {
        listeners: Option(0, "array:string"),
      },
      response: RetVal("console.startlisteners")
    },
    stopListeners: {
      request: {
        listeners: Option(0, "nullable:array:string"),
      },
      response: RetVal("array:string")
    },
    getCachedMessages: {
      request: {
        messageTypes: Option(0, "array:string"),
      },
      // the return value here has a field "string" which can either be a longStringActor
      // or a plain string. Since we do not have union types, we cannot fully type this
      // response
      response: RetVal("console.cachedmessages")
    },
    evaluateJS: {
      request: {
        text: Option(0, "string"),
        bindObjectActor: Option(0, "string"),
        frameActor: Option(0, "string"),
        url: Option(0, "string"),
        selectedNodeActor: Option(0, "string"),
        selectedObjectActor: Option(0, "string"),
      },
      response: RetVal("json")
    },
    evaluateJSAsync: {
      request: {
        text: Option(0, "string"),
        bindObjectActor: Option(0, "string"),
        frameActor: Option(0, "string"),
        url: Option(0, "string"),
        selectedNodeActor: Option(0, "string"),
        selectedObjectActor: Option(0, "string"),
      },
      response: RetVal("console.evaluatejsasync")
    },
    autocomplete: {
      request: {
        frameActor: Option(0, "string"),
        text: Option(0, "string"),
        cursor: Option(0, "number")
      },
      response: RetVal("console.autocomplete")
    },
    clearMessagesCache: {
      request: {},
      response: {}
    },
    getPreferences: {
      request: {
        preferences: Option(0, "json")
      },
      response: RetVal("json")
    },
    setPreferences: {
      request: {
        preferences: Option(0, "json")
      },
      response: RetVal("json")
    },
    sendHTTPRequest: {
      request: {
        request: Option(0, "json")
      },
      response: RetVal("json")
    }
  }
});

exports.webconsoleSpec = webconsoleSpec;
