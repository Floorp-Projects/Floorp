/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

Cu.import("resource://gre/modules/ExtensionUtils.jsm");

var {
  runSafeSyncWithoutClone,
  SingletonEventManager,
} = ExtensionUtils;

extensions.registerSchemaAPI("omnibox", "addon_child", context => {
  return {
    omnibox: {
      onInputChanged: new SingletonEventManager(context, "omnibox.onInputChanged", fire => {
        let listener = (text, id) => {
          runSafeSyncWithoutClone(fire, text, suggestions => {
            // TODO: Switch to using callParentFunctionNoReturn once bug 1314903 is fixed.
            context.childManager.callParentAsyncFunction("omnibox_internal.addSuggestions", [
              id,
              suggestions,
            ]);
          });
        };
        context.childManager.getParentEvent("omnibox_internal.onInputChanged").addListener(listener);
        return () => {
          context.childManager.getParentEvent("omnibox_internal.onInputChanged").removeListener(listener);
        };
      }).api(),
    },
  };
});
