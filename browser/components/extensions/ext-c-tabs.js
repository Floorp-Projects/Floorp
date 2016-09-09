/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

extensions.registerSchemaAPI("tabs", "addon_child", context => {
  return {
    tabs: {
      connect: function(tabId, connectInfo) {
        let name = "";
        if (connectInfo && connectInfo.name !== null) {
          name = connectInfo.name;
        }
        let recipient = {
          extensionId: context.extension.id,
          tabId,
        };
        if (connectInfo && connectInfo.frameId !== null) {
          recipient.frameId = connectInfo.frameId;
        }
        return context.messenger.connect(context.messageManager, name, recipient);
      },

      sendMessage: function(tabId, message, options, responseCallback) {
        let recipient = {
          extensionId: context.extension.id,
          tabId: tabId,
        };
        if (options && options.frameId !== null) {
          recipient.frameId = options.frameId;
        }
        return context.messenger.sendMessage(context.messageManager, message, recipient, responseCallback);
      },
    },
  };
});
