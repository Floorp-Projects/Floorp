/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

module.metadata = {
  "stability": "unstable"
};

const { URL } = require('../url');
const cache = {};

function MatchPattern(pattern) {
  if (cache[pattern]) return cache[pattern];

  if (typeof pattern.test == "function") {
    // For compatibility with -moz-document rules, we require the RegExp's
    // global, ignoreCase, and multiline flags to be set to false.
    if (pattern.global) {
      throw new Error("A RegExp match pattern cannot be set to `global` " +
                      "(i.e. //g).");
    }
    if (pattern.multiline) {
      throw new Error("A RegExp match pattern cannot be set to `multiline` " +
                      "(i.e. //m).");
    }

    this.regexp = pattern;
  }
  else {
    let firstWildcardPosition = pattern.indexOf("*");
    let lastWildcardPosition = pattern.lastIndexOf("*");
    if (firstWildcardPosition != lastWildcardPosition)
      throw new Error("There can be at most one '*' character in a wildcard.");

    if (firstWildcardPosition == 0) {
      if (pattern.length == 1)
        this.anyWebPage = true;
      else if (pattern[1] != ".")
        throw new Error("Expected a *.<domain name> string, got: " + pattern);
      else
        this.domain = pattern.substr(2);
    }
    else {
      if (pattern.indexOf(":") == -1) {
        throw new Error("When not using *.example.org wildcard, the string " +
                        "supplied is expected to be either an exact URL to " +
                        "match or a URL prefix. The provided string ('" +
                        pattern + "') is unlikely to match any pages.");
      }

      if (firstWildcardPosition == -1)
        this.exactURL = pattern;
      else if (firstWildcardPosition == pattern.length - 1)
        this.urlPrefix = pattern.substr(0, pattern.length - 1);
      else {
        throw new Error("The provided wildcard ('" + pattern + "') has a '*' " +
                        "in an unexpected position. It is expected to be the " +
                        "first or the last character in the wildcard.");
      }
    }
  }

  cache[pattern] = this;
}

MatchPattern.prototype = {
  test: function MatchPattern_test(urlStr) {
    try {
      var url = URL(urlStr);
    }
    catch (err) {
      return false;
    }

    // Test the URL against a RegExp pattern.  For compatibility with
    // -moz-document rules, we require the RegExp to match the entire URL,
    // so we not only test for a match, we also make sure the matched string
    // is the entire URL string.
    //
    // Assuming most URLs don't match most match patterns, we call `test` for
    // speed when determining whether or not the URL matches, then call `exec`
    // for the small subset that match to make sure the entire URL matches.
    if (this.regexp && this.regexp.test(urlStr) &&
        this.regexp.exec(urlStr)[0] == urlStr)
      return true;

    if (this.anyWebPage && /^(https?|ftp)$/.test(url.scheme))
      return true;

    if (this.exactURL && this.exactURL == urlStr)
      return true;

    // Tests the urlStr against domain and check if
    // wildcard submitted (*.domain.com), it only allows
    // subdomains (sub.domain.com) or from the root (http://domain.com)
    // and reject non-matching domains (otherdomain.com)
    // bug 856913
    if (this.domain && url.host &&
         (url.host === this.domain ||
          url.host.slice(-this.domain.length - 1) === "." + this.domain))
      return true;

    if (this.urlPrefix && 0 == urlStr.indexOf(this.urlPrefix))
      return true;

    return false;
  },

  toString: function () '[object MatchPattern]'
};

exports.MatchPattern = MatchPattern;
