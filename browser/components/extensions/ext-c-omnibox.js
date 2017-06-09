/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

// The ext-* files are imported into the same scopes.
/* import-globals-from ../../../toolkit/components/extensions/ext-c-toolkit.js */

this.omnibox = class extends ExtensionAPI {
  getAPI(context) {
    return {
      omnibox: {
        onInputChanged: new EventManager(context, "omnibox.onInputChanged", fire => {
          let listener = (text, id) => {
            fire.asyncWithoutClone(text, suggestions => {
              context.childManager.callParentFunctionNoReturn("omnibox_internal.addSuggestions", [
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
  }
};
