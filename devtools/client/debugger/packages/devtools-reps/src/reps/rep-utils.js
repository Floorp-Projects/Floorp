/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Dependencies
const validProtocols = /(http|https|ftp|data|resource|chrome):/i;

// URL Regex, common idioms:
//
// Lead-in (URL):
// (                     Capture because we need to know if there was a lead-in
//                       character so we can include it as part of the text
//                       preceding the match. We lack look-behind matching.
//  ^|                   The URL can start at the beginning of the string.
//  [\s(,;'"`“]          Or whitespace or some punctuation that does not imply
//                       a context which would preclude a URL.
// )
//
// We do not need a trailing look-ahead because our regex's will terminate
// because they run out of characters they can eat.

// What we do not attempt to have the regexp do:
// - Avoid trailing '.' and ')' characters.  We let our greedy match absorb
//   these, but have a separate regex for extra characters to leave off at the
//   end.
//
// The Regex (apart from lead-in/lead-out):
// (                     Begin capture of the URL
//  (?:                  (potential detect beginnings)
//   https?:\/\/|        Start with "http" or "https"
//   www\d{0,3}[.][a-z0-9.\-]{2,249}|
//                      Start with "www", up to 3 numbers, then "." then
//                       something that looks domain-namey.  We differ from the
//                       next case in that we do not constrain the top-level
//                       domain as tightly and do not require a trailing path
//                       indicator of "/".  This is IDN root compatible.
//   [a-z0-9.\-]{2,250}[.][a-z]{2,4}\/
//                       Detect a non-www domain, but requiring a trailing "/"
//                       to indicate a path.  This only detects IDN domains
//                       with a non-IDN root.  This is reasonable in cases where
//                       there is no explicit http/https start us out, but
//                       unreasonable where there is.  Our real fix is the bug
//                       to port the Thunderbird/gecko linkification logic.
//
//                       Domain names can be up to 253 characters long, and are
//                       limited to a-zA-Z0-9 and '-'.  The roots don't have
//                       hyphens unless they are IDN roots.  Root zones can be
//                       found here: http://www.iana.org/domains/root/db
//  )
//  [-\w.!~*'();,/?:@&=+$#%]*
//                       path onwards. We allow the set of characters that
//                       encodeURI does not escape plus the result of escaping
//                       (so also '%')
// )
// eslint-disable-next-line max-len
const urlRegex = /(^|[\s(,;'"`“])((?:https?:\/\/|www\d{0,3}[.][a-z0-9.\-]{2,249}|[a-z0-9.\-]{2,250}[.][a-z]{2,4}\/)[-\w.!~*'();,/?:@&=+$#%]*)/im;

// Set of terminators that are likely to have been part of the context rather
// than part of the URL and so should be uneaten. This is '(', ',', ';', plus
// quotes and question end-ing punctuation and the potential permutations with
// parentheses (english-specific).
const uneatLastUrlCharsRegex = /(?:[),;.!?`'"]|[.!?]\)|\)[.!?])$/;

const ELLIPSIS = "\u2026";
const dom = require("react-dom-factories");
const { span } = dom;

/**
 * Returns true if the given object is a grip (see RDP protocol)
 */
function isGrip(object) {
  return object && object.actor;
}

function escapeNewLines(value) {
  return value.replace(/\r/gm, "\\r").replace(/\n/gm, "\\n");
}

// Map from character code to the corresponding escape sequence.  \0
// isn't here because it would require special treatment in some
// situations.  \b, \f, and \v aren't here because they aren't very
// common.  \' isn't here because there's no need, we only
// double-quote strings.
const escapeMap = {
  // Tab.
  9: "\\t",
  // Newline.
  0xa: "\\n",
  // Carriage return.
  0xd: "\\r",
  // Quote.
  0x22: '\\"',
  // Backslash.
  0x5c: "\\\\",
};

// Regexp that matches any character we might possibly want to escape.
// Note that we over-match here, because it's difficult to, say, match
// an unpaired surrogate with a regexp.  The details are worked out by
// the replacement function; see |escapeString|.
const escapeRegexp = new RegExp(
  "[" +
    // Quote and backslash.
    '"\\\\' +
    // Controls.
    "\x00-\x1f" +
    // More controls.
    "\x7f-\x9f" +
    // BOM
    "\ufeff" +
    // Specials, except for the replacement character.
    "\ufff0-\ufffc\ufffe\uffff" +
    // Surrogates.
    "\ud800-\udfff" +
    // Mathematical invisibles.
    "\u2061-\u2064" +
    // Line and paragraph separators.
    "\u2028-\u2029" +
    // Private use area.
    "\ue000-\uf8ff" +
    "]",
  "g"
);

/**
 * Escape a string so that the result is viewable and valid JS.
 * Control characters, other invisibles, invalid characters,
 * backslash, and double quotes are escaped.  The resulting string is
 * surrounded by double quotes.
 *
 * @param {String} str
 *        the input
 * @param {Boolean} escapeWhitespace
 *        if true, TAB, CR, and NL characters will be escaped
 * @return {String} the escaped string
 */
function escapeString(str, escapeWhitespace) {
  return `"${str.replace(escapeRegexp, (match, offset) => {
    const c = match.charCodeAt(0);
    if (c in escapeMap) {
      if (!escapeWhitespace && (c === 9 || c === 0xa || c === 0xd)) {
        return match[0];
      }
      return escapeMap[c];
    }
    if (c >= 0xd800 && c <= 0xdfff) {
      // Find the full code point containing the surrogate, with a
      // special case for a trailing surrogate at the start of the
      // string.
      if (c >= 0xdc00 && offset > 0) {
        --offset;
      }
      const codePoint = str.codePointAt(offset);
      if (codePoint >= 0xd800 && codePoint <= 0xdfff) {
        // Unpaired surrogate.
        return `\\u${codePoint.toString(16)}`;
      } else if (codePoint >= 0xf0000 && codePoint <= 0x10fffd) {
        // Private use area.  Because we visit each pair of a such a
        // character, return the empty string for one half and the
        // real result for the other, to avoid duplication.
        if (c <= 0xdbff) {
          return `\\u{${codePoint.toString(16)}}`;
        }
        return "";
      }
      // Other surrogate characters are passed through.
      return match;
    }
    return `\\u${`0000${c.toString(16)}`.substr(-4)}`;
  })}"`;
}

/**
 * Escape a property name, if needed.  "Escaping" in this context
 * means surrounding the property name with quotes.
 *
 * @param {String}
 *        name the property name
 * @return {String} either the input, or the input surrounded by
 *                  quotes, properly quoted in JS syntax.
 */
function maybeEscapePropertyName(name) {
  // Quote the property name if it needs quoting.  This particular
  // test is an approximation; see
  // https://mathiasbynens.be/notes/javascript-properties.  However,
  // the full solution requires a fair amount of Unicode data, and so
  // let's defer that until either it's important, or the \p regexp
  // syntax lands, see
  // https://github.com/tc39/proposal-regexp-unicode-property-escapes.
  if (!/^\w+$/.test(name)) {
    name = escapeString(name);
  }
  return name;
}

function cropMultipleLines(text, limit) {
  return escapeNewLines(cropString(text, limit));
}

function rawCropString(text, limit, alternativeText = ELLIPSIS) {
  // Crop the string only if a limit is actually specified.
  if (!limit || limit <= 0) {
    return text;
  }

  // Set the limit at least to the length of the alternative text
  // plus one character of the original text.
  if (limit <= alternativeText.length) {
    limit = alternativeText.length + 1;
  }

  const halfLimit = (limit - alternativeText.length) / 2;

  if (text.length > limit) {
    return (
      text.substr(0, Math.ceil(halfLimit)) +
      alternativeText +
      text.substr(text.length - Math.floor(halfLimit))
    );
  }

  return text;
}

function cropString(text, limit, alternativeText) {
  return rawCropString(sanitizeString(`${text}`), limit, alternativeText);
}

function sanitizeString(text) {
  // Replace all non-printable characters, except of
  // (horizontal) tab (HT: \x09) and newline (LF: \x0A, CR: \x0D),
  // with unicode replacement character (u+fffd).
  // eslint-disable-next-line no-control-regex
  const re = new RegExp("[\x00-\x08\x0B\x0C\x0E-\x1F\x7F-\x9F]", "g");
  return text.replace(re, "\ufffd");
}

function parseURLParams(url) {
  url = new URL(url);
  return parseURLEncodedText(url.searchParams);
}

function parseURLEncodedText(text) {
  const params = [];

  // In case the text is empty just return the empty parameters
  if (text == "") {
    return params;
  }

  const searchParams = new URLSearchParams(text);
  const entries = [...searchParams.entries()];
  return entries.map(entry => {
    return {
      name: entry[0],
      value: entry[1],
    };
  });
}

function getFileName(url) {
  const split = splitURLBase(url);
  return split.name;
}

function splitURLBase(url) {
  if (!isDataURL(url)) {
    return splitURLTrue(url);
  }
  return {};
}

function getURLDisplayString(url) {
  return cropString(url);
}

function isDataURL(url) {
  return url && url.substr(0, 5) == "data:";
}

function splitURLTrue(url) {
  const reSplitFile = /(.*?):\/{2,3}([^\/]*)(.*?)([^\/]*?)($|\?.*)/;
  const m = reSplitFile.exec(url);

  if (!m) {
    return {
      name: url,
      path: url,
    };
  } else if (m[4] == "" && m[5] == "") {
    return {
      protocol: m[1],
      domain: m[2],
      path: m[3],
      name: m[3] != "/" ? m[3] : m[2],
    };
  }

  return {
    protocol: m[1],
    domain: m[2],
    path: m[2] + m[3],
    name: m[4] + m[5],
  };
}

/**
 * Wrap the provided render() method of a rep in a try/catch block that will
 * render a fallback rep if the render fails.
 */
function wrapRender(renderMethod) {
  const wrappedFunction = function(props) {
    try {
      return renderMethod.call(this, props);
    } catch (e) {
      console.error(e);
      return span(
        {
          className: "objectBox objectBox-failure",
          title:
            "This object could not be rendered, " +
            "please file a bug on bugzilla.mozilla.org",
        },
        /* Labels have to be hardcoded for reps, see Bug 1317038. */
        "Invalid object"
      );
    }
  };
  wrappedFunction.propTypes = renderMethod.propTypes;
  return wrappedFunction;
}

/**
 * Get preview items from a Grip.
 *
 * @param {Object} Grip from which we want the preview items
 * @return {Array} Array of the preview items of the grip, or an empty array
 *                 if the grip does not have preview items
 */
function getGripPreviewItems(grip) {
  if (!grip) {
    return [];
  }

  // Promise resolved value Grip
  if (grip.promiseState && grip.promiseState.value) {
    return [grip.promiseState.value];
  }

  // Array Grip
  if (grip.preview && grip.preview.items) {
    return grip.preview.items;
  }

  // Node Grip
  if (grip.preview && grip.preview.childNodes) {
    return grip.preview.childNodes;
  }

  // Set or Map Grip
  if (grip.preview && grip.preview.entries) {
    return grip.preview.entries.reduce((res, entry) => res.concat(entry), []);
  }

  // Event Grip
  if (grip.preview && grip.preview.target) {
    const keys = Object.keys(grip.preview.properties);
    const values = Object.values(grip.preview.properties);
    return [grip.preview.target, ...keys, ...values];
  }

  // RegEx Grip
  if (grip.displayString) {
    return [grip.displayString];
  }

  // Generic Grip
  if (grip.preview && grip.preview.ownProperties) {
    let propertiesValues = Object.values(grip.preview.ownProperties).map(
      property => property.value || property
    );

    const propertyKeys = Object.keys(grip.preview.ownProperties);
    propertiesValues = propertiesValues.concat(propertyKeys);

    // ArrayBuffer Grip
    if (grip.preview.safeGetterValues) {
      propertiesValues = propertiesValues.concat(
        Object.values(grip.preview.safeGetterValues).map(
          property => property.getterValue || property
        )
      );
    }

    return propertiesValues;
  }

  return [];
}

/**
 * Get the type of an object.
 *
 * @param {Object} Grip from which we want the type.
 * @param {boolean} noGrip true if the object is not a grip.
 * @return {boolean}
 */
function getGripType(object, noGrip) {
  if (noGrip || Object(object) !== object) {
    return typeof object;
  }
  if (object.type === "object") {
    return object.class;
  }
  return object.type;
}

/**
 * Determines whether a grip is a string containing a URL.
 *
 * @param string grip
 *        The grip, which may contain a URL.
 * @return boolean
 *         Whether the grip is a string containing a URL.
 */
function containsURL(grip) {
  // An URL can't be shorter than 5 char (e.g. "ftp:").
  if (typeof grip !== "string" || grip.length < 5) {
    return false;
  }

  return validProtocols.test(grip);
}

/**
 * Determines whether a string token is a valid URL.
 *
 * @param string token
 *        The token.
 * @return boolean
 *         Whether the token is a URL.
 */
function isURL(token) {
  try {
    if (!validProtocols.test(token)) {
      return false;
    }
    new URL(token);
    return true;
  } catch (e) {
    return false;
  }
}

/**
 * Returns new array in which `char` are interleaved between the original items.
 *
 * @param {Array} items
 * @param {String} char
 * @returns Array
 */
function interleave(items, char) {
  return items.reduce((res, item, index) => {
    if (index !== items.length - 1) {
      return res.concat(item, char);
    }
    return res.concat(item);
  }, []);
}

const ellipsisElement = span(
  {
    key: "more",
    className: "more-ellipsis",
    title: `more${ELLIPSIS}`,
  },
  ELLIPSIS
);

module.exports = {
  interleave,
  isGrip,
  isURL,
  cropString,
  containsURL,
  rawCropString,
  sanitizeString,
  escapeString,
  wrapRender,
  cropMultipleLines,
  parseURLParams,
  parseURLEncodedText,
  getFileName,
  getURLDisplayString,
  maybeEscapePropertyName,
  getGripPreviewItems,
  getGripType,
  ellipsisElement,
  ELLIPSIS,
  uneatLastUrlCharsRegex,
  urlRegex,
};
