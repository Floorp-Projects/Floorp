/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

this.blobConverters = (function() {
  const exports = {};

  exports.dataUrlToBlob = function(url) {
    const binary = atob(url.split(",", 2)[1]);
    let contentType = exports.getTypeFromDataUrl(url);
    if (contentType !== "image/png" && contentType !== "image/jpeg") {
      contentType = "image/png";
    }
    const data = Uint8Array.from(binary, char => char.charCodeAt(0));
    const blob = new Blob([data], {type: contentType});
    return blob;
  };

  exports.getTypeFromDataUrl = function(url) {
    let contentType = url.split(",", 1)[0];
    contentType = contentType.split(";", 1)[0];
    contentType = contentType.split(":", 2)[1];
    return contentType;
  };

  exports.blobToArray = function(blob) {
    return new Promise((resolve, reject) => {
      const reader = new FileReader();
      reader.addEventListener("loadend", function() {
        resolve(reader.result);
      });
      reader.readAsArrayBuffer(blob);
    });
  };

  exports.blobToDataUrl = function(blob) {
    return new Promise((resolve, reject) => {
      const reader = new FileReader();
      reader.addEventListener("loadend", function() {
        resolve(reader.result);
      });
      reader.readAsDataURL(blob);
    });
  };

  return exports;
})();
null;
