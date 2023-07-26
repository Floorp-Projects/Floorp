/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This module exports a tokenizer to be used by the urlbar model.
 * Emitted tokens are objects in the shape { type, value }, where type is one
 * of UrlbarTokenizer.TYPE.
 */

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  UrlbarUtils: "resource:///modules/UrlbarUtils.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "logger", () =>
  lazy.UrlbarUtils.getLogger({ prefix: "Tokenizer" })
);

export var UrlbarTokenizer = {
  // Regex matching on whitespaces.
  REGEXP_SPACES: /\s+/,
  REGEXP_SPACES_START: /^\s+/,

  // Regex used to guess url-like strings.
  // These are not expected to be 100% correct, we accept some user mistypes
  // and we're unlikely to be able to cover 100% of the cases.
  REGEXP_LIKE_PROTOCOL: /^[A-Z+.-]+:\/*(?!\/)/i,
  REGEXP_USERINFO_INVALID_CHARS: /[^\w.~%!$&'()*+,;=:-]/,
  REGEXP_HOSTPORT_INVALID_CHARS: /[^\[\]A-Z0-9.:-]/i,
  REGEXP_SINGLE_WORD_HOST: /^[^.:]+$/i,
  REGEXP_HOSTPORT_IP_LIKE: /^(?=(.*[.:].*){2})[a-f0-9\.\[\]:]+$/i,
  // This accepts partial IPv4.
  REGEXP_HOSTPORT_INVALID_IP:
    /\.{2,}|\d{5,}|\d{4,}(?![:\]])|^\.|^(\d+\.){4,}\d+$|^\d{4,}$/,
  // This only accepts complete IPv4.
  REGEXP_HOSTPORT_IPV4: /^(\d{1,3}\.){3,}\d{1,3}(:\d+)?$/,
  // This accepts partial IPv6.
  REGEXP_HOSTPORT_IPV6: /^\[([0-9a-f]{0,4}:){0,7}[0-9a-f]{0,4}\]?$/i,
  REGEXP_COMMON_EMAIL: /^[\w!#$%&'*+/=?^`{|}~.-]+@[\[\]A-Z0-9.-]+$/i,
  REGEXP_HAS_PORT: /:\d+$/,
  // Regex matching a percent encoded char at the beginning of a string.
  REGEXP_PERCENT_ENCODED_START: /^(%[0-9a-f]{2}){2,}/i,
  // Regex matching scheme and colon, plus, if present, two slashes.
  REGEXP_PREFIX: /^[a-z-]+:(?:\/){0,2}/i,

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
    RESTRICT_ACTION: 11,
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
    ACTION: ">",
  },

  // The keys of characters in RESTRICT that will enter search mode.
  get SEARCH_MODE_RESTRICT() {
    return new Set([
      this.RESTRICT.HISTORY,
      this.RESTRICT.BOOKMARK,
      this.RESTRICT.OPENPAGE,
      this.RESTRICT.SEARCH,
      this.RESTRICT.ACTION,
    ]);
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
    if (!this.looksLikeOrigin(prePath, { ignoreKnownDomains: true })) {
      return false;
    }

    let path = slashIndex != -1 ? token.slice(slashIndex) : "";
    lazy.logger.debug("path", path);
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
    if (Services.uriFixup.isDomainKnown(hostPort)) {
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
   * @param {object} options Options object
   * @param {boolean} [options.ignoreKnownDomains] If true, the origin doesn't have to be
   *        in the known domain list
   * @param {boolean} [options.noIp] If true, the origin cannot be an IP address
   * @param {boolean} [options.noPort] If true, the origin cannot have a port number
   * @returns {boolean} whether the token looks like an origin.
   */
  looksLikeOrigin(
    token,
    { ignoreKnownDomains = false, noIp = false, noPort = false } = {}
  ) {
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
    let hasPort = this.REGEXP_HAS_PORT.test(hostPort);
    lazy.logger.debug("userinfo", userinfo);
    lazy.logger.debug("hostPort", hostPort);
    if (noPort && hasPort) {
      return false;
    }
    if (
      this.REGEXP_HOSTPORT_IPV4.test(hostPort) ||
      this.REGEXP_HOSTPORT_IPV6.test(hostPort)
    ) {
      return !noIp;
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

    // If it looks like a single word host, check the known domains.
    if (
      !ignoreKnownDomains &&
      !userinfo &&
      !hasPort &&
      this.REGEXP_SINGLE_WORD_HOST.test(hostPort)
    ) {
      return Services.uriFixup.isDomainKnown(hostPort);
    }

    return true;
  },

  /**
   * Tokenizes the searchString from a UrlbarQueryContext.
   *
   * @param {UrlbarQueryContext} queryContext
   *        The query context object to tokenize
   * @returns {UrlbarQueryContext} the same query context object with a new
   *          tokens property.
   */
  tokenize(queryContext) {
    lazy.logger.debug(
      "Tokenizing search string",
      JSON.stringify(queryContext.searchString)
    );
    if (!queryContext.trimmedSearchString) {
      queryContext.tokens = [];
      return queryContext;
    }
    let unfiltered = splitString(queryContext.searchString);
    let tokens = filterTokens(unfiltered);
    queryContext.tokens = tokens;
    return queryContext;
  },

  /**
   * Given a token, tells if it's a restriction token.
   *
   * @param {object} token
   *   The token to check.
   * @returns {boolean} Whether the token is a restriction character.
   */
  isRestrictionToken(token) {
    return (
      token &&
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
 *
 * @param {string} searchString
 *        The search string to split
 * @returns {Array} An array of string tokens.
 */
function splitString(searchString) {
  // The first step is splitting on unicode whitespaces. We ignore whitespaces
  // if the search string starts with "data:", to better support Web developers
  // and compatiblity with other browsers.
  let trimmed = searchString.trim();
  let tokens;
  if (trimmed.startsWith("data:")) {
    tokens = [trimmed];
  } else if (trimmed.length < 500) {
    tokens = trimmed.split(UrlbarTokenizer.REGEXP_SPACES);
  } else {
    // If the string is very long, tokenizing all of it would be expensive. So
    // we only tokenize a part of it, then let the last token become a
    // catch-all.
    tokens = trimmed.substring(0, 500).split(UrlbarTokenizer.REGEXP_SPACES);
    tokens[tokens.length - 1] += trimmed.substring(500);
  }

  if (!tokens.length) {
    return tokens;
  }

  // If there is no separate restriction token, it's possible we have to split
  // a token, if it's the first one and it includes a leading restriction char
  // or it's the last one and it includes a trailing restriction char.
  // This allows to not require the user to add artificial whitespaces to
  // enforce restrictions, for example typing questions would restrict to
  // search results.
  const hasRestrictionToken = tokens.some(t => CHAR_TO_TYPE_MAP.has(t));
  if (hasRestrictionToken) {
    return tokens;
  }

  // Check for an unambiguous restriction char at the beginning of the first
  // token, or at the end of the last token. We only count trailing restriction
  // chars if they are the search restriction char, which is "?". This is to
  // allow for a typed question to yield only search results.
  const firstToken = tokens[0];
  if (
    CHAR_TO_TYPE_MAP.has(firstToken[0]) &&
    !UrlbarTokenizer.REGEXP_PERCENT_ENCODED_START.test(firstToken)
  ) {
    tokens[0] = firstToken.substring(1);
    tokens.splice(0, 0, firstToken[0]);
    return tokens;
  }

  const lastIndex = tokens.length - 1;
  const lastToken = tokens[lastIndex];
  if (
    lastToken[lastToken.length - 1] == UrlbarTokenizer.RESTRICT.SEARCH &&
    !UrlbarTokenizer.looksLikeUrl(lastToken, { requirePath: true })
  ) {
    tokens[lastIndex] = lastToken.substring(0, lastToken.length - 1);
    tokens.push(lastToken[lastToken.length - 1]);
  }

  return tokens;
}

/**
 * Given an array of unfiltered tokens, this function filters them and converts
 * to token objects with a type.
 *
 * @param {Array} tokens
 *        An array of strings, representing search tokens.
 * @returns {Array} An array of token objects.
 * Note: restriction characters are only considered if they appear at the start
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
    // For privacy reasons, we don't want to send a data (or other kind of) URI
    // to a search engine. So we want to parse any single long token below.
    if (tokens.length > 1 && token.length > 500) {
      filtered.push(tokenObj);
      break;
    }

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

  lazy.logger.info("Filtered Tokens", filtered);
  return filtered;
}
