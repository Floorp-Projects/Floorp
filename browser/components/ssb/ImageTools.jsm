/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["ImageTools"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "NetUtil",
  "resource://gre/modules/NetUtil.jsm"
);
XPCOMUtils.defineLazyServiceGetter(
  this,
  "ImgTools",
  "@mozilla.org/image/tools;1",
  Ci.imgITools
);

const ImageTools = {
  /**
   * Given a data URI decodes the data into an object with "type" which is the
   * found mimetype and "container" which is an imgIContainer.
   *
   * @param {nsIURI} dataURI the URI to load.
   * @return {Promise<object>} the image info.
   */
  loadImage(dataURI) {
    return new Promise((resolve, reject) => {
      if (!dataURI.schemeIs("data")) {
        reject(new Error("Should only be loading data URIs."));
        return;
      }

      let channel = NetUtil.newChannel({
        uri: dataURI,
        loadUsingSystemPrincipal: true,
      });

      ImgTools.decodeImageFromChannelAsync(
        dataURI,
        channel,
        (container, status) => {
          if (Components.isSuccessCode(status)) {
            resolve({
              type: channel.contentType,
              container,
            });
          } else {
            reject(Components.Exception("Failed to load image.", status));
          }
        },
        null
      );
    });
  },
};
