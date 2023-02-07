/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* global ExtensionAPI, ExtensionCommon, Services, XPCOMUtils */

this.IOFile = class extends ExtensionAPI {
  getAPI(context) {
    const EventManager = ExtensionCommon.EventManager;

    Cu.importGlobalProperties(["IOUtils"]);

    const { OS } = ChromeUtils.import(
      "resource://gre/modules/osfile.jsm"
    );

    return {
      IOFile: {
        async createEmptyFile(path) {
          await IOUtils.writeUTF8(path, "");
        },
        async createDir(path) {
          return OS.File.makeDir(path);
        },
        async exists(path) {
          return OS.File.exists(path);
        },
        async copyFile(srcPath, destPath) {
          return OS.File.copy(srcPath, destPath);
        },
        async copyDir(srcPath, destPath) {
          return IOUtils.copy(srcPath, destPath, { recursive: true });
        },
        async move(srcPath, destPath) {
          return OS.File.move(srcPath, destPath);
        },
        async removeFile(path) {
          return OS.File.remove(path);
        },
        async removeDir(path) {
          return OS.File.removeDir(path, { ignoreAbsent: true });
        },
      },
    };
  }
};
