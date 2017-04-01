/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/ExtensionUtils.jsm");

const {
  SingletonEventManager,
} = ExtensionUtils;

extensions.registerSchemaAPI("devtools.network", "devtools_parent", (context) => {
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
});
