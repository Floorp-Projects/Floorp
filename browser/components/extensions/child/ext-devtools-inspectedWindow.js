/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.devtools_inspectedWindow = class extends ExtensionAPI {
  getAPI(context) {
    // `devtoolsToolboxInfo` is received from the child process when the root devtools view
    // has been created, and every sub-frame of that top level devtools frame will
    // receive the same information when the context has been created from the
    // `ExtensionChild.createExtensionContext` method.
    let tabId =
      context.devtoolsToolboxInfo &&
      context.devtoolsToolboxInfo.inspectedWindowTabId;

    return {
      devtools: {
        inspectedWindow: {
          get tabId() {
            return tabId;
          },
        },
      },
    };
  }
};
