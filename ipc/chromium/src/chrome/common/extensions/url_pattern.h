// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_EXTENSIONS_MATCH_PATTERN_H_
#define CHROME_BROWSER_EXTENSIONS_MATCH_PATTERN_H_

#include "googleurl/src/gurl.h"

// A pattern that can be used to match URLs. A URLPattern is a very restricted
// subset of URL syntax:
//
// <url-pattern> := <scheme>://<host><path>
// <scheme> := 'http' | 'https' | 'file' | 'ftp' | 'chrome'
// <host> := '*' | '*.' <anychar except '/' and '*'>+
// <path> := '/' <any chars>
//
// * Host is not used when the scheme is 'file'.
// * The path can have embedded '*' characters which act as glob wildcards.
//
// Examples of valid patterns:
// - http://*/*
// - http://*/foo*
// - https://*.google.com/foo*bar
// - chrome://foo/bar
// - file://monkey*
// - http://127.0.0.1/*
//
// Examples of invalid patterns:
// - http://* -- path not specified
// - http://*foo/bar -- * not allowed as substring of host component
// - http://foo.*.bar/baz -- * must be first component
// - http:/bar -- scheme separator not found
// - foo://* -- invalid scheme
//
// Design rationale:
// * We need to be able to tell users what 'sites' a given URLPattern will
//   affect. For example "This extension will interact with the site
//   'www.google.com'.
// * We'd like to be able to convert as many existing Greasemonkey @include
//   patterns to URLPatterns as possible. Greasemonkey @include patterns are
//   simple globs, so this won't be perfect.
// * Although we would like to support any scheme, it isn't clear what to tell
//   users about URLPatterns that affect data or javascript URLs, and saying
//   something useful about chrome-extension URLs is more work, so those are
//   left out for now.
//
// From a 2008-ish crawl of userscripts.org, the following patterns were found
// in @include lines:
// - total lines                    : 24471
// - @include *                     :   919
// - @include http://[^\*]+?/       : 11128 (no star in host)
// - @include http://\*\.[^\*]+?/   :  2325 (host prefixed by *.)
// - @include http://\*[^\.][^\*]+?/:  1524 (host prefixed by *, no dot -- many
//                                           appear to only need subdomain
//                                           matching, not real prefix matching)
// - @include http://[^\*/]+\*/     :   320 (host suffixed by *)
// - @include contains .tld         :   297 (host suffixed by .tld -- a special
//                                           Greasemonkey domain component that
//                                           tries to match all valid registry-
//                                           controlled suffixes)
// - @include http://\*/            :   228 (host is * exactly, but there is
//                                           more to the pattern)
//
// So, we can support at least half of current @include lines without supporting
// subdomain matching. We can pick up at least another 10% by supporting
// subdomain matching. It is probably possible to coerce more of the existing
// patterns to URLPattern, but the resulting pattern will be more restrictive
// than the original glob, which is probably better than nothing.
class URLPattern {
 public:
  URLPattern() : match_subdomains_(false) {}

  // Initializes this instance by parsing the provided string. On failure, the
  // instance will have some intermediate values and is in an invalid state.
  bool Parse(const std::string& pattern_str);

  // Returns true if this instance matches the specified URL.
  bool MatchesUrl(const GURL& url);

  std::string GetAsString() const;

  // Get the scheme the pattern matches. This will always return a valid scheme
  // if is_valid() returns true.
  std::string scheme() const { return scheme_; }

  // Gets the host the pattern matches. This can be an empty string if the
  // pattern matches all hosts (the input was <scheme>://*/<whatever>).
  std::string host() const { return host_; }

  // Gets whether to match subdomains of host().
  bool match_subdomains() const { return match_subdomains_; }

  // Gets the path the pattern matches with the leading slash. This can have
  // embedded asterisks which are interpreted using glob rules.
  std::string path() const { return path_; }

 private:
  // Returns true if |test| matches our host.
  bool MatchesHost(const GURL& test);

  // Returns true if |test| matches our path.
  bool MatchesPath(const GURL& test);

  // The scheme for the pattern.
  std::string scheme_;

  // The host without any leading "*" components.
  std::string host_;

  // Whether we should match subdomains of the host. This is true if the first
  // component of the pattern's host was "*".
  bool match_subdomains_;

  // The path to match. This is everything after the host of the URL, or
  // everything after the scheme in the case of file:// URLs.
  std::string path_;

  // The path with "?" and "\" characters escaped for use with the
  // MatchPattern() function. This is populated lazily, the first time it is
  // needed.
  std::string path_escaped_;
};

#endif  // CHROME_BROWSER_EXTENSIONS_MATCH_PATTERN_H_
