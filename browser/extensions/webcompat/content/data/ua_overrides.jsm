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
    }
  }
];

var EXPORTED_SYMBOLS = ["UAOverrides"]; /* exported UAOverrides */
