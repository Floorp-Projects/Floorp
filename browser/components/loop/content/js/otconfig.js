/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

window.OTProperties = {
  cdnURL: "loop/"
};
window.OTProperties.assetURL = window.OTProperties.cdnURL + "sdk-content/";
window.OTProperties.configURL = window.OTProperties.assetURL + "js/dynamic_config.min.js";

// We don't use the SDK's CSS. This will prevent spurious 404 errors.
window.OTProperties.cssURL = "about:blank";
