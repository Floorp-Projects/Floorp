/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

interface Cookie;
interface URI;

/**
 * A URL match pattern as used by the WebExtension and Chrome extension APIs.
 *
 * A match pattern is a string with one of the following formats:
 *
 *  - "<all_urls>"
 *    The literal string "<all_urls>" matches any URL with a supported
 *    protocol.
 *
 *  - <proto>://<host>/<path>
 *    A URL pattern with the following placeholders:
 *
 *    - <proto>
 *      The protocol to match, or "*" to match either "http" or "https".
 *    - <host>
 *      The hostname to match. May be either a complete, literal hostname to
 *      match a specific host, the wildcard character "*", to match any host,
 *      or a subdomain pattern, with "*." followed by a domain name, to match
 *      that domain name or any subdomain thereof.
 *    - <path>
 *      A glob pattern for paths to match. A "*" may appear anywhere within
 *      the path, and will match any string of characters. If no "*" appears,
 *      the URL path must exactly match the pattern path.
 */
[Constructor(DOMString pattern, optional MatchPatternOptions options),
 ChromeOnly, Exposed=(Window,System)]
interface MatchPattern {
  /**
   * Returns true if the given URI matches the pattern.
   *
   * If explicit is true, only explicit domain matches, without wildcards, are
   * considered.
   */
  [Throws]
  boolean matches(URI uri, optional boolean explicit = false);

  [Throws]
  boolean matches(DOMString url, optional boolean explicit = false);

  /**
   * Returns true if a URL exists which a) would be able to access the given
   * cookie, and b) would be matched by this match pattern.
   */
  boolean matchesCookie(Cookie cookie);

  /**
   * Returns true if this pattern will match any host which would be matched
   * by the given pattern.
   */
  boolean subsumes(MatchPattern pattern);

  /**
   * Returns true if there is any host which would be matched by both this
   * pattern and the given pattern.
   */
  boolean overlaps(MatchPattern pattern);

  /**
   * The match pattern string represented by this pattern.
   */
  [Constant]
  readonly attribute DOMString pattern;
};

/**
 * A set of MatchPattern objects, which implements the MatchPattern API and
 * matches when any of its sub-patterns matches.
 */
[Constructor(sequence<(DOMString or MatchPattern)> patterns, optional MatchPatternOptions options),
 ChromeOnly, Exposed=(Window,System)]
interface MatchPatternSet {
  /**
   * Returns true if the given URI matches any sub-pattern.
   *
   * If explicit is true, only explicit domain matches, without wildcards, are
   * considered.
   */
  [Throws]
  boolean matches(URI uri, optional boolean explicit = false);

  [Throws]
  boolean matches(DOMString url, optional boolean explicit = false);

  /**
   * Returns true if any sub-pattern matches the given cookie.
   */
  boolean matchesCookie(Cookie cookie);

  /**
   * Returns true if any sub-pattern subsumes the given pattern.
   */
  boolean subsumes(MatchPattern pattern);

  /**
   * Returns true if any sub-pattern overlaps the given pattern.
   */
  boolean overlaps(MatchPattern pattern);

  /**
   * Returns true if any sub-pattern overlaps any sub-pattern the given
   * pattern set.
   */
  boolean overlaps(MatchPatternSet patternSet);

  /**
   * Returns true if any sub-pattern overlaps *every* sub-pattern in the given
   * pattern set.
   */
  boolean overlapsAll(MatchPatternSet patternSet);

  [Cached, Constant, Frozen]
  readonly attribute sequence<MatchPattern> patterns;
};

dictionary MatchPatternOptions {
  /**
   * If true, the path portion of the pattern is ignored, and replaced with a
   * wildcard. The `pattern` property is updated to reflect this.
   */
  boolean ignorePath = false;

  /**
   * If true, the set of schemes this pattern can match is restricted to
   * those accessible by WebExtensions.
   */
  boolean restrictSchemes = true;
};
