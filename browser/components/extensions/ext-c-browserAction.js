/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

Cu.import("resource://gre/modules/ExtensionUtils.jsm");
var {
  IconDetails,
} = ExtensionUtils;

extensions.registerSchemaAPI("browserAction", "addon_child", context => {
  return {
    browserAction: {
      setIcon: function(details) {
        // This needs to run in the addon process because normalization requires
        // the use of <canvas>.
        let normalizedDetails = {
          tabId: details.tabId,
          path: IconDetails.normalize(details, context.extension, context),
        };
        return context.childManager.callParentAsyncFunction("browserAction.setIcon", [
          normalizedDetails,
        ]);
      },
    },
  };
});
