/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  Arg,
  RetVal,
  generateActorSpec,
  types,
} = require("devtools/shared/protocol");

/**
 * Sent with the eval and reload requests, used to inform the
 * webExtensionInspectedWindowActor about the caller information
 * to be able to evaluate code as being executed from the caller
 * WebExtension sources, or log errors with information that can
 * help the addon developer to more easily identify the affected
 * lines in his own addon code.
 */
types.addDictType("webExtensionCallerInfo", {
  // Information related to the line of code that has originated
  // the request.
  url: "string",
  lineNumber: "nullable:number",

  // The called addonId.
  addonId: "string",
});

/**
 * RDP type related to the inspectedWindow.eval method request.
 */
types.addDictType("webExtensionEvalOptions", {
  frameURL: "nullable:string",
  contextSecurityOrigin: "nullable:string",
  useContentScriptContext: "nullable:boolean",

  // Return the evalResult as a grip (used by the WebExtensions
  // devtools inspector's sidebar.setExpression API method).
  evalResultAsGrip: "nullable:boolean",

  // The actor ID of the node selected in the inspector if any,
  // used to provide the '$0' binding.
  toolboxSelectedNodeActorID: "nullable:string",

  // The actor ID of the console actor,
  // used to provide the 'inspect' binding.
  toolboxConsoleActorID: "nullable:string",
});

/**
 * RDP type related to the inspectedWindow.eval method result errors.
 *
 * This type has been modelled on the same data format
 * used in the corresponding chrome API method.
 */
types.addDictType("webExtensionEvalExceptionInfo", {
  // The following properties are set if the error has not occurred
  // in the evaluated JS code.
  isError: "nullable:boolean",
  code: "nullable:string",
  description: "nullable:string",
  details: "nullable:array:json",

  // The following properties are set if the error has occurred
  // in the evaluated JS code.
  isException: "nullable:string",
  value: "nullable:string",
});

/**
 * RDP type related to the inspectedWindow.eval method result.
 */
types.addDictType("webExtensionEvalResult", {
  // The following properties are set if the evaluation has been
  // completed successfully.
  value: "nullable:json",
  valueGrip: "nullable:json",
  // The following properties are set if the evalutation has been
  // completed with errors.
  exceptionInfo: "nullable:webExtensionEvalExceptionInfo",
});

/**
 * RDP type related to the inspectedWindow.reload method request.
 */
types.addDictType("webExtensionReloadOptions", {
  ignoreCache: "nullable:boolean",
  userAgent: "nullable:string",
  injectedScript: "nullable:string",
});

const webExtensionInspectedWindowSpec = generateActorSpec({
  typeName: "webExtensionInspectedWindow",

  methods: {
    reload: {
      request: {
        webExtensionCallerInfo: Arg(0, "webExtensionCallerInfo"),
        options: Arg(1, "webExtensionReloadOptions"),
      },
    },
    eval: {
      request: {
        webExtensionCallerInfo: Arg(0, "webExtensionCallerInfo"),
        expression: Arg(1, "string"),
        options: Arg(2, "webExtensionEvalOptions"),
      },

      response: {
        evalResult: RetVal("webExtensionEvalResult"),
      },
    },
  },
});

exports.webExtensionInspectedWindowSpec = webExtensionInspectedWindowSpec;
