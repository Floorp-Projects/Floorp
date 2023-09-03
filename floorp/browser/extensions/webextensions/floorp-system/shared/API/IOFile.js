/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* global ExtensionAPI, ExtensionCommon, Services, XPCOMUtils */

this.IOFile = class extends ExtensionAPI {
  getAPI(context) {
    const EventManager = ExtensionCommon.EventManager;

    Cu.importGlobalProperties(["IOUtils"]);

    return {
      IOFile: {
        async createEmptyFile(path) {
          await IOUtils.writeUTF8(path, "");
        },
        async createDir(path) {
          return IOUtils.makeDirectory(path);
        },
        async exists(path) {
          return IOUtils.exists(path);
        },
        async copyFile(srcPath, destPath) {
          return IOUtils.copy(srcPath, destPath);
        },
        async copyDir(srcPath, destPath) {
          return IOUtils.copy(srcPath, destPath, { recursive: true });
        },
        async move(srcPath, destPath) {
          return IOUtils.move(srcPath, destPath);
        },
        async removeFile(path) {
          return IOUtils.remove(path);
        },
        async removeDir(path) {
          return IOUtils.remove(path, { recursive: true });
        },
      },
    };
  }
};
