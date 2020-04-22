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

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

ChromeUtils.defineModuleGetter(this, "Log", "resource://gre/modules/Log.jsm");
XPCOMUtils.defineLazyGetter(this, "logger", () =>
  Log.repository.getLogger("Urlbar.Tokenizer")
);

var UrlbarTokenizer = {
  // Regex matching on whitespaces.
  REGEXP_SPACES: /\s+/,

  // Regex used to guess url-like strings.
  // These are not expected to be 100% correct, we accept some user mistypes
  // and we're unlikely to be able to cover 100% of the cases.
  REGEXP_LIKE_PROTOCOL: /^[A-Z+.-]+:\/*(?!\/)/i,
  REGEXP_USERINFO_INVALID_CHARS: /[^\w.~%!$&'()*+,;=:-]/,
  REGEXP_HOSTPORT_INVALID_CHARS: /[^\[\]A-Z0-9.:-]/i,
  REGEXP_SINGLE_WORD_HOST: /^[^.:]+$/i,
  REGEXP_HOSTPORT_IP_LIKE: /^(?=(.*[.:].*){2})[a-f0-9\.\[\]:]+$/i,
  // This accepts partial IPv4.
  REGEXP_HOSTPORT_INVALID_IP: /\.{2,}|\d{5,}|\d{4,}(?![:\]])|^\.|^(\d+\.){4,}\d+$|^\d{4,}$/,
  // This only accepts complete IPv4.
  REGEXP_HOSTPORT_IPV4: /^(\d{1,3}\.){3,}\d{1,3}(:\d+)?$/,
  // This accepts partial IPv6.
  REGEXP_HOSTPORT_IPV6: /^\[([0-9a-f]{0,4}:){0,7}[0-9a-f]{0,4}\]?$/i,
  REGEXP_COMMON_EMAIL: /^[\w!#$%&'*+/=?^`{|}~.-]+@[\[\]A-Z0-9.-]+$/i,
  REGEXP_HAS_PORT: /:\d+$/,
  // Regex matching a percent encoded char at the beginning of a string.
  REGEXP_PERCENT_ENCODED_START: /^(%[0-9a-f]{2}){2,}/i,

  TYPE: {
    TEXT: 1,
    POSSIBLE_ORIGIN: 2, // It may be an ip, a domain, but even just a single word used as host.
    POSSIBLE_URL: 3, // Consumers should still check this with a fixup.
    RESTRICT_HISTORY: 4,
    RESTRICT_BOOKMARK: 5,
    RESTRICT_TAG: 6,
    RESTRICT_OPENPAGE: 7,
    RESTRICT_SEARCH: 8,
    RESTRICT_TITLE: 9,
    RESTRICT_URL: 10,
  },

  // The special characters below can be typed into the urlbar to restrict
  // the search to a certain category, like history, bookmarks or open pages; or
  // to force a match on just the title or url.
  // These restriction characters can be typed alone, or at word boundaries,
  // provided their meaning cannot be confused, for example # could be present
  // in a valid url, and thus it should not be interpreted as a restriction.
  RESTRICT: {
    HISTORY: "^",
    BOOKMARK: "*",
    TAG: "+",
    OPENPAGE: "%",
    SEARCH: "?",
    TITLE: "#",
    URL: "$",
  },

  /**
   * Returns whether the passed in token looks like a URL.
   * This is based on guessing and heuristics, that means if this function
   * returns false, it's surely not a URL, if it returns true, the result must
   * still be verified through URIFixup.
   *
   * @param {string} token
   *        The string token to verify
   * @param {boolean} [requirePath] The url must have a path
   * @returns {boolean} whether the token looks like a URL.
   */
  looksLikeUrl(token, { requirePath = false } = {}) {
    if (token.length < 2) {
      return false;
    }
    // Ignore spaces and require path for the data: protocol.
    if (token.startsWith("data:")) {
      return token.length > 5;
    }
    if (this.REGEXP_SPACES.test(token)) {
      return false;
    }
    // If it starts with something that looks like a protocol, it's likely a url.
    if (this.REGEXP_LIKE_PROTOCOL.test(token)) {
      return true;
    }
    // Guess path and prePath. At this point we should be analyzing strings not
    // having a protocol.
    let slashIndex = token.indexOf("/");
    let prePath = slashIndex != -1 ? token.slice(0, slashIndex) : token;
    if (!this.looksLikeOrigin(prePath, { ignoreWhitelist: true })) {
      return false;
    }

    let path = slashIndex != -1 ? token.slice(slashIndex) : "";
    logger.debug("path", path);
    if (requirePath && !path) {
      return false;
    }
    // If there are both path and userinfo, it's likely a url.
    let atIndex = prePath.indexOf("@");
    let userinfo = atIndex != -1 ? prePath.slice(0, atIndex) : "";
    if (path.length && userinfo.length) {
      return true;
    }

    // If the first character after the slash in the path is a letter, then the
    // token may be an "abc/def" url.
    if (/^\/[a-z]/i.test(path)) {
      return true;
    }
    // If the path contains special chars, it is likely a url.
    if (["%", "?", "#"].some(c => path.includes(c))) {
      return true;
    }

    // The above looksLikeOrigin call told us the prePath looks like an origin,
    // now we go into details checking some common origins.
    let hostPort = atIndex != -1 ? prePath.slice(atIndex + 1) : prePath;
    if (this.REGEXP_HOSTPORT_IPV4.test(hostPort)) {
      return true;
    }
    // ipv6 is very complex to support, just check for a few chars.
    if (
      this.REGEXP_HOSTPORT_IPV6.test(hostPort) &&
      ["[", "]", ":"].some(c => hostPort.includes(c))
    ) {
      return true;
    }
    if (Services.uriFixup.isDomainWhitelisted(hostPort)) {
      return true;
    }
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
   * @param {boolean} [ignoreWhitelist] If true, the origin doesn't have to be
   *        in the whitelist
   * @returns {boolean} whether the token looks like an origin.
   */
  looksLikeOrigin(token, { ignoreWhitelist = false } = {}) {
    if (!token.length) {
      return false;
    }
    let atIndex = token.indexOf("@");
    if (atIndex != -1 && this.REGEXP_COMMON_EMAIL.test(token)) {
      // We prefer handling it as an email rather than an origin with userinfo.
      return false;
    }
    let userinfo = atIndex != -1 ? token.slice(0, atIndex) : "";
    let hostPort = atIndex != -1 ? token.slice(atIndex + 1) : token;
    logger.debug("userinfo", userinfo);
    logger.debug("hostPort", hostPort);
    if (
      this.REGEXP_HOSTPORT_IPV4.test(hostPort) ||
      this.REGEXP_HOSTPORT_IPV6.test(hostPort)
    ) {
      return true;
    }

    // Check for invalid chars.
    if (
      this.REGEXP_LIKE_PROTOCOL.test(hostPort) ||
      this.REGEXP_USERINFO_INVALID_CHARS.test(userinfo) ||
      this.REGEXP_HOSTPORT_INVALID_CHARS.test(hostPort) ||
      (!this.REGEXP_SINGLE_WORD_HOST.test(hostPort) &&
        this.REGEXP_HOSTPORT_IP_LIKE.test(hostPort) &&
        this.REGEXP_HOSTPORT_INVALID_IP.test(hostPort))
    ) {
      return false;
    }

    // If it looks like a single word host, check the whitelist.
    if (
      !ignoreWhitelist &&
      !userinfo &&
      !this.REGEXP_HAS_PORT.test(hostPort) &&
      this.REGEXP_SINGLE_WORD_HOST.test(hostPort)
    ) {
      return Services.uriFixup.isDomainWhitelisted(hostPort);
    }

    return true;
  },

  /**
   * Tokenizes the searchString from a UrlbarQueryContext.
   * @param {UrlbarQueryContext} queryContext
   *        The query context object to tokenize
   * @returns {UrlbarQueryContext} the same query context object with a new
   *          tokens property.
   */
  tokenize(queryContext) {
    logger.info("Tokenizing", queryContext);
    let searchString = queryContext.searchString;
    if (!searchString.trim()) {
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
    return (
      token.type >= this.TYPE.RESTRICT_HISTORY &&
      token.type <= this.TYPE.RESTRICT_URL
    );
  },
};

const CHAR_TO_TYPE_MAP = new Map(
  Object.entries(UrlbarTokenizer.RESTRICT).map(([type, char]) => [
    char,
    UrlbarTokenizer.TYPE[`RESTRICT_${type}`],
  ])
);

/**
 * Given a search string, splits it into string tokens.
 * @param {string} searchString
 *        The search string to split
 * @returns {array} An array of string tokens.
 */
function splitString(searchString) {
  // The first step is splitting on unicode whitespaces. We ignore whitespaces
  // if the search string starts with "data:", to better support Web developers
  // and compatiblity with other browsers.
  let trimmed = searchString.trim();
  let tokens = trimmed.startsWith("data:")
    ? [trimmed]
    : trimmed.split(UrlbarTokenizer.REGEXP_SPACES);
  let accumulator = [];
  let hasRestrictionToken = tokens.some(t => CHAR_TO_TYPE_MAP.has(t));
  let chars = Array.from(CHAR_TO_TYPE_MAP.keys()).join("");
  logger.debug("Restriction chars", chars);
  for (let i = 0; i < tokens.length; ++i) {
    // If there is no separate restriction token, it's possible we have to split
    // a token, if it's the first one and it includes a leading restriction char
    // or it's the last one and it includes a trailing restriction char.
    // This allows to not require the user to add artificial whitespaces to
    // enforce restrictions, for example typing questions would restrict to
    // search results.
    let token = tokens[i];
    if (!hasRestrictionToken && token.length > 1) {
      // Check for an unambiguous restriction char at the beginning of the
      // first token, or at the end of the last token.
      if (
        i == 0 &&
        chars.includes(token[0]) &&
        !UrlbarTokenizer.REGEXP_PERCENT_ENCODED_START.test(token)
      ) {
        hasRestrictionToken = true;
        accumulator.push(token[0]);
        accumulator.push(token.slice(1));
        continue;
      } else if (
        i == tokens.length - 1 &&
        chars.includes(token[token.length - 1]) &&
        !UrlbarTokenizer.looksLikeUrl(token, { requirePath: true })
      ) {
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
 * @note restriction characters are only considered if they appear at the start
 *       or at the end of the tokens list. In case of restriction characters
 *       conflict, the most external ones win. Leading ones win over trailing
 *       ones. Discarded restriction characters are considered text.
 */
function filterTokens(tokens) {
  let filtered = [];
  let restrictions = [];
  for (let i = 0; i < tokens.length; ++i) {
    let token = tokens[i];
    let tokenObj = {
      value: token,
      lowerCaseValue: token.toLocaleLowerCase(),
      type: UrlbarTokenizer.TYPE.TEXT,
    };
    let restrictionType = CHAR_TO_TYPE_MAP.get(token);
    if (restrictionType) {
      restrictions.push({ index: i, type: restrictionType });
    } else if (UrlbarTokenizer.looksLikeOrigin(token)) {
      tokenObj.type = UrlbarTokenizer.TYPE.POSSIBLE_ORIGIN;
    } else if (UrlbarTokenizer.looksLikeUrl(token, { requirePath: true })) {
      tokenObj.type = UrlbarTokenizer.TYPE.POSSIBLE_URL;
    }
    filtered.push(tokenObj);
  }

  // Handle restriction characters.
  if (restrictions.length) {
    // We can apply two kind of restrictions: type (bookmark, search, ...) and
    // matching (url, title). These kind of restrictions can be combined, but we
    // can only have one restriction per kind.
    let matchingRestrictionFound = false;
    let typeRestrictionFound = false;
    function assignRestriction(r) {
      if (r && !(matchingRestrictionFound && typeRestrictionFound)) {
        if (
          [
            UrlbarTokenizer.TYPE.RESTRICT_TITLE,
            UrlbarTokenizer.TYPE.RESTRICT_URL,
          ].includes(r.type)
        ) {
          if (!matchingRestrictionFound) {
            matchingRestrictionFound = true;
            filtered[r.index].type = r.type;
            return true;
          }
        } else if (!typeRestrictionFound) {
          typeRestrictionFound = true;
          filtered[r.index].type = r.type;
          return true;
        }
      }
      return false;
    }

    // Look at the first token.
    let found = assignRestriction(restrictions.find(r => r.index == 0));
    if (found) {
      // If the first token was assigned, look at the next one.
      assignRestriction(restrictions.find(r => r.index == 1));
    }
    // Then look at the last token.
    let lastIndex = tokens.length - 1;
    found = assignRestriction(restrictions.find(r => r.index == lastIndex));
    if (found) {
      // If the last token was assigned, look at the previous one.
      assignRestriction(restrictions.find(r => r.index == lastIndex - 1));
    }
  }

  logger.info("Filtered Tokens", tokens);
  return filtered;
}
