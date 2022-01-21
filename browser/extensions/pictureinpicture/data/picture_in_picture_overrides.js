/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* globals browser */

let AVAILABLE_PIP_OVERRIDES;

{
  // See PictureInPictureControls.jsm for these values.
  // eslint-disable-next-line no-unused-vars
  const TOGGLE_POLICIES = browser.pictureInPictureChild.getPolicies();
  const KEYBOARD_CONTROLS = browser.pictureInPictureChild.getKeyboardControls();

  AVAILABLE_PIP_OVERRIDES = {
    // The keys of this object are match patterns for URLs, as documented in
    // https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/Match_patterns
    //
    // Example:
    //
    // "https://*.youtube.com/*": {
    //   policy: TOGGLE_POLICIES.THREE_QUARTERS,
    //   keyboardControls: KEYBOARD_CONTROLS.PLAY_PAUSE | KEYBOARD_CONTROLS.VOLUME,
    // },
    // "https://*.twitch.tv/mikeconley_dot_ca/*": {
    //   policy: TOGGLE_POLICIES.TOP,
    //   keyboardControls: KEYBOARD_CONTROLS.NONE,
    // },

    instagram: {
      "https://www.instagram.com/*": { policy: TOGGLE_POLICIES.ONE_QUARTER },
    },

    laracasts: {
      "https://*.laracasts.com/*": { policy: TOGGLE_POLICIES.ONE_QUARTER },
    },

    netflix: {
      "https://*.netflix.com/*": { keyboardControls: ~KEYBOARD_CONTROLS.SEEK },
      "https://*.netflix.com/browse": { policy: TOGGLE_POLICIES.HIDDEN },
      "https://*.netflix.com/latest": { policy: TOGGLE_POLICIES.HIDDEN },
    },

    twitch: {
      "https://*.twitch.tv/*": { policy: TOGGLE_POLICIES.ONE_QUARTER },
      "https://*.twitch.tech/*": { policy: TOGGLE_POLICIES.ONE_QUARTER },
      "https://*.twitch.a2z.com/*": { policy: TOGGLE_POLICIES.ONE_QUARTER },
    },

    udemy: {
      "https://*.udemy.com/*": { policy: TOGGLE_POLICIES.ONE_QUARTER },
    },

    youtube: {
      "https://*.youtube.com/*": { visibilityThreshold: 0.9 },
    },
  };
}
