/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * For detailed information on our policies, and a documention on this format
 * and its possibilites, please check the Mozilla-Wiki at
 *
 * https://wiki.mozilla.org/Compatibility/Go_Faster_Addon/Override_Policies_and_Workflows#User_Agent_overrides
 */
const UAOverrides = [

  /*
   * This is a dummy override that applies a Chrome UA to a dummy site that
   * blocks all browsers but Chrome.
   *
   * This was only put in place to allow QA to test this system addon on an
   * actual site, since we were not able to find a proper override in time.
   */
  {
    baseDomain: "schub.io",
    applications: ["firefox", "fennec"],
    uriMatcher: (uri) => uri.includes("webcompat-addon-testcases.schub.io"),
    uaTransformer: (originalUA) => {
      let prefix = originalUA.substr(0, originalUA.indexOf(")") + 1);
      return `${prefix} AppleWebKit/537.36 (KHTML, like Gecko) Chrome/54.0.2840.98 Safari/537.36`;
    },
  },

  /*
   * Bug 1480710 - m.imgur.com - Build UA override
   * WebCompat issue #13154 - https://webcompat.com/issues/13154
   *
   * imgur returns a 404 for requests to CSS and JS file if requested with a Fennec
   * User Agent. By removing the Fennec identifies and adding Chrome Mobile's, we
   * receive the correct CSS and JS files.
   */
  {
    baseDomain: "imgur.com",
    applications: ["fennec"],
    uriMatcher: (uri) => uri.includes("m.imgur.com"),
    uaTransformer: (originalUA) => {
      let prefix = originalUA.substr(0, originalUA.indexOf(")") + 1);
      return prefix + " AppleWebKit/537.36 (KHTML, like Gecko) Chrome/68.0.3440.85 Mobile Safari/537.36";
    },
  },

  /*
   * Bug 755590 - sites.google.com - top bar doesn't show up in Firefox for Android
   *
   * Google Sites does show a different top bar template based on the User Agent.
   * For Fennec, this results in a broken top bar. Appending Chrome and Mobile Safari
   * identifiers to the UA results in a correct rendering.
   */
  {
    baseDomain: "google.com",
    applications: ["fennec"],
    uriMatcher: (uri) => uri.includes("sites.google.com"),
    uaTransformer: (originalUA) => {
      return originalUA + " Chrome/68.0.3440.85 Mobile Safari/537.366";
    },
  },
];

var EXPORTED_SYMBOLS = ["UAOverrides"]; /* exported UAOverrides */
