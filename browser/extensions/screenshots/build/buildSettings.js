/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

window.buildSettings = {
  defaultSentryDsn: "https://904ccdd4866247c092ae8fc1a4764a63:940d44bdc71d4daea133c19080ccd38d@sentry.prod.mozaws.net/224",
  logLevel: "" || "warn",
  captureText: ("" === "true"),
  uploadBinary: ("" === "true"),
  pngToJpegCutoff: parseInt("" || 2500000, 10),
  maxImageHeight: parseInt("" || 10000, 10),
  maxImageWidth: parseInt("" || 10000, 10)
};
null;

