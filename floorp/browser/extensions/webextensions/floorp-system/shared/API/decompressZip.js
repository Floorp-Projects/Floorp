/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* global ExtensionAPI, ExtensionCommon, Services, XPCOMUtils */

this.decompressZip = class extends ExtensionAPI {
  getAPI(context) {
    const EventManager = ExtensionCommon.EventManager;

    const { AppConstants } = ChromeUtils.import(
      "resource://gre/modules/AppConstants.jsm"
    );
    
    const ZipReader = Components.Constructor(
      "@mozilla.org/libjar/zip-reader;1",
      "nsIZipReader",
      "open"
    );
    
    const { OS } = ChromeUtils.import(
      "resource://gre/modules/osfile.jsm"
    );
    
    const { FileUtils } = ChromeUtils.import(
      "resource://gre/modules/FileUtils.jsm"
    );

    return {
      decompressZip: {
        async decompress(zipPath, targetDirPath) {
          let zipreader = new ZipReader(
            FileUtils.File(zipPath)
          );

          let entries = [];
          for (let entry of zipreader.findEntries("*")) {
            entries.push(entry);
          }
          entries.sort((entry1, entry2) => {
            return String(entry1).length - String(entry2).length
          });
          for (let entry of entries) {
            let path = OS.Path.join(
              targetDirPath,
              AppConstants.platform === "win" ?
                String(entry).replaceAll("/", "\\") :
                String(entry)
            );
            await zipreader.extract(
              entry,
              FileUtils.File(path)
            )
          }

          zipreader.close();
          return;
        },
      },
    };
  }
};
