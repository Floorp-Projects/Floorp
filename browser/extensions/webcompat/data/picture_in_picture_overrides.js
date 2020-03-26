/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* globals browser */

let AVAILABLE_PIP_OVERRIDES;

{
  // See PictureInPictureTogglePolicy.jsm for these values.
  // eslint-disable-next-line no-unused-vars
  const TOGGLE_POLICIES = browser.pictureInPictureChild.getPolicies();

  AVAILABLE_PIP_OVERRIDES = {
    // The keys of this object are match patterns for URLs, as documented in
    // https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/Match_patterns
    //
    // Example:
    //
    // "https://*.youtube.com/*": TOGGLE_POLICIES.THREE_QUARTERS,
    // "https://*.twitch.tv/mikeconley_dot_ca/*": TOGGLE_POLICIES.TOP,

    // Instagram
    "https://www.instagram.com/*": TOGGLE_POLICIES.ONE_QUARTER,

    // Laracasts
    "https://*.laracasts.com/*": TOGGLE_POLICIES.ONE_QUARTER,

    // Twitch
    "https://*.twitch.tv/*": TOGGLE_POLICIES.ONE_QUARTER,
    "https://*.twitch.tech/*": TOGGLE_POLICIES.ONE_QUARTER,
    "https://*.twitch.a2z.com/*": TOGGLE_POLICIES.ONE_QUARTER,

    // Udemy
    "https://*.udemy.com/*": TOGGLE_POLICIES.ONE_QUARTER,
  };
}
