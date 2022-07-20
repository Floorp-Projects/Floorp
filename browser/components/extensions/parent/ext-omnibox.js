/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  ExtensionSearchHandler:
    "resource://gre/modules/ExtensionSearchHandler.sys.mjs",
});

this.omnibox = class extends ExtensionAPIPersistent {
  PERSISTENT_EVENTS = {
    onInputStarted({ fire }) {
      let { extension } = this;
      let listener = eventName => {
        fire.sync();
      };
      extension.on(ExtensionSearchHandler.MSG_INPUT_STARTED, listener);
      return {
        unregister() {
          extension.off(ExtensionSearchHandler.MSG_INPUT_STARTED, listener);
        },
        convert(_fire) {
          fire = _fire;
        },
      };
    },
    onInputCancelled({ fire }) {
      let { extension } = this;
      let listener = eventName => {
        fire.sync();
      };
      extension.on(ExtensionSearchHandler.MSG_INPUT_CANCELLED, listener);
      return {
        unregister() {
          extension.off(ExtensionSearchHandler.MSG_INPUT_CANCELLED, listener);
        },
        convert(_fire) {
          fire = _fire;
        },
      };
    },
    onInputEntered({ fire }) {
      let { extension } = this;
      let listener = (eventName, text, disposition) => {
        fire.sync(text, disposition);
      };
      extension.on(ExtensionSearchHandler.MSG_INPUT_ENTERED, listener);
      return {
        unregister() {
          extension.off(ExtensionSearchHandler.MSG_INPUT_ENTERED, listener);
        },
        convert(_fire) {
          fire = _fire;
        },
      };
    },
    onInputChanged({ fire }) {
      let { extension } = this;
      let listener = (eventName, text, id) => {
        fire.sync(text, id);
      };
      extension.on(ExtensionSearchHandler.MSG_INPUT_CHANGED, listener);
      return {
        unregister() {
          extension.off(ExtensionSearchHandler.MSG_INPUT_CHANGED, listener);
        },
        convert(_fire) {
          fire = _fire;
        },
      };
    },
  };

  onManifestEntry(entryName) {
    let { extension } = this;
    let { manifest } = extension;

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
    return {
      omnibox: {
        setDefaultSuggestion: suggestion => {
          try {
            // This will throw if the keyword failed to register.
            ExtensionSearchHandler.setDefaultSuggestion(
              this.keyword,
              suggestion
            );
          } catch (e) {
            return Promise.reject(e.message);
          }
        },

        onInputStarted: new EventManager({
          context,
          module: "omnibox",
          event: "onInputStarted",
          extensionApi: this,
        }).api(),

        onInputCancelled: new EventManager({
          context,
          module: "omnibox",
          event: "onInputCancelled",
          extensionApi: this,
        }).api(),

        onInputEntered: new EventManager({
          context,
          module: "omnibox",
          event: "onInputEntered",
          extensionApi: this,
        }).api(),

        onInputChanged: new EventManager({
          context,
          module: "omnibox",
          event: "onInputChanged",
          extensionApi: this,
        }).api(),

        // Internal APIs.
        addSuggestions: (id, suggestions) => {
          try {
            ExtensionSearchHandler.addSuggestions(
              this.keyword,
              id,
              suggestions
            );
          } catch (e) {
            // Silently fail because the extension developer can not know for sure if the user
            // has already invalidated the callback when asynchronously providing suggestions.
          }
        },
      },
    };
  }
};
