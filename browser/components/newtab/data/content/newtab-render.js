/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// exported by activity-stream.bundle.js
if (window.__FROM_STARTUP_CACHE__) {
  window.NewtabRenderUtils.renderCache(window.__STARTUP_STATE__);
} else {
  window.NewtabRenderUtils.renderWithoutState();
}
