/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  types,
  generateActorSpec,
  RetVal,
  Option,
  Arg,
} = require("devtools/shared/protocol");

types.addDictType("console.startlisteners", {
  startedListeners: "array:string",
});

types.addDictType("console.stoplisteners", {
  stoppedListeners: "array:string",
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
      awaitResult: Option(0, "nullable:boolean"),
      errorMessageName: Option(0, "nullable:string"),
      exception: Option(0, "nullable:json"),
      exceptionMessage: Option(0, "nullable:string"),
      exceptionDocURL: Option(0, "nullable:string"),
      exceptionStack: Option(0, "nullable:json"),
      hasException: Option(0, "nullable:boolean"),
      frame: Option(0, "nullable:json"),
      helperResult: Option(0, "nullable:json"),
      input: Option(0, "nullable:string"),
      notes: Option(0, "nullable:string"),
      result: Option(0, "nullable:json"),
      startTime: Option(0, "number"),
      timestamp: Option(0, "number"),
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
      clonedFromContentProcess: Option(0, "nullable:boolean"),
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
    lastPrivateContextExited: {},
    documentEvent: {
      name: Option(0, "string"),
      time: Option(0, "string"),
      hasNativeConsoleAPI: Option(0, "boolean"),
    },
  },

  methods: {
    /**
     * Start the given Web Console listeners.
     *
     * @see webconsoleFront LISTENERS
     * @Arg array events
     *        Array of events you want to start. See this.LISTENERS for
     *        known events.
     */
    startListeners: {
      request: {
        listeners: Arg(0, "array:string"),
      },
      response: RetVal("console.startlisteners"),
    },
    /**
     * Stop the given Web Console listeners.
     *
     * @see webconsoleFront LISTENERS
     * @Arg array events
     *        Array of events you want to stop. See this.LISTENERS for
     *        known events.
     * @Arg function onResponse
     *        Function to invoke when the server response is received.
     */
    stopListeners: {
      request: {
        listeners: Arg(0, "nullable:array:string"),
      },
      response: RetVal("console.stoplisteners"),
    },
    /**
     * Retrieve the cached messages from the server.
     *
     * @see webconsoleFront CACHED_MESSAGES
     * @Arg array types
     *        The array of message types you want from the server. See
     *        this.CACHED_MESSAGES for known types.
     */
    getCachedMessages: {
      request: {
        messageTypes: Arg(0, "array:string"),
      },
      // the return value here has a field "string" which can either be a longStringActor
      // or a plain string. Since we do not have union types, we cannot fully type this
      // response
      response: RetVal("console.cachedmessages"),
    },
    evaluateJSAsync: {
      request: {
        text: Option(0, "string"),
        frameActor: Option(0, "string"),
        url: Option(0, "string"),
        selectedNodeActor: Option(0, "string"),
        selectedObjectActor: Option(0, "string"),
        innerWindowID: Option(0, "number"),
        mapped: Option(0, "nullable:json"),
        eager: Option(0, "nullable:boolean"),
      },
      response: RetVal("console.evaluatejsasync"),
    },
    /**
     * Autocomplete a JavaScript expression.
     *
     * @Arg {String} string
     *      The code you want to autocomplete.
     * @Arg {Number} cursor
     *      Cursor location inside the string. Index starts from 0.
     * @Arg {String} frameActor
     *      The id of the frame actor that made the call.
     * @Arg {String} selectedNodeActor: Actor id of the selected node in the inspector.
     * @Arg {Array} authorizedEvaluations
     *      Array of the properties access which can be executed by the engine.
     *      Example: [["x", "myGetter"], ["x", "myGetter", "y", "anotherGetter"]] to
     *      retrieve properties of `x.myGetter.` and `x.myGetter.y.anotherGetter`.
     */
    autocomplete: {
      request: {
        text: Arg(0, "string"),
        cursor: Arg(1, "nullable:number"),
        frameActor: Arg(2, "nullable:string"),
        selectedNodeActor: Arg(3, "nullable:string"),
        authorizedEvaluations: Arg(4, "nullable:json"),
        expressionVars: Arg(5, "nullable:json"),
      },
      response: RetVal("console.autocomplete"),
    },

    /**
     * Same as clearMessagesCache, but wait for the server response.
     */
    clearMessagesCacheAsync: {
      request: {},
    },

    /**
     * Get Web Console-related preferences on the server.
     *
     * @Arg array preferences
     *        An array with the preferences you want to retrieve.
     */
    getPreferences: {
      request: {
        preferences: Arg(0, "array:string"),
      },
      response: RetVal("json"),
    },
    /**
     * Set Web Console-related preferences on the server.
     *
     * @Arg object preferences
     *        An object with the preferences you want to change.
     */
    setPreferences: {
      request: {
        preferences: Arg(0, "json"),
      },
      response: RetVal("json"),
    },
    /**
     * Send a HTTP request with the given data.
     *
     * @Arg object data
     *        The details of the HTTP request.
     */
    sendHTTPRequest: {
      request: {
        request: Arg(0, "json"),
      },
      response: RetVal("json"),
    },

    blockRequest: {
      request: {
        filter: Arg(0, "json"),
      },
    },

    unblockRequest: {
      request: {
        filter: Arg(0, "json"),
      },
    },

    setBlockedUrls: {
      request: {
        url: Arg(0, "json"),
      },
    },
    getBlockedUrls: {
      request: {},
      response: {
        blockedUrls: RetVal("array:string"),
      },
    },
  },
};

const webconsoleSpec = generateActorSpec(webconsoleSpecPrototype);

exports.webconsoleSpecPrototype = webconsoleSpecPrototype;
exports.webconsoleSpec = webconsoleSpec;
