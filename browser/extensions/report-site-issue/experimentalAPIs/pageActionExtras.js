/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* global ExtensionAPI */

this.pageActionExtras = class extends ExtensionAPI {
  getAPI(context) {
    const extension = context.extension;
    const pageActionAPI = extension.apiManager.getAPI("pageAction", extension,
                                                      context.envType);
    const {Management: {global: {windowTracker}}} =
                ChromeUtils.import("resource://gre/modules/Extension.jsm", null);
    return {
      pageActionExtras: {
        async setDefaultTitle(title) {
          pageActionAPI.defaults.title = title;
          // Make sure the new default title is considered right away
          for (const window of windowTracker.browserWindows()) {
            const tab = window.gBrowser.selectedTab;
            if (pageActionAPI.isShown(tab)) {
              pageActionAPI.updateButton(window);
            }
          }
        },
        async setLabelForHistogram(label) {
          pageActionAPI.browserPageAction._labelForHistogram = label;
        },
        async setTooltipText(text) {
          pageActionAPI.browserPageAction.setTooltip(text);
        },
      },
    };
  }
};
