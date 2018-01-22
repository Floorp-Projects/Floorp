/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

// The ext-* files are imported into the same scopes.
/* import-globals-from ext-devtools.js */

this.devtools_network = class extends ExtensionAPI {
  getAPI(context) {
    return {
      devtools: {
        network: {
          onNavigated: new EventManager(context, "devtools.onNavigated", fire => {
            let listener = (event, data) => {
              fire.async(data.url);
            };

            let targetPromise = getDevToolsTargetForContext(context);
            targetPromise.then(target => {
              target.on("navigate", listener);
            });
            return () => {
              targetPromise.then(target => {
                target.off("navigate", listener);
              });
            };
          }).api(),

          getHAR: function() {
            return context.devToolsToolbox.getHARFromNetMonitor();
          },
        },
      },
    };
  }
};
