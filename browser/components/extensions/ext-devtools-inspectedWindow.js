/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

/* global getDevToolsTargetForContext */

var {
  SpreadArgs,
} = ExtensionUtils;

this.devtools_inspectedWindow = class extends ExtensionAPI {
  getAPI(context) {
    const {
      WebExtensionInspectedWindowFront,
    } = require("devtools/shared/fronts/webextension-inspected-window");

    // Lazily retrieve and store an inspectedWindow actor front per child context.
    let waitForInspectedWindowFront;
    async function getInspectedWindowFront() {
      // If there is not yet a front instance, then a lazily cloned target for the context is
      // retrieved using the DevtoolsParentContextsManager helper (which is an asynchronous operation,
      // because the first time that the target has been cloned, it is not ready to be used to create
      // the front instance until it is connected to the remote debugger successfully).
      const clonedTarget = await getDevToolsTargetForContext(context);
      return new WebExtensionInspectedWindowFront(clonedTarget.client, clonedTarget.form);
    }

    function getToolboxOptions() {
      const options = {};
      const toolbox = context.devToolsToolbox;
      const selectedNode = toolbox.selection;

      if (selectedNode && selectedNode.nodeFront) {
        // If there is a selected node in the inspector, we hand over
        // its actor id to the eval request in order to provide the "$0" binding.
        options.toolboxSelectedNodeActorID = selectedNode.nodeFront.actorID;
      }

      // Provide the console actor ID to implement the "inspect" binding.
      options.toolboxConsoleActorID = toolbox.target.form.consoleActor;

      return options;
    }

    // TODO(rpl): retrive a more detailed callerInfo object, like the filename and
    // lineNumber of the actual extension called, in the child process.
    const callerInfo = {
      addonId: context.extension.id,
      url: context.extension.baseURI.spec,
    };

    return {
      devtools: {
        inspectedWindow: {
          async eval(expression, options) {
            if (!waitForInspectedWindowFront) {
              waitForInspectedWindowFront = getInspectedWindowFront();
            }

            const front = await waitForInspectedWindowFront;

            const evalOptions = Object.assign({}, options, getToolboxOptions());

            const evalResult = await front.eval(callerInfo, expression, evalOptions);

            // TODO(rpl): check for additional undocumented behaviors on chrome
            // (e.g. if we should also print error to the console or set lastError?).
            return new SpreadArgs([evalResult.value, evalResult.exceptionInfo]);
          },
          async reload(options) {
            const {ignoreCache, userAgent, injectedScript} = options || {};

            if (!waitForInspectedWindowFront) {
              waitForInspectedWindowFront = getInspectedWindowFront();
            }

            const front = await waitForInspectedWindowFront;
            front.reload(callerInfo, {ignoreCache, userAgent, injectedScript});
          },
        },
      },
    };
  }
};
