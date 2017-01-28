/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This is an array of objects that specify user agent overrides. Each object
 * can have three attributes:
 *
 *   * `baseDomain`, required: The base domain that further checks and user
 *     agents override are applied to. This does not include subdomains.
 *   * `uriMatcher`: Function that gets the requested URI passed in the first
 *     argument and needs to return boolean whether or not the override should
 *     be applied. If not provided, the user agent override will be applied
 *     every time.
 *   * `uaTransformer`, required: Function that gets the original Firefox user
 *     agent passed as its first argument and needs to return a string that
 *     will be used as the the user agent for this URI.
 *
 * Examples:
 *
 * Gets applied for all requests to mozilla.org and subdomains:
 *
 * ```
 *   {
 *     baseDomain: "mozilla.org",
 *     uaTransformer: (originalUA) => `Ohai Mozilla, it's me, ${originalUA}`
 *   }
 * ```
 *
 * Applies to *.example.com/app/*:
 *
 * ```
 *   {
 *     baseDomain: "example.com",
 *     uriMatcher: (uri) => uri.includes("/app/"),
 *     uaTransformer: (originalUA) => originalUA.replace("Firefox", "Otherfox")
 *   }
 * ```
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
    uriMatcher: (uri) => uri.includes("webcompat-ua-dummy.schub.io"),
    uaTransformer: (originalUA) => {
      let prefix = originalUA.substr(0, originalUA.indexOf(")") + 1);
      return `${prefix} AppleWebKit/537.36 (KHTML, like Gecko) Chrome/54.0.2840.98 Safari/537.36`;
    }
  }
];

this.EXPORTED_SYMBOLS = ["UAOverrides"]; /* exported UAOverrides */
