/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-disable mozilla/reject-some-requires */

"use strict";

loader.lazyRequireGetter(this, "HarExporter", "devtools/client/netmonitor/src/har/har-exporter", true);

/**
 * Helper object with HAR related context menu actions.
 */
var HarMenuUtils = {
  /**
   * Copy HAR from the network panel content to the clipboard.
   */
  copyAllAsHar(requests, connector) {
    return HarExporter.copy(this.getDefaultHarOptions(requests, connector));
  },

  /**
   * Save HAR from the network panel content to a file.
   */
  saveAllAsHar(requests, connector) {
    // This will not work in launchpad
    // document.execCommand(‘cut’/‘copy’) was denied because it was not called from
    // inside a short running user-generated event handler.
    // https://developer.mozilla.org/en-US/Add-ons/WebExtensions/Interact_with_the_clipboard
    return HarExporter.save(this.getDefaultHarOptions(requests, connector));
  },

  getDefaultHarOptions(requests, connector) {
    return {
      connector: connector,
      items: requests,
    };
  },
};

// Exports from this module
exports.HarMenuUtils = HarMenuUtils;
