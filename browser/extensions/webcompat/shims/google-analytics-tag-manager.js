/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// based on https://github.com/gorhill/uBlock/blob/caa8e7d35ba61214a9d13e7d324b2bd2aa73237f/src/web_accessible_resources/googletagmanager_gtm.js

"use strict";

if (!window.ga) {
  window.ga = () => {};

  try {
    window.dataLayer.hide.end();
  } catch (_) {}

  const dl = window.dataLayer;
  if (typeof dl.push === "function") {
    dl.push = o => {
      if (o instanceof Object && typeof o.eventCallback === "function") {
        setTimeout(o.eventCallback, 1);
      }
    };
  }
}
