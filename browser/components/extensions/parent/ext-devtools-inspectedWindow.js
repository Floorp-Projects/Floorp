/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var { SpreadArgs } = ExtensionCommon;

this.devtools_inspectedWindow = class extends ExtensionAPI {
  getAPI(context) {
    // Lazily retrieved inspectedWindow actor front per child context.
    let waitForInspectedWindowFront;

    // TODO - Bug 1448878: retrieve a more detailed callerInfo object,
    // like the filename and lineNumber of the actual extension called
    // in the child process.
    const callerInfo = {
      addonId: context.extension.id,
      url: context.extension.baseURI.spec,
    };

    return {
      devtools: {
        inspectedWindow: {
          async eval(expression, options) {
            if (!waitForInspectedWindowFront) {
              waitForInspectedWindowFront = getInspectedWindowFront(context);
            }

            const front = await waitForInspectedWindowFront;

            const evalOptions = Object.assign(
              {},
              options,
              getToolboxEvalOptions(context)
            );

            const evalResult = await front.eval(
              callerInfo,
              expression,
              evalOptions
            );

            // TODO(rpl): check for additional undocumented behaviors on chrome
            // (e.g. if we should also print error to the console or set lastError?).
            return new SpreadArgs([evalResult.value, evalResult.exceptionInfo]);
          },
          async reload(options) {
            const { ignoreCache, userAgent, injectedScript } = options || {};

            if (!waitForInspectedWindowFront) {
              waitForInspectedWindowFront = getInspectedWindowFront(context);
            }

            const front = await waitForInspectedWindowFront;
            front.reload(callerInfo, {
              ignoreCache,
              userAgent,
              injectedScript,
            });
          },
        },
      },
    };
  }
};
