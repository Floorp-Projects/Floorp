/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=2 ts=2 sts=2 expandtab
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This component handles fixing up URIs, by correcting obvious typos and adding
 * missing schemes.
 */

// TODO (Bug 1623383): This is a direct conversion from .cpp, as such a few
// functions are overly complex and should be cleaned up.
/* eslint complexity: ["error", 63] */

var EXPORTED_SYMBOLS = ["URIFixup", "URIFixupInfo"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);

XPCOMUtils.defineLazyServiceGetter(
  this,
  "externalProtocolService",
  "@mozilla.org/uriloader/external-protocol-service;1",
  "nsIExternalProtocolService"
);

XPCOMUtils.defineLazyServiceGetter(
  this,
  "defaultProtocolHandler",
  "@mozilla.org/network/protocol;1?name=default",
  "nsIProtocolHandler"
);

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "fixupSchemeTypos",
  "browser.fixup.typo.scheme",
  true
);
XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "dnsFirstForSingleWords",
  "browser.fixup.dns_first_for_single_words",
  false
);
XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "keywordEnabled",
  "keyword.enabled",
  true
);

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "alternateEnabled",
  "browser.fixup.alternate.enabled",
  true
);

const {
  FIXUP_FLAG_NONE,
  FIXUP_FLAG_ALLOW_KEYWORD_LOOKUP,
  FIXUP_FLAGS_MAKE_ALTERNATE_URI,
  FIXUP_FLAG_PRIVATE_CONTEXT,
  FIXUP_FLAG_FIX_SCHEME_TYPOS,
} = Ci.nsIURIFixup;

const COMMON_PROTOCOLS = ["http", "https", "ftp", "file"];

// Regex used to identify user:password tokens in url strings.
// This is not a strict character check, because we try to fixup this part of
// a the url too.
XPCOMUtils.defineLazyGetter(
  this,
  "userPasswordRegex",
  () => /^([a-z+.-]+:\/{0,3})*[^\/@]+@.+/i
);

// Regex used to identify ascii alphabetical characters.
XPCOMUtils.defineLazyGetter(this, "asciiAlphaRegex", () => /^[a-z]$/i);

// Regex used to identify ascii a-f characters.
XPCOMUtils.defineLazyGetter(this, "asciiHexRegex", () => /^[a-f0-9]$/i);

// Regex used to identify ascii numerical characters.
XPCOMUtils.defineLazyGetter(this, "asciiDigitRegex", () => /^[0-9]$/);

// Regex used to identify tab separated content (having at least 2 tabs).
XPCOMUtils.defineLazyGetter(this, "maxOneTabRegex", () => /^[^\t]*\t?[^\t]*$/);

// Regex to test if the protocol might actually be a url without a protocol:
//
//   http://www.faqs.org/rfcs/rfc1738.html
//   http://www.faqs.org/rfcs/rfc2396.html
//
// e.g. Anything of the form:
//
//   <hostname>:<port> or
//   <hostname>:<port>/
//
// Where <hostname> is a string of alphanumeric characters and dashes
// separated by dots.
// and <port> is a 5 or less digits. This actually breaks the rfc2396
// definition of a scheme which allows dots in schemes.
//
// Note:
//   People expecting this to work with
//   <user>:<password>@<host>:<port>/<url-path> will be disappointed!
//
// Note: Parser could be a lot tighter, tossing out silly hostnames
//       such as those containing consecutive dots and so on.
XPCOMUtils.defineLazyGetter(
  this,
  "possiblyHostPortRegex",
  () => /^[a-z0-9-]+(\.[a-z0-9-]+)*:[0-9]{1,5}([/?#]|$)/i
);

// Regex used to strip newlines.
XPCOMUtils.defineLazyGetter(this, "newLinesRegex", () => /[\r\n]/g);

// Regex used to match a possible protocol.
// This resembles the logic in Services.io.extractScheme, thus \t is admitted
// and stripped later. We don't use Services.io.extractScheme because of
// performance bottleneck caused by crossing XPConnect.
XPCOMUtils.defineLazyGetter(
  this,
  "possibleProtocolRegex",
  () => /^([a-z][a-z0-9.+\t-]*):/i
);

// Cache of whitelisted domains.
XPCOMUtils.defineLazyGetter(this, "domainsWhitelist", () => {
  let domains = new Set(
    Services.prefs
      .getChildList("browser.fixup.domainwhitelist.")
      .filter(p => Services.prefs.getBoolPref(p, false))
      .map(p => p.substring(30))
  );
  // Hold onto the observer.
  domains._observer = {
    observe(subject, topic, data) {
      let domain = data.substring(30);
      if (Services.prefs.getBoolPref(data, false)) {
        domains.add(domain);
      } else {
        domains.delete(domain);
      }
    },
    QueryInterface: ChromeUtils.generateQI([
      Ci.nsIObserver,
      Ci.nsISupportsWeakReference,
    ]),
  };
  Services.prefs.addObserver(
    "browser.fixup.domainwhitelist.",
    domains._observer,
    true
  );
  return domains;
});

function URIFixup() {}

URIFixup.prototype = {
  get FIXUP_FLAG_NONE() {
    return FIXUP_FLAG_NONE;
  },
  get FIXUP_FLAG_ALLOW_KEYWORD_LOOKUP() {
    return FIXUP_FLAG_ALLOW_KEYWORD_LOOKUP;
  },
  get FIXUP_FLAGS_MAKE_ALTERNATE_URI() {
    return FIXUP_FLAGS_MAKE_ALTERNATE_URI;
  },
  get FIXUP_FLAG_PRIVATE_CONTEXT() {
    return FIXUP_FLAG_PRIVATE_CONTEXT;
  },
  get FIXUP_FLAG_FIX_SCHEME_TYPOS() {
    return FIXUP_FLAG_FIX_SCHEME_TYPOS;
  },

  createFixupURI(uriString, fixupFlags = FIXUP_FLAG_NONE, postData) {
    return this.getFixupURIInfo(uriString, fixupFlags, postData).preferredURI;
  },

  getFixupURIInfo(uriString, fixupFlags = FIXUP_FLAG_NONE, postData) {
    let isPrivateContext = fixupFlags & FIXUP_FLAG_PRIVATE_CONTEXT;

    // Eliminate embedded newlines, which single-line text fields now allow,
    // and cleanup the empty spaces and tabs that might be on each end.
    uriString = uriString.trim().replace(newLinesRegex, "");

    if (!uriString) {
      throw new Components.Exception(
        "Should pass a non-null uri",
        Cr.NS_ERROR_FAILURE
      );
    }

    let info = new URIFixupInfo(uriString);

    let scheme = extractScheme(uriString);
    let isCommonProtocol = COMMON_PROTOCOLS.includes(scheme);
    // View-source is a pseudo scheme. We're interested in fixing up the stuff
    // after it. The easiest way to do that is to call this method again with
    // the "view-source:" lopped off and then prepend it again afterwards.
    if (scheme == "view-source") {
      // We disable keyword lookup and alternate URIs so that small typos don't
      // cause us to look at very different domains.
      let newFixupFlags =
        fixupFlags &
        ~FIXUP_FLAG_ALLOW_KEYWORD_LOOKUP &
        ~FIXUP_FLAGS_MAKE_ALTERNATE_URI;

      let innerURIString = uriString.substring(12).trim();

      // Prevent recursion.
      let innerScheme = extractScheme(innerURIString);
      if (innerScheme == "view-source") {
        throw new Components.Exception(
          "Prevent view-source recursion",
          Cr.NS_ERROR_FAILURE
        );
      }

      let innerInfo = this.getFixupURIInfo(
        innerURIString,
        newFixupFlags,
        postData
      );
      if (!innerInfo.preferredURI) {
        throw new Components.Exception("No preferredURI", Cr.NS_ERROR_FAILURE);
      }
      info.fixedURI = Services.io.newURI(
        "view-source:" + innerInfo.preferredURI.spec
      );
      info.preferredURI = info.fixedURI;
      return info;
    }

    if (scheme.length < 2) {
      // Check if it is a file path. We skip most schemes because the only case
      // where a file path may look like having a scheme is "X:" on Windows.
      let fileURI = fileURIFixup(uriString);
      if (fileURI) {
        info.fixedURI = info.preferredURI = fileURI;
        info.fixupChangedProtocol = true;
        return info;
      }
    }

    // Fix up common scheme typos.
    // TODO: Use levenshtein distance here?
    if (
      fixupSchemeTypos &&
      fixupFlags & FIXUP_FLAG_FIX_SCHEME_TYPOS &&
      scheme &&
      !isCommonProtocol
    ) {
      for (let [typo, fixed] of [
        ["ttp", "http"],
        ["htp", "http"],
        ["ttps", "https"],
        ["tps", "https"],
        ["ps", "https"],
        ["htps", "https"],
        ["ile", "file"],
        ["le", "file"],
      ]) {
        if (uriString.startsWith(typo)) {
          scheme = fixed;
          uriString = scheme + uriString.substring(typo.length);
          isCommonProtocol = true;
          break;
        }
      }
    }

    // Now we need to check whether "scheme" is something we don't
    // really know about.
    let isDefaultProtocolHandler =
      !isCommonProtocol &&
      scheme &&
      Services.io.getProtocolHandler(scheme) == defaultProtocolHandler;

    if (!isDefaultProtocolHandler || !possiblyHostPortRegex.test(uriString)) {
      // Just try to create an URL out of it.
      try {
        info.fixedURI = Services.io.newURI(uriString);
      } catch (ex) {
        if (ex.result != Cr.NS_ERROR_MALFORMED_URI) {
          throw ex;
        }
      }
    }

    // TODO (Bug 1588118): Should check FIXUP_FLAG_ALLOW_KEYWORD_LOOKUP
    // instead of FIXUP_FLAG_FIX_SCHEME_TYPOS.
    if (
      info.fixedURI &&
      isDefaultProtocolHandler &&
      keywordEnabled &&
      fixupFlags & FIXUP_FLAG_FIX_SCHEME_TYPOS &&
      scheme &&
      !externalProtocolService.externalProtocolHandlerExists(scheme)
    ) {
      if (!userPasswordRegex.test(uriString)) {
        // This basically means we're dealing with a theoretically valid
        // URI... but we have no idea how to load it. (e.g. "christmas:humbug")
        // It's more likely the user wants to search, and so we
        // chuck this over to their preferred search provider instead:
        tryKeywordFixupForURIInfo(uriString, info, isPrivateContext, postData);
      } else {
        // If the given URL has a user:password we can't just pass it to the
        // external protocol handler; we'll try using it with http instead
        // later
        info.fixedURI = null;
      }
    }

    if (info.fixedURI) {
      if (!info.preferredURI) {
        if (fixupFlags & FIXUP_FLAGS_MAKE_ALTERNATE_URI) {
          info.fixupCreatedAlternateURI = makeAlternateFixedURI(info);
        }
        info.preferredURI = info.fixedURI;
      }
      return info;
    }

    // Fix up protocol string before calling KeywordURIFixup, because
    // it cares about the hostname of such URIs.
    // Prune duff protocol schemes:
    //   ://totallybroken.url.com
    //   //shorthand.url.com
    let inputHadDuffProtocol = false;
    if (!scheme) {
      if (uriString.startsWith("://")) {
        uriString = uriString.substring(3);
        inputHadDuffProtocol = true;
      } else if (uriString.startsWith("//")) {
        uriString = uriString.substring(2);
        inputHadDuffProtocol = true;
      }
    }

    // Avoid fixing up content that looks like tab-separated values.
    // Assume that 1 tab is accidental, but more than 1 implies this is
    // supposed to be tab-separated content.
    if (!isCommonProtocol && maxOneTabRegex.test(uriString)) {
      let uriWithProtocol = fixupURIProtocol(uriString, info);
      if (uriWithProtocol) {
        info.fixedURI = uriWithProtocol;
      }
    }

    // See if it is a keyword and whether a keyword must be fixed up.
    if (
      keywordEnabled &&
      fixupFlags & FIXUP_FLAG_ALLOW_KEYWORD_LOOKUP &&
      !inputHadDuffProtocol
    ) {
      keywordURIFixup(uriString, info, isPrivateContext, postData);
      if (info.preferredURI) {
        return info;
      }
    }

    // Did the caller want us to try an alternative URI?
    // If so, attempt to fixup http://foo into http://www.foo.com
    if (info.fixedURI && fixupFlags & FIXUP_FLAGS_MAKE_ALTERNATE_URI) {
      info.fixupCreatedAlternateURI = makeAlternateFixedURI(info);
    }

    if (info.fixedURI) {
      info.preferredURI = info.fixedURI;
      return info;
    }

    // If we still haven't been able to construct a valid URI, try to force a
    // keyword match.  This catches search strings with '.' or ':' in them.
    if (keywordEnabled && fixupFlags & FIXUP_FLAG_ALLOW_KEYWORD_LOOKUP) {
      tryKeywordFixupForURIInfo(
        info.originalInput,
        info,
        isPrivateContext,
        postData
      );
    }

    if (!info.preferredURI) {
      // we couldn't salvage anything.
      throw new Components.Exception(
        "Couldn't build a valid uri",
        Cr.NS_ERROR_MALFORMED_URI
      );
    }

    return info;
  },

  webNavigationFlagsToFixupFlags(href, navigationFlags) {
    try {
      Services.io.newURI(href);
      // Remove LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP for valid uris.
      navigationFlags &= ~Ci.nsIWebNavigation
        .LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP;
    } catch (ex) {}

    let fixupFlags = FIXUP_FLAG_NONE;
    if (
      navigationFlags & Ci.nsIWebNavigation.LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP
    ) {
      fixupFlags |= FIXUP_FLAG_ALLOW_KEYWORD_LOOKUP;
    }
    if (navigationFlags & Ci.nsIWebNavigation.LOAD_FLAGS_FIXUP_SCHEME_TYPOS) {
      fixupFlags |= FIXUP_FLAG_FIX_SCHEME_TYPOS;
    }
    return fixupFlags;
  },

  keywordToURI(keyword, isPrivateContext, postData) {
    if (Services.appinfo.processType == Ci.nsIXULRuntime.PROCESS_TYPE_CONTENT) {
      // There's no search service in the content process, thus all the calls
      // from it that care about keywords conversion should go through the
      // parent process.
      throw new Components.Exception(
        "Can't invoke URIFixup in the content process",
        Cr.NS_ERROR_NOT_AVAILABLE
      );
    }
    let info = new URIFixupInfo(keyword);

    // Strip leading "?" and leading/trailing spaces from aKeyword
    if (keyword.startsWith("?")) {
      keyword = keyword.substring(1);
    }
    keyword = keyword.trim();

    // Try falling back to the search service's default search engine
    // We must use an appropriate search engine depending on the private
    // context.
    let engine = isPrivateContext
      ? Services.search.defaultPrivateEngine
      : Services.search.defaultEngine;

    // We allow default search plugins to specify alternate parameters that are
    // specific to keyword searches.
    let responseType = null;
    if (engine.supportsResponseType("application/x-moz-keywordsearch")) {
      responseType = "application/x-moz-keywordsearch";
    }
    let submission = engine.getSubmission(keyword, responseType, "keyword");
    if (!submission) {
      throw new Components.Exception(
        "Invalid search submission",
        Cr.NS_ERROR_NOT_AVAILABLE
      );
    }
    let submissionPostDataStream = submission.postData;
    if (submissionPostDataStream) {
      if (postData) {
        // TODO (Bug 1626016): instead of having postData as an optional out
        // argument, it could be part of URIFixupInfo.
        postData.value = submissionPostDataStream;
      } else {
        // The submission specifies POST data (i.e. the search
        // engine's "method" is POST), but our caller didn't allow
        // passing post data back. No point passing back a URL that
        // won't load properly.
        throw new Components.Exception(
          "Didn't request POST data",
          Cr.NS_ERROR_NOT_AVAILABLE
        );
      }
    }

    info.keywordProviderName = engine.name;
    info.keywordAsSent = keyword;
    info.preferredURI = submission.uri;
    return info;
  },

  isDomainWhitelisted(asciiHost, dotLoc) {
    if (dnsFirstForSingleWords) {
      return true;
    }
    // Check if this domain is whitelisted as an actual
    // domain (which will prevent a keyword query)
    // NB: any processing of the host here should stay in sync with
    // code in the front-end(s) that set the pref.
    if (dotLoc == asciiHost.length - 1) {
      asciiHost = asciiHost.substring(0, asciiHost.length - 1);
    }
    return domainsWhitelist.has(asciiHost);
  },

  classID: Components.ID("{c6cf88b7-452e-47eb-bdc9-86e3561648ef}"),
  _xpcom_factory: XPCOMUtils.generateSingletonFactory(URIFixup),
  QueryInterface: ChromeUtils.generateQI([Ci.nsIURIFixup]),
};

function URIFixupInfo(originalInput = "") {
  this._originalInput = originalInput;
}

URIFixupInfo.prototype = {
  set consumer(consumer) {
    this._consumer = consumer || null;
  },
  get consumer() {
    return this._consumer || null;
  },

  set preferredURI(uri) {
    this._preferredURI = uri;
  },
  get preferredURI() {
    return this._preferredURI || null;
  },

  set fixedURI(uri) {
    this._fixedURI = uri;
  },
  get fixedURI() {
    return this._fixedURI || null;
  },

  set keywordProviderName(name) {
    this._keywordProviderName = name;
  },
  get keywordProviderName() {
    return this._keywordProviderName || "";
  },

  set keywordAsSent(keyword) {
    this._keywordAsSent = keyword;
  },
  get keywordAsSent() {
    return this._keywordAsSent || "";
  },

  set fixupChangedProtocol(changed) {
    this._fixupChangedProtocol = changed;
  },
  get fixupChangedProtocol() {
    return !!this._fixupChangedProtocol;
  },

  set fixupCreatedAlternateURI(changed) {
    this._fixupCreatedAlternateURI = changed;
  },
  get fixupCreatedAlternateURI() {
    return !!this._fixupCreatedAlternateURI;
  },

  set originalInput(input) {
    this._originalInput = input;
  },
  get originalInput() {
    return this._originalInput || "";
  },

  classID: Components.ID("{33d75835-722f-42c0-89cc-44f328e56a86}"),
  QueryInterface: ChromeUtils.generateQI([Ci.nsIURIFixupInfo]),
};

// Helpers

function tryKeywordFixupForURIInfo(
  uriString,
  fixupInfo,
  isPrivateContext,
  postData
) {
  try {
    let keywordInfo = Services.uriFixup.keywordToURI(
      uriString,
      isPrivateContext,
      postData
    );
    fixupInfo.keywordProviderName = keywordInfo.keywordProviderName;
    fixupInfo.keywordAsSent = keywordInfo.keywordAsSent;
    fixupInfo.preferredURI = keywordInfo.preferredURI;
  } catch (ex) {}
}

function makeAlternateFixedURI(info) {
  let uri = info.fixedURI;
  if (
    !alternateEnabled ||
    // Code only works for http. Not for any other protocol including https!
    !uri.schemeIs("http") ||
    // Security - URLs with user / password info should NOT be fixed up
    uri.userPass ||
    // Don't fix up hosts with ports
    uri.port != -1
  ) {
    return false;
  }

  let oldHost = uri.host;
  // Don't fix up 'localhost' because that's confusing:
  if (oldHost == "localhost") {
    return false;
  }
  let numDots = (oldHost.match(/\./g) || []).length;

  // Get the prefix and suffix to stick onto the new hostname. By default these
  // are www. & .com but they could be any other value, e.g. www. & .org
  let prefix = Services.prefs.getCharPref(
    "browser.fixup.alternate.prefix",
    "www."
  );
  let suffix = Services.prefs.getCharPref(
    "browser.fixup.alternate.suffix",
    ".com"
  );

  let newHost = "";
  if (numDots == 0) {
    newHost = prefix + oldHost + suffix;
  } else if (numDots == 1) {
    if (prefix && oldHost.toLowerCase() == prefix) {
      newHost = oldHost + suffix;
    } else if (suffix) {
      newHost = prefix + oldHost;
    }
  }
  if (!newHost) {
    return false;
  }

  // Assign the new host string over the old one
  try {
    info.fixedURI = uri
      .mutate()
      .setHost(newHost)
      .finalize();
  } catch (ex) {
    if (ex.result != Cr.NS_ERROR_MALFORMED_URI) {
      throw ex;
    }
    return false;
  }
  return true;
}

/**
 * Try to fixup a file URI.
 * @param uriString The file URI to fix.
 * @returns {nsIURI} a fixed uri or null.
 * @note FileURIFixup only returns a URI if it has to add the file: protocol.
 */
function fileURIFixup(uriString) {
  let attemptFixup = false;
  if (AppConstants.platform == "win") {
    // Check for "\"" in the url-string or just a drive (e.g. C:).
    attemptFixup =
      uriString.includes("\\") ||
      (uriString.length == 2 && uriString.endsWith(":"));
  } else {
    // UNIX: Check if it starts with "/".
    attemptFixup = uriString.startsWith("/");
  }
  if (attemptFixup) {
    try {
      // Test if this is a valid path by trying to create a local file
      // object. The URL of that is returned if successful.
      let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
      file.initWithPath(uriString);
      return Services.io.newFileURI(file);
    } catch (ex) {
      // Not a file uri.
    }
  }
  return null;
}

function fixupURIProtocol(uriString, fixupInfo) {
  // Add http:// to front of url if it has no spec
  //
  // Should fix:
  //
  //   no-scheme.com
  //   ftp.no-scheme.com
  //   ftp4.no-scheme.com
  //   no-scheme.com/query?foo=http://www.foo.com
  //   user:pass@no-scheme.com
  //
  let schemePos = uriString.indexOf("://");
  if (schemePos == -1 || schemePos > uriString.search(/[:\/]/)) {
    uriString = "http://" + uriString;
    // TODO: This should only be set if this code runs and newURI succeeds
    // below, but tests fail and some code may depend on this bug.
    fixupInfo.fixupChangedProtocol = true;
  }
  try {
    return Services.io.newURI(uriString);
  } catch (ex) {
    // We generated an invalid uri.
  }
  return null;
}

function keywordURIFixup(uriString, fixupInfo, isPrivateContext, postData) {
  // These are keyword formatted strings
  // "what is mozilla"
  // "what is mozilla?"
  // "docshell site:mozilla.org" - has no dot/colon in the first space-separated
  // substring
  // "?mozilla" - anything that begins with a question mark
  // "?site:mozilla.org docshell"
  // Things that have a quote before the first dot/colon
  // "mozilla" - checked against a whitelist to see if it's a host or not
  // ".mozilla", "mozilla." - ditto

  // These are not keyword formatted strings
  // "www.blah.com" - first space-separated substring contains a dot, doesn't
  // start with "?" "www.blah.com stuff" "nonQualifiedHost:80" - first
  // space-separated substring contains a colon, doesn't start with "?"
  // "nonQualifiedHost:80 args"
  // "nonQualifiedHost?"
  // "nonQualifiedHost?args"
  // "nonQualifiedHost?some args"
  // "blah.com."

  // Note: -1 is greater than any actual location
  // in practice.  So if we cast all locations to uint32_t, then a <
  // b guarantees that either b is kNotFound and a is found, or both
  // are found and a found before b.

  // TODO: the cpp code used to assign -1 to an unsigned int, causing overflow.
  // We should replace most of this with better js code.
  const NOT_FOUND = Infinity;
  let firstDotLoc = NOT_FOUND;
  let lastDotLoc = NOT_FOUND;
  let firstColonLoc = NOT_FOUND;
  let firstQuoteLoc = NOT_FOUND;
  let firstSpaceLoc = NOT_FOUND;
  let firstQMarkLoc = NOT_FOUND;
  let lastLSBracketLoc = NOT_FOUND;
  let lastSlashLoc = NOT_FOUND;
  let foundDots = 0;
  let foundColons = 0;
  let foundDigits = 0;
  let foundRSBrackets = 0;
  let looksLikeIpv6 = true;
  let hasAsciiAlpha = false;
  for (let i = 0, pos = 0; i < uriString.length; ++i, ++pos) {
    let iter = uriString[i];
    if (pos >= 1 && foundRSBrackets == 0) {
      if (
        !(
          lastLSBracketLoc == 0 &&
          (iter == ":" ||
            iter == "." ||
            iter == "]" ||
            asciiHexRegex.test(iter))
        )
      ) {
        looksLikeIpv6 = false;
      }
    }

    // If we're at the end of the string or this is the first slash,
    // check if the thing before the slash looks like ipv4:
    if (
      (i == uriString.length - 1 ||
        (lastSlashLoc == NOT_FOUND && iter == "/")) &&
      // Need 2 or 3 dots + only digits
      (foundDots == 2 || foundDots == 3) &&
      // and they should be all that came before now:
      (foundDots + foundDigits == pos ||
        // or maybe there was also exactly 1 colon that came after the last
        // dot, and the digits, dots and colon were all that came before now:
        (foundColons == 1 &&
          firstColonLoc > lastDotLoc &&
          foundDots + foundDigits + foundColons == pos))
    ) {
      // Hurray, we got ourselves some ipv4!
      // At this point, there's no way we will do a keyword lookup, so just bail
      // immediately:
      return;
    }

    if (iter == ".") {
      foundDots++;
      lastDotLoc = pos;
      if (firstDotLoc == NOT_FOUND) {
        firstDotLoc = pos;
      }
    } else if (iter == ":") {
      foundColons++;
      if (firstColonLoc == NOT_FOUND) {
        firstColonLoc = pos;
      }
    } else if (iter == " " && firstSpaceLoc == NOT_FOUND) {
      firstSpaceLoc = pos;
    } else if (iter == "?" && firstQMarkLoc == NOT_FOUND) {
      firstQMarkLoc = pos;
    } else if ((iter == "'" || iter == '"') && firstQuoteLoc == NOT_FOUND) {
      firstQuoteLoc = pos;
    } else if (iter == "[") {
      lastLSBracketLoc = pos;
    } else if (iter == "]") {
      foundRSBrackets++;
    } else if (iter == "/") {
      lastSlashLoc = pos;
    } else if (asciiAlphaRegex.test(iter)) {
      hasAsciiAlpha = true;
    } else if (asciiDigitRegex.test(iter)) {
      foundDigits++;
    }
  }

  if (lastLSBracketLoc > 0 || foundRSBrackets != 1) {
    looksLikeIpv6 = false;
  }

  // If there are only colons and only hexadecimal characters ([a-z][0-9])
  // enclosed in [], then don't do a keyword lookup
  if (looksLikeIpv6) {
    return;
  }

  let asciiHost = fixupInfo.fixedURI && fixupInfo.fixedURI.asciiHost;
  let displayHost = fixupInfo.fixedURI && fixupInfo.fixedURI.displayHost;

  // We do keyword lookups if a space or quote preceded the dot, colon
  // or question mark (or if the latter is not found, or if the input starts
  // with a question mark)
  if (
    ((firstSpaceLoc < firstDotLoc || firstQuoteLoc < firstDotLoc) &&
      (firstSpaceLoc < firstColonLoc || firstQuoteLoc < firstColonLoc) &&
      (firstSpaceLoc < firstQMarkLoc || firstQuoteLoc < firstQMarkLoc)) ||
    firstQMarkLoc == 0
  ) {
    tryKeywordFixupForURIInfo(
      fixupInfo.originalInput,
      fixupInfo,
      isPrivateContext,
      postData
    );
    // ... or when the asciiHost is the same as displayHost and there are no
    // characters from [a-z][A-Z]
  } else if (
    asciiHost &&
    displayHost &&
    !hasAsciiAlpha &&
    asciiHost.toLowerCase() == displayHost.toLowerCase()
  ) {
    if (!dnsFirstForSingleWords) {
      tryKeywordFixupForURIInfo(
        fixupInfo.originalInput,
        fixupInfo,
        isPrivateContext,
        postData
      );
    }
    // ... or if there is no question mark or colon, and there is either no
    // dot, or exactly 1 and it is the first or last character of the input:
  } else if (
    (firstDotLoc == NOT_FOUND ||
      (foundDots == 1 &&
        (firstDotLoc == 0 || firstDotLoc == uriString.length - 1))) &&
    firstColonLoc == NOT_FOUND &&
    firstQMarkLoc == NOT_FOUND
  ) {
    if (
      asciiHost &&
      Services.uriFixup.isDomainWhitelisted(asciiHost, firstDotLoc)
    ) {
      return;
    }

    // ... unless there are no dots, and a slash, and alpha characters, and
    // this is a valid host:
    if (
      firstDotLoc == NOT_FOUND &&
      lastSlashLoc != NOT_FOUND &&
      hasAsciiAlpha &&
      asciiHost
    ) {
      return;
    }

    // If we get here, we don't have a valid URI, or we did but the
    // host is not whitelisted, so we do a keyword search *anyway*:
    tryKeywordFixupForURIInfo(
      fixupInfo.originalInput,
      fixupInfo,
      isPrivateContext,
      postData
    );
  }
}

function extractScheme(uriString) {
  let matches = uriString.match(possibleProtocolRegex);
  return matches ? matches[1].replace("\t", "").toLowerCase() : "";
}
