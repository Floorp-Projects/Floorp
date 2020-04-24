/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var { SpreadArgs } = ExtensionCommon;

var { ExtensionError } = ExtensionUtils;

this.devtools_network = class extends ExtensionAPI {
  getAPI(context) {
    return {
      devtools: {
        network: {
          onNavigated: new EventManager({
            context,
            name: "devtools.onNavigated",
            register: fire => {
              let listener = data => {
                fire.async(data.url);
              };

              let targetPromise = context.getCurrentDevToolsTarget();
              targetPromise.then(target => {
                target.on("navigate", listener);
              });
              return () => {
                targetPromise.then(target => {
                  target.off("navigate", listener);
                });
              };
            },
          }).api(),

          getHAR: function() {
            return context.devToolsToolbox.getHARFromNetMonitor();
          },

          onRequestFinished: new EventManager({
            context,
            name: "devtools.network.onRequestFinished",
            register: fire => {
              const listener = data => {
                fire.async(data);
              };

              const toolbox = context.devToolsToolbox;
              toolbox.addRequestFinishedListener(listener);

              return () => {
                toolbox.removeRequestFinishedListener(listener);
              };
            },
          }).api(),

          // The following method is used internally to allow the request API
          // piece that is running in the child process to ask the parent process
          // to fetch response content from the back-end.
          Request: {
            async getContent(requestId) {
              return context.devToolsToolbox
                .fetchResponseContent(requestId)
                .then(
                  ({ content }) =>
                    new SpreadArgs([content.text, content.mimeType])
                )
                .catch(err => {
                  const debugName = context.extension.policy.debugName;
                  const errorMsg =
                    "Unexpected error while fetching response content";
                  Cu.reportError(
                    `${debugName}: ${errorMsg} for ${requestId}: ${err}`
                  );
                  throw new ExtensionError(errorMsg);
                });
            },
          },
        },
      },
    };
  }
};
