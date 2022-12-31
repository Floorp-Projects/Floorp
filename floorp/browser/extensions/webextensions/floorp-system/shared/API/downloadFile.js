/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* global ExtensionAPI, ExtensionCommon, Services, XPCOMUtils */

this.downloadFile = class extends ExtensionAPI {
  getAPI(context) {
    const EventManager = ExtensionCommon.EventManager;

    Cu.importGlobalProperties(["fetch", "IOUtils"]);

    return {
      downloadFile: {
        async download(url, path) {
          let data = await new Promise((resolve, reject) => {
            fetch(url)
              .then(res => {
                if (res.status !== 200) throw `${res.status} ${res.statusText}`;
                return res.arrayBuffer();
              })
              .then(resolve)
              .catch(reject);
          });
          await IOUtils.write(path, new Uint8Array(data));
          return;
        },
      },
    };
  }
};
