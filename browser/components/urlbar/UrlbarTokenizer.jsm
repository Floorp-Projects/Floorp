/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * This module exports a tokenizer to be used by the urlbar model.
 * Emitted tokens are objects in the shape { type, value }, where type is one
 * of UrlbarTokenizer.TYPE.
 */

var EXPORTED_SYMBOLS = ["UrlbarTokenizer"];

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");

ChromeUtils.defineModuleGetter(this, "Log",
                               "resource://gre/modules/Log.jsm");
XPCOMUtils.defineLazyGetter(this, "logger", () =>
  Log.repository.getLogger("Places.Urlbar.Tokenizer"));

var UrlbarTokenizer = {
  // Regex matching on whitespaces.
  REGEXP_SPACES: /\s+/,

  // Regex used to guess url-like strings.
  // These are not expected to cover 100% of the cases.
  REGEXP_PROTOCOL: /^[A-Z+.-]+:(\/\/)?(?!\/)/i,
  REGEXP_USERINFO_INVALID_CHARS: /[^\w.~%!$&'()*+,;=:-]/,
  REGEXP_HOSTPORT_INVALID_CHARS: /[^\[\]A-Z0-9.:-]/i,
  REGEXP_HOSTPORT_IP_LIKE: /^[a-f0-9\.\[\]:]+$/i,
  REGEXP_HOSTPORT_INVALID_IP: /\.{2,}|\d{5,}|\d{4,}(?![:\]])|^\.|\.$|^(\d+\.){4,}\d+$|^\d+$/,
  REGEXP_HOSTPORT_IPV4: /^(\d{1,3}\.){3,}\d{1,3}(:\d+)?$/,
  REGEXP_HOSTPORT_IPV6: /^[0-9A-F:\[\]]{1,4}$/i,
  REGEXP_COMMON_EMAIL: /^[\w!#$%&'*+\/=?^`{|}~-]+@[\[\]A-Z0-9.-]+$/i,

  TYPE: {
    TEXT: 1,
    POSSIBLE_ORIGIN: 2, // It may be an ip, a domain, but even just a single word used as host.
    POSSIBLE_URL: 3, // Consumers should still check this with a fixup.
    RESTRICT_HISTORY: 4,
    RESTRICT_BOOKMARK: 5,
    RESTRICT_TAG: 6,
    RESTRICT_OPENPAGE: 7,
    RESTRICT_TYPED: 8,
    RESTRICT_SEARCH: 9,
    RESTRICT_TITLE: 10,
    RESTRICT_URL: 11,
  },

  /**
   * Returns whether the passed in token looks like a URL.
   * This is based on guessing and heuristics, that means if this function
   * returns false, it's surely not a URL, if it returns true, the result must
   * still be verified through URIFixup.
   *
   * @param {string} token
   *        The string token to verify
   * @param {object} options {
   *          requirePath: the url must have a path
   *        }
   * @returns {boolean} whether the token looks like a URL.
   */
  looksLikeUrl(token, options = {}) {
    if (token.length < 2)
      return false;
    // It should be a single word.
    if (this.REGEXP_SPACES.test(token))
      return false;
    // If it starts with something that looks like a protocol, it's likely a url.
    if (this.REGEXP_PROTOCOL.test(token))
      return true;
    // Guess path and prePath. At this point we should be analyzing strings not
    // having a protocol.
    let slashIndex = token.indexOf("/");
    let prePath = slashIndex != -1 ? token.slice(0, slashIndex) : token;
    if (!this.looksLikeOrigin(prePath))
      return false;

    let path = slashIndex != -1 ? token.slice(slashIndex) : "";
    logger.debug("path", path);
    if (options.requirePath && !path)
      return false;
    // If there are both path and userinfo, it's likely a url.
    let atIndex = prePath.indexOf("@");
    let userinfo = atIndex != -1 ? prePath.slice(0, atIndex) : "";
    if (path.length && userinfo.length)
      return true;

    // If the path contains special chars, it is likely a url.
    if (["%", "?", "#"].some(c => path.includes(c)))
      return true;

    // The above looksLikeOrigin call told us the prePath looks like an origin,
    // now we go into details checking some common origins.
    let hostPort = atIndex != -1 ? prePath.slice(atIndex + 1) : prePath;
    if (this.REGEXP_HOSTPORT_IPV4.test(hostPort))
      return true;
    // ipv6 is very complex to support, just check for a few chars.
    if (this.REGEXP_HOSTPORT_IPV6.test(hostPort) &&
        ["[", "]", ":"].some(c => hostPort.includes(c)))
      return true;
    if (Services.uriFixup.isDomainWhitelisted(hostPort, -1))
      return true;
    return false;
  },

  /**
   * Returns whether the passed in token looks like an origin.
   * This is based on guessing and heuristics, that means if this function
   * returns false, it's surely not an origin, if it returns true, the result
   * must still be verified through URIFixup.
   *
   * @param {string} token
   *        The string token to verify
   * @returns {boolean} whether the token looks like an origin.
   */
  looksLikeOrigin(token) {
    let atIndex = token.indexOf("@");
    if (atIndex != -1 && this.REGEXP_COMMON_EMAIL.test(token)) {
      // We prefer handling it as an email rather than an origin with userinfo.
      return false;
    }
    let userinfo = atIndex != -1 ? token.slice(0, atIndex) : "";
    let hostPort = atIndex != -1 ? token.slice(atIndex + 1) : token;
    logger.debug("userinfo", userinfo);
    logger.debug("hostPort", hostPort);
    if (this.REGEXP_HOSTPORT_IPV4.test(hostPort))
      return true;
    if (this.REGEXP_HOSTPORT_IPV6.test(hostPort))
      return true;
    // Check for invalid chars.
    return !this.REGEXP_USERINFO_INVALID_CHARS.test(userinfo) &&
           !this.REGEXP_HOSTPORT_INVALID_CHARS.test(hostPort) &&
           (!this.REGEXP_HOSTPORT_IP_LIKE.test(hostPort) ||
            !this.REGEXP_HOSTPORT_INVALID_IP.test(hostPort));
  },

  /**
   * Tokenizes the searchString from a QueryContext.
   * @param {object} queryContext
   *        The QueryContext object to tokenize
   * @returns {object} the same QueryContext object with a new tokens property.
   */
  tokenize(queryContext) {
    logger.info("Tokenizing", queryContext);
    let searchString = queryContext.searchString;
    if (searchString.length == 0) {
      queryContext.tokens = [];
      return queryContext;
    }

    let unfiltered = splitString(searchString);
    let tokens = filterTokens(unfiltered);
    queryContext.tokens = tokens;
    return queryContext;
  },

  /**
   * Given a token, tells if it's a restriction token.
   * @param {string} token
   * @returns {boolean} Whether the token is a restriction character.
   */
  isRestrictionToken(token) {
    return token.type >= this.TYPE.RESTRICT_HISTORY &&
           token.type <= this.TYPE.RESTRICT_URL;
  },
};

// The special characters below can be typed into the urlbar to restrict
// the search to a certain category, like history, bookmarks or open pages; or
// to force a match on just the title or url.
// These restriction characters can be typed alone, or at word boundaries,
// provided their meaning cannot be confused, for example # could be present
// in a valid url, and thus it should not be interpreted as a restriction.
UrlbarTokenizer.CHAR_TO_TYPE_MAP = new Map([
  ["^", UrlbarTokenizer.TYPE.RESTRICT_HISTORY],
  ["*", UrlbarTokenizer.TYPE.RESTRICT_BOOKMARK],
  ["+", UrlbarTokenizer.TYPE.RESTRICT_TAG],
  ["%", UrlbarTokenizer.TYPE.RESTRICT_OPENPAGE],
  ["~", UrlbarTokenizer.TYPE.RESTRICT_TYPED],
  ["$", UrlbarTokenizer.TYPE.RESTRICT_SEARCH],
  ["#", UrlbarTokenizer.TYPE.RESTRICT_TITLE],
  ["@", UrlbarTokenizer.TYPE.RESTRICT_URL],
]);

/**
 * Given a search string, splits it into string tokens.
 * @param {string} searchString
 *        The search string to split
 * @returns {array} An array of string tokens.
 */
function splitString(searchString) {
  // The first step is splitting on unicode whitespaces.
  let tokens = searchString.trim().split(UrlbarTokenizer.REGEXP_SPACES);
  let accumulator = [];
  let hasRestrictionToken = tokens.some(t => UrlbarTokenizer.CHAR_TO_TYPE_MAP.has(t));
  let chars = Array.from(UrlbarTokenizer.CHAR_TO_TYPE_MAP.keys()).join("");
  logger.debug("Restriction chars", chars);
  for (let token of tokens) {
    // It's possible we have to split a token, if there's no separate restriction
    // character and a token starts or ends with a restriction character, and it's
    // not confusable (for example # at the end of an url.
    // If the token looks like a url, certain characters may appear at the end
    // of the path or the query string, thus ignore those.
    if (!hasRestrictionToken &&
        token.length > 1 &&
        !UrlbarTokenizer.looksLikeUrl(token, {requirePath: true})) {
      // Check for a restriction char at the beginning.
      if (chars.includes(token[0])) {
        hasRestrictionToken = true;
        accumulator.push(token[0]);
        accumulator.push(token.slice(1));
        continue;
      } else if (chars.includes(token[token.length - 1])) {
        hasRestrictionToken = true;
        accumulator.push(token.slice(0, token.length - 1));
        accumulator.push(token[token.length - 1]);
        continue;
      }
    }
    accumulator.push(token);
  }
  logger.info("Found tokens", accumulator);
  return accumulator;
}

/**
 * Given an array of unfiltered tokens, this function filters them and converts
 * to token objects with a type.
 *
 * @param {array} tokens
 *        An array of strings, representing search tokens.
 * @returns {array} An array of token objects.
 */
function filterTokens(tokens) {
  let filtered = [];
  let foundRestriction = [];
  // Tokens that can be combined with others (but not with themselves).
  // We can have a maximum of 2 tokens, one combinable and one non-combinable.
  let combinables = new Set([
    UrlbarTokenizer.TYPE.RESTRICT_TITLE,
    UrlbarTokenizer.TYPE.RESTRICT_URL,
  ]);
  for (let token of tokens) {
    let tokenObj = {
      value: token,
      type: UrlbarTokenizer.TYPE.TEXT,
    };
    let restrictionType = UrlbarTokenizer.CHAR_TO_TYPE_MAP.get(token);
    if (tokens.length > 1 &&
        restrictionType &&
        foundRestriction.length == 0 ||
        (foundRestriction.length == 1 &&
         (combinables.has(foundRestriction[0]) && !combinables.has(restrictionType)) ||
         (!combinables.has(foundRestriction[0]) && combinables.has(restrictionType)))) {
      tokenObj.type = restrictionType;
      foundRestriction.push(restrictionType);
    } else if (UrlbarTokenizer.looksLikeOrigin(token)) {
      tokenObj.type = UrlbarTokenizer.TYPE.POSSIBLE_ORIGIN;
    } else if (UrlbarTokenizer.looksLikeUrl(token, {requirePath: true})) {
      tokenObj.type = UrlbarTokenizer.TYPE.POSSIBLE_URL;
    }
    filtered.push(tokenObj);
  }
  logger.info("Filtered Tokens", tokens);
  return filtered;
}
