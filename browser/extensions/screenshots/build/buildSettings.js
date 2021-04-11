/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

window.buildSettings = {
  defaultSentryDsn: "",
  logLevel: "" || "warn",
  captureText: "" === "true",
  uploadBinary: "" === "true",
  pngToJpegCutoff: parseInt("" || 2500000, 10),
  maxImageHeight: parseInt("" || 10000, 10),
  maxImageWidth: parseInt("" || 10000, 10),
};
null;
