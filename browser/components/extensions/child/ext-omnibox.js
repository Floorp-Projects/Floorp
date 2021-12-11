/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.omnibox = class extends ExtensionAPI {
  getAPI(context) {
    return {
      omnibox: {
        onInputChanged: new EventManager({
          context,
          name: "omnibox.onInputChanged",
          register: fire => {
            let listener = (text, id) => {
              fire.asyncWithoutClone(text, suggestions => {
                context.childManager.callParentFunctionNoReturn(
                  "omnibox.addSuggestions",
                  [id, suggestions]
                );
              });
            };
            context.childManager
              .getParentEvent("omnibox.onInputChanged")
              .addListener(listener);
            return () => {
              context.childManager
                .getParentEvent("omnibox.onInputChanged")
                .removeListener(listener);
            };
          },
        }).api(),
      },
    };
  }
};
