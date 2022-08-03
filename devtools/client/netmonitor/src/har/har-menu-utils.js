/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { L10N } = require("devtools/client/netmonitor/src/utils/l10n");

loader.lazyRequireGetter(
  this,
  "HarExporter",
  "devtools/client/netmonitor/src/har/har-exporter",
  true
);

loader.lazyGetter(this, "HarImporter", function() {
  return require("devtools/client/netmonitor/src/har/har-importer").HarImporter;
});

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

  /**
   * Import HAR file and preview its content in the Network panel.
   */
  openHarFile(actions, openSplitConsole) {
    const fp = Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);
    fp.init(
      window,
      L10N.getStr("netmonitor.har.importHarDialogTitle"),
      Ci.nsIFilePicker.modeOpen
    );

    // Append file filters
    fp.appendFilter(
      L10N.getStr("netmonitor.har.importDialogHARFilter"),
      "*.har"
    );
    fp.appendFilter(L10N.getStr("netmonitor.har.importDialogAllFilter"), "*.*");

    fp.open(rv => {
      if (rv == Ci.nsIFilePicker.returnOK) {
        const file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
        file.initWithPath(fp.file.path);
        readFile(file).then(har => {
          if (har) {
            this.appendPreview(har, actions, openSplitConsole);
          }
        });
      }
    });
  },

  appendPreview(har, actions, openSplitConsole) {
    try {
      const importer = new HarImporter(actions);
      importer.import(har);
    } catch (err) {
      if (openSplitConsole) {
        openSplitConsole("Error while processing HAR file: " + err.message);
      }
    }
  },

  getDefaultHarOptions(requests, connector) {
    return {
      connector,
      items: requests,
    };
  },
};

// Helpers

function readFile(file) {
  return new Promise(resolve => {
    IOUtils.read(file.path).then(data => {
      const decoder = new TextDecoder();
      resolve(decoder.decode(data));
    });
  });
}

// Exports from this module
exports.HarMenuUtils = HarMenuUtils;
