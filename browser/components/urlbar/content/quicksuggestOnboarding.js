/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

document.addEventListener("dialogextra1", () => {
  window.arguments[0].learnMore = true;
  window.close();
});
