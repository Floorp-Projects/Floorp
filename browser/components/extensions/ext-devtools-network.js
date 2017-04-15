/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

this.devtools_network = class extends ExtensionAPI {
  getAPI(context) {
    return {
      devtools: {
        network: {
          onNavigated: new SingletonEventManager(context, "devtools.onNavigated", fire => {
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
        },
      },
    };
  }
};
