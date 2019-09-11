/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

interface LoadInfo;
interface URI;
interface WindowProxy;

typedef (MatchPatternSet or sequence<DOMString>) MatchPatternSetOrStringSequence;
typedef (MatchGlob or DOMString) MatchGlobOrString;

[ChromeOnly, Exposed=Window]
interface MozDocumentMatcher {
  [Throws]
  constructor(MozDocumentMatcherInit options);

  /**
   * Returns true if the script's match and exclude patterns match the given
   * URI, without reference to attributes such as `allFrames`.
   */
  boolean matchesURI(URI uri);

  /**
   * Returns true if the the given URI and LoadInfo objects match.
   * This should be used to determine whether to begin pre-loading a content
   * script based on network events.
   */
  boolean matchesLoadInfo(URI uri, LoadInfo loadInfo);

  /**
   * Returns true if the given window matches. This should be used
   * to determine whether to run a script in a window at load time.
   */
  boolean matchesWindow(WindowProxy window);

  /**
   * If true, match all frames. If false, match only top-level frames.
   */
  [Constant]
  readonly attribute boolean allFrames;

  /**
   * If true, this (misleadingly-named, but inherited from Chrome) attribute
   * causes us to match frames with URLs which inherit a principal that
   * matches one of the match patterns, such as about:blank or about:srcdoc.
   * If false, we only match frames with an explicit matching URL.
   */
  [Constant]
  readonly attribute boolean matchAboutBlank;

  /**
   * The outer window ID of the frame in which to run the script, or 0 if it
   * should run in the top-level frame. Should only be used for
   * dynamically-injected scripts.
   */
  [Constant]
  readonly attribute unsigned long long? frameID;

  /**
   * The set of match patterns for URIs of pages in which this script should
   * run. This attribute is mandatory, and is a prerequisite for all other
   * match patterns.
   */
  [Constant]
  readonly attribute MatchPatternSet matches;

  /**
   * A set of match patterns for URLs in which this script should not run,
   * even if they match other include patterns or globs.
   */
  [Constant]
  readonly attribute MatchPatternSet? excludeMatches;

  /**
   * A set of glob matchers for URLs in which this script should run. If this
   * list is present, the script will only run in URLs which match the
   * `matches` pattern as well as one of these globs.
   */
  [Cached, Constant, Frozen]
  readonly attribute sequence<MatchGlob>? includeGlobs;

  /**
   * A set of glob matchers for URLs in which this script should not run, even
   * if they match other include patterns or globs.
   */
  [Cached, Constant, Frozen]
  readonly attribute sequence<MatchGlob>? excludeGlobs;

  /**
   * The policy object for the extension that this matcher belongs to.
   */
  [Constant]
  readonly attribute WebExtensionPolicy? extension;
};

dictionary MozDocumentMatcherInit {
  boolean allFrames = false;

  boolean matchAboutBlank = false;

  unsigned long long? frameID = null;

  required MatchPatternSetOrStringSequence matches;

  MatchPatternSetOrStringSequence? excludeMatches = null;

  sequence<MatchGlobOrString>? includeGlobs = null;

  sequence<MatchGlobOrString>? excludeGlobs = null;

  boolean hasActiveTabPermission = false;
};

/**
 * Describes the earliest point in the load cycle at which a script should
 * run.
 */
enum ContentScriptRunAt {
  /**
   * The point in the load cycle just after the document element has been
   * inserted, before any page scripts have been allowed to run.
   */
  "document_start",
  /**
   * The point after which the page DOM has fully loaded, but before all page
   * resources have necessarily been loaded. Corresponds approximately to the
   * DOMContentLoaded event.
   */
  "document_end",
  /**
   * The first point after the page and all of its resources has fully loaded
   * when the event loop is idle, and can run scripts without delaying a paint
   * event.
   */
  "document_idle",
};

[ChromeOnly, Exposed=Window]
interface WebExtensionContentScript : MozDocumentMatcher {
  [Throws]
  constructor(WebExtensionPolicy extension,
	      WebExtensionContentScriptInit options);

  /**
   * The earliest point in the load cycle at which this script should run. For
   * static content scripts, in extensions which were present at browser
   * startup, the browser makes every effort to make sure that the script runs
   * no later than this point in the load cycle. For dynamic content scripts,
   * and scripts from extensions installed during this session, the scripts
   * may run at a later point.
   */
  [Constant]
  readonly attribute ContentScriptRunAt runAt;

  /**
   * A set of paths, relative to the extension root, of CSS sheets to inject
   * into matching pages.
   */
  [Cached, Constant, Frozen]
  readonly attribute sequence<DOMString> cssPaths;

  /**
   * A set of paths, relative to the extension root, of JavaScript scripts to
   * execute in matching pages.
   */
  [Cached, Constant, Frozen]
  readonly attribute sequence<DOMString> jsPaths;
};

dictionary WebExtensionContentScriptInit : MozDocumentMatcherInit {
  ContentScriptRunAt runAt = "document_idle";

  sequence<DOMString> cssPaths = [];

  sequence<DOMString> jsPaths = [];
};
