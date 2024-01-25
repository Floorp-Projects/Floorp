/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Bug 1790750 - The page is blank on relearnings.com/linkedinlearning.html
 * with ETP set to Standard
 *
 * The linkedin video iframe loaded on relearnings.com/linkedinlearning.html is
 * denied storage access by ETP due to being a tracker breaking the website.
 * Since the iframe is a video which users want to access if visiting the URL
 * this intervention sets the window location to the iframe URL allowing it
 * first party storage access.
 */

/* globals exportFunction */

const LINKEDIN_LEARNING_PATH_PREFIX =
  "https://www.linkedin.com/learning/embed/";

document.addEventListener("DOMContentLoaded", function () {
  let iframes = document.getElementsByTagName("iframe");
  if (iframes.length === 1) {
    if (iframes[0].src.startsWith(LINKEDIN_LEARNING_PATH_PREFIX)) {
      window.location = iframes[0].src;
      console.info(
        "The window.location has been changed for compatibility reasons. See https://bugzilla.mozilla.org/show_bug.cgi?id=1790750 for details."
      );
    }
  }
});
