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
            return front.eval(callerInfo, expression, options || {}).then(evalResult => {
              // TODO(rpl): check for additional undocumented behaviors on chrome
              // (e.g. if we should also print error to the console or set lastError?).
              return new SpreadArgs([evalResult.value, evalResult.exceptionInfo]);
            });
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
