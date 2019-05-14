/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

ChromeUtils.defineModuleGetter(this, "ExtensionSearchHandler",
                               "resource://gre/modules/ExtensionSearchHandler.jsm");

this.omnibox = class extends ExtensionAPI {
  onManifestEntry(entryName) {
    let {extension} = this;
    let {manifest} = extension;

    let keyword = manifest.omnibox.keyword;
    try {
      // This will throw if the keyword is already registered.
      ExtensionSearchHandler.registerKeyword(keyword, extension);
      this.keyword = keyword;
    } catch (e) {
      extension.manifestError(e.message);
    }
  }

  onShutdown() {
    ExtensionSearchHandler.unregisterKeyword(this.keyword);
  }

  getAPI(context) {
    let {extension} = context;
    return {
      omnibox: {
        setDefaultSuggestion: (suggestion) => {
          try {
            // This will throw if the keyword failed to register.
            ExtensionSearchHandler.setDefaultSuggestion(this.keyword, suggestion);
          } catch (e) {
            return Promise.reject(e.message);
          }
        },

        onInputStarted: new EventManager({
          context,
          name: "omnibox.onInputStarted",
          register: fire => {
            let listener = (eventName) => {
              fire.sync();
            };
            extension.on(ExtensionSearchHandler.MSG_INPUT_STARTED, listener);
            return () => {
              extension.off(ExtensionSearchHandler.MSG_INPUT_STARTED, listener);
            };
          },
        }).api(),

        onInputCancelled: new EventManager({
          context,
          name: "omnibox.onInputCancelled",
          register: fire => {
            let listener = (eventName) => {
              fire.sync();
            };
            extension.on(ExtensionSearchHandler.MSG_INPUT_CANCELLED, listener);
            return () => {
              extension.off(ExtensionSearchHandler.MSG_INPUT_CANCELLED, listener);
            };
          },
        }).api(),

        onInputEntered: new EventManager({
          context,
          name: "omnibox.onInputEntered",
          register: fire => {
            let listener = (eventName, text, disposition) => {
              fire.sync(text, disposition);
            };
            extension.on(ExtensionSearchHandler.MSG_INPUT_ENTERED, listener);
            return () => {
              extension.off(ExtensionSearchHandler.MSG_INPUT_ENTERED, listener);
            };
          },
        }).api(),

        // Internal APIs.
        addSuggestions: (id, suggestions) => {
          try {
            ExtensionSearchHandler.addSuggestions(this.keyword, id, suggestions);
          } catch (e) {
            // Silently fail because the extension developer can not know for sure if the user
            // has already invalidated the callback when asynchronously providing suggestions.
          }
        },

        onInputChanged: new EventManager({
          context,
          name: "omnibox.onInputChanged",
          register: fire => {
            let listener = (eventName, text, id) => {
              fire.sync(text, id);
            };
            extension.on(ExtensionSearchHandler.MSG_INPUT_CHANGED, listener);
            return () => {
              extension.off(ExtensionSearchHandler.MSG_INPUT_CHANGED, listener);
            };
          },
        }).api(),
      },
    };
  }
};
