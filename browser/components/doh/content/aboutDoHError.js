/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

document.getElementById("retry").addEventListener("click", () => {
  document.dispatchEvent(new CustomEvent("DoHRetry", { bubbles: true }));
});

document.getElementById("allowFallback").addEventListener("click", () => {
  document.dispatchEvent(
    new CustomEvent("DoHAllowFallback", { bubbles: true })
  );
});

document.getElementById("learnMoreLink").addEventListener("click", () => {
  let block = document.getElementById("learn-more-block");
  block.hidden = !block.hidden;
});
