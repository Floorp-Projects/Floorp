/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

Cu.import("resource://gre/modules/ExtensionUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "ExtensionSearchHandler",
                                  "resource://gre/modules/ExtensionSearchHandler.jsm");
var {
  SingletonEventManager,
} = ExtensionUtils;

// WeakMap[extension -> keyword]
let gKeywordMap = new WeakMap();

/* eslint-disable mozilla/balanced-listeners */
extensions.on("manifest_omnibox", (type, directive, extension, manifest) => {
  let keyword = manifest.omnibox.keyword;
  try {
    // This will throw if the keyword is already registered.
    ExtensionSearchHandler.registerKeyword(keyword, extension);
    gKeywordMap.set(extension, keyword);
  } catch (e) {
    extension.manifestError(e.message);
  }
});

extensions.on("shutdown", (type, extension) => {
  let keyword = gKeywordMap.get(extension);
  if (keyword) {
    ExtensionSearchHandler.unregisterKeyword(keyword);
    gKeywordMap.delete(extension);
  }
});
/* eslint-enable mozilla/balanced-listeners */

extensions.registerSchemaAPI("omnibox", "addon_parent", context => {
  let {extension} = context;
  return {
    omnibox: {
      setDefaultSuggestion(suggestion) {
        let keyword = gKeywordMap.get(extension);
        try {
          // This will throw if the keyword failed to register.
          ExtensionSearchHandler.setDefaultSuggestion(keyword, suggestion);
        } catch (e) {
          return Promise.reject(e.message);
        }
      },

      onInputStarted: new SingletonEventManager(context, "omnibox.onInputStarted", fire => {
        let listener = (eventName) => {
          fire();
        };
        extension.on(ExtensionSearchHandler.MSG_INPUT_STARTED, listener);
        return () => {
          extension.off(ExtensionSearchHandler.MSG_INPUT_STARTED, listener);
        };
      }).api(),

      onInputCancelled: new SingletonEventManager(context, "omnibox.onInputCancelled", fire => {
        let listener = (eventName) => {
          fire();
        };
        extension.on(ExtensionSearchHandler.MSG_INPUT_CANCELLED, listener);
        return () => {
          extension.off(ExtensionSearchHandler.MSG_INPUT_CANCELLED, listener);
        };
      }).api(),

      onInputEntered: new SingletonEventManager(context, "omnibox.onInputEntered", fire => {
        let listener = (eventName, text, disposition) => {
          fire(text, disposition);
        };
        extension.on(ExtensionSearchHandler.MSG_INPUT_ENTERED, listener);
        return () => {
          extension.off(ExtensionSearchHandler.MSG_INPUT_ENTERED, listener);
        };
      }).api(),
    },

    omnibox_internal: {
      addSuggestions(id, suggestions) {
        let keyword = gKeywordMap.get(extension);
        try {
          ExtensionSearchHandler.addSuggestions(keyword, id, suggestions);
        } catch (e) {
          // Silently fail because the extension developer can not know for sure if the user
          // has already invalidated the callback when asynchronously providing suggestions.
        }
      },

      onInputChanged: new SingletonEventManager(context, "omnibox_internal.onInputChanged", fire => {
        let listener = (eventName, text, id) => {
          fire(text, id);
        };
        extension.on(ExtensionSearchHandler.MSG_INPUT_CHANGED, listener);
        return () => {
          extension.off(ExtensionSearchHandler.MSG_INPUT_CHANGED, listener);
        };
      }).api(),
    },
  };
});
