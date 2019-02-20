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
  nativeConsoleAPI: "nullable:boolean",
  traits: "console.traits",
});

types.addDictType("console.autocomplete", {
  matches: "array:string",
  matchProp: "string",
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
  timeStamp: "nullable:string",
});

const webconsoleSpecPrototype = {
  typeName: "console",

  events: {
    evaluationResult: {
      resultID: Option(0, "string"),
      type: "evaluationResult",
      awaitResult: Option(0, "nullable:boolean"),
      errorMessageName: Option(0, "nullable:string"),
      exception: Option(0, "nullable:json"),
      exceptionMessage: Option(0, "nullable:string"),
      exceptionDocURL: Option(0, "nullable:string"),
      frame: Option(0, "nullable:json"),
      helperResult: Option(0, "nullable:json"),
      input: Option(0, "nullable:string"),
      notes: Option(0, "nullable:string"),
      result: Option(0, "nullable:json"),
      timestamp: Option(0, "string"),
      topLevelAwaitRejected: Option(0, "nullable:boolean"),
    },
    fileActivity: {
      uri: Option(0, "string"),
    },
    pageError: {
      pageError: Option(0, "json"),
    },
    logMessage: {
      message: Option(0, "json"),
      timeStamp: Option(0, "string"),
    },
    consoleAPICall: {
      message: Option(0, "json"),
    },
    reflowActivity: {
      interruptible: Option(0, "boolean"),
      start: Option(0, "number"),
      end: Option(0, "number"),
      sourceURL: Option(0, "nullable:string"),
      sourceLine: Option(0, "nullable:number"),
      functionName: Option(0, "nullable:string"),
    },
    // This event is modified re-emitted on the client as "networkEvent".
    // In order to avoid a naming collision, we rename the server event.
    serverNetworkEvent: {
      type: "networkEvent",
      eventActor: Option(0, "json"),
    },
    inspectObject: {
      objectActor: Option(0, "json"),
    },
    lastPrivateContextExited: {
    },
    documentEvent: {
      name: Option(0, "string"),
      time: Option(0, "string"),
    },
  },

  methods: {
    startListeners: {
      request: {
        listeners: Option(0, "array:string"),
      },
      response: RetVal("console.startlisteners"),
    },
    stopListeners: {
      request: {
        listeners: Option(0, "nullable:array:string"),
      },
      response: RetVal("array:string"),
    },
    getCachedMessages: {
      request: {
        messageTypes: Option(0, "array:string"),
      },
      // the return value here has a field "string" which can either be a longStringActor
      // or a plain string. Since we do not have union types, we cannot fully type this
      // response
      response: RetVal("console.cachedmessages"),
    },
    evaluateJS: {
      request: {
        text: Option(0, "string"),
        bindObjectActor: Option(0, "string"),
        frameActor: Option(0, "string"),
        url: Option(0, "string"),
        selectedNodeActor: Option(0, "string"),
        selectedObjectActor: Option(0, "string"),
        mapped: Option(0, "nullable:json"),
      },
      response: RetVal("json"),
    },
    evaluateJSAsync: {
      request: {
        text: Option(0, "string"),
        bindObjectActor: Option(0, "string"),
        frameActor: Option(0, "string"),
        url: Option(0, "string"),
        selectedNodeActor: Option(0, "string"),
        selectedObjectActor: Option(0, "string"),
        mapped: Option(0, "nullable:json"),
      },
      response: RetVal("console.evaluatejsasync"),
    },
    autocomplete: {
      request: {
        text: Option(0, "string"),
        cursor: Option(0, "nullable:number"),
        frameActor: Option(0, "nullable:string"),
        selectedNodeActor: Option(0, "nullable:string"),
      },
      response: RetVal("console.autocomplete"),
    },
    clearMessagesCache: {
      oneway: true,
    },
    getPreferences: {
      request: {
        preferences: Option(0, "array:string"),
      },
      response: RetVal("json"),
    },
    setPreferences: {
      request: {
        preferences: Option(0, "json"),
      },
      response: RetVal("json"),
    },
    sendHTTPRequest: {
      request: {
        request: Option(0, "json"),
      },
      response: RetVal("json"),
    },
  },
};

const webconsoleSpec = generateActorSpec(webconsoleSpecPrototype);

exports.webconsoleSpecPrototype = webconsoleSpecPrototype;
exports.webconsoleSpec = webconsoleSpec;
