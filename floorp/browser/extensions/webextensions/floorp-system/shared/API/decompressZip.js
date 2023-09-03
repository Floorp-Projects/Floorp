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

    const { FileUtils } = ChromeUtils.import(
      "resource://gre/modules/FileUtils.jsm"
    );

    const { PathUtils } = ChromeUtils.importESModule(
      "resource://gre/modules/PathUtils.sys.mjs"
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
            let entryPath = String(entry);
            if (PathUtils.normalize(entryPath).startsWith("..") ||
              PathUtils.normalize(entryPath).startsWith("/")) {
              throw new console.error( "!!! Zip Slip detected !!!");
            }
            let path = PathUtils.join(
              targetDirPath,
              AppConstants.platform === "win" ?
                entryPath.replaceAll("/", "\\") :
                entryPath
            );
            await zipreader.extract(
              entry,
              FileUtils.File(path)
            )
          }

          zipreader.close();
        },
      },
    };
  }
};
