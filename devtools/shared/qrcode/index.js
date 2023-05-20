/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Lazily require encoder and decoder in case only one is needed
Object.defineProperty(this, "Encoder", {
  get: () =>
    require("resource://devtools/shared/qrcode/encoder/index.js").Encoder,
});
Object.defineProperty(this, "QRRSBlock", {
  get: () =>
    require("resource://devtools/shared/qrcode/encoder/index.js").QRRSBlock,
});
Object.defineProperty(this, "QRErrorCorrectLevel", {
  get: () =>
    require("resource://devtools/shared/qrcode/encoder/index.js")
      .QRErrorCorrectLevel,
});
Object.defineProperty(this, "decoder", {
  get: () => {
    // Some applications don't ship the decoder, see moz.build
    try {
      return require("resource://devtools/shared/qrcode/decoder/index.js");
    } catch (e) {
      return null;
    }
  },
});

/**
 * There are many "versions" of QR codes, which describes how many dots appear
 * in the resulting image, thus limiting the amount of data that can be
 * represented.
 *
 * The encoder used here allows for versions 1 - 10 (more dots for larger
 * versions).
 *
 * It expects you to pick a version large enough to contain your message.  Here
 * we search for the mimimum version based on the message length.
 * @param string message
 *        Text to encode
 * @param string quality
 *        Quality level: L, M, Q, H
 * @return integer
 */
exports.findMinimumVersion = function (message, quality) {
  const msgLength = message.length;
  const qualityLevel = QRErrorCorrectLevel[quality];
  for (let version = 1; version <= 10; version++) {
    const rsBlocks = QRRSBlock.getRSBlocks(version, qualityLevel);
    let maxLength = rsBlocks.reduce((prev, block) => {
      return prev + block.dataCount;
    }, 0);
    // Remove two bytes to fit header info
    maxLength -= 2;
    if (msgLength <= maxLength) {
      return version;
    }
  }
  throw new Error("Message too large");
};

/**
 * Simple wrapper around the underlying encoder's API.
 * @param string  message
 *        Text to encode
 * @param string  quality (optional)
          Quality level: L, M, Q, H
 * @param integer version (optional)
 *        QR code "version" large enough to contain the message
 * @return object with the following fields:
 *   * src:    an image encoded a data URI
 *   * height: image height
 *   * width:  image width
 */
exports.encodeToDataURI = function (message, quality, version) {
  quality = quality || "H";
  version = version || exports.findMinimumVersion(message, quality);
  const encoder = new Encoder(version, quality);
  encoder.addData(message);
  encoder.make();
  return encoder.createImgData();
};

/**
 * Simple wrapper around the underlying decoder's API.
 * @param string URI
 *        URI of an image of a QR code
 * @return Promise
 *         The promise will be resolved with a string, which is the data inside
 *         the QR code.
 */
exports.decodeFromURI = function (URI) {
  if (!decoder) {
    return Promise.reject();
  }
  return new Promise((resolve, reject) => {
    decoder.decodeFromURI(URI, resolve, reject);
  });
};

/**
 * Decode a QR code that has been drawn to a canvas element.
 * @param Canvas canvas
 *        <canvas> element to read from
 * @return string
 *         The data inside the QR code
 */
exports.decodeFromCanvas = function (canvas) {
  if (!decoder) {
    throw new Error("Decoder not available");
  }
  return decoder.decodeFromCanvas(canvas);
};
