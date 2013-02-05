/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.metadata = {
  "stability": "unstable"
};

const { URL } = require("../url");

exports.MatchPattern = MatchPattern;

function MatchPattern(pattern) {
  if (typeof pattern.test == "function") {

    // For compatibility with -moz-document rules, we require the RegExp's
    // global, ignoreCase, and multiline flags to be set to false.
    if (pattern.global) {
      throw new Error("A RegExp match pattern cannot be set to `global` " +
                      "(i.e. //g).");
    }
    if (pattern.ignoreCase) {
      throw new Error("A RegExp match pattern cannot be set to `ignoreCase` " +
                      "(i.e. //i).");
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
    //
    if (this.regexp && this.regexp.test(urlStr) &&
        this.regexp.exec(urlStr)[0] == urlStr)
      return true;

    if (this.anyWebPage && /^(https?|ftp)$/.test(url.scheme))
      return true;
    if (this.exactURL && this.exactURL == urlStr)
      return true;
    if (this.domain && url.host &&
        url.host.slice(-this.domain.length) == this.domain)
      return true;
    if (this.urlPrefix && 0 == urlStr.indexOf(this.urlPrefix))
      return true;

    return false;
  }

};
