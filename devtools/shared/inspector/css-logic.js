/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { getRootBindingParent } = require("devtools/shared/layout/utils");
const { getTabPrefs } = require("devtools/shared/indentation");

/*
 * About the objects defined in this file:
 * - CssLogic contains style information about a view context. It provides
 *   access to 2 sets of objects: Css[Sheet|Rule|Selector] provide access to
 *   information that does not change when the selected element changes while
 *   Css[Property|Selector]Info provide information that is dependent on the
 *   selected element.
 *   Its key methods are highlight(), getPropertyInfo() and forEachSheet(), etc
 *   It also contains a number of static methods for l10n, naming, etc
 *
 * - CssSheet provides a more useful API to a DOM CSSSheet for our purposes,
 *   including shortSource and href.
 * - CssRule a more useful API to a nsIDOMCSSRule including access to the group
 *   of CssSelectors that the rule provides properties for
 * - CssSelector A single selector - i.e. not a selector group. In other words
 *   a CssSelector does not contain ','. This terminology is different from the
 *   standard DOM API, but more inline with the definition in the spec.
 *
 * - CssPropertyInfo contains style information for a single property for the
 *   highlighted element.
 * - CssSelectorInfo is a wrapper around CssSelector, which adds sorting with
 *   reference to the selected element.
 */

/**
 * Provide access to the style information in a page.
 * CssLogic uses the standard DOM API, and the Gecko inIDOMUtils API to access
 * styling information in the page, and present this to the user in a way that
 * helps them understand:
 * - why their expectations may not have been fulfilled
 * - how browsers process CSS
 * @constructor
 */

const Services = require("Services");

loader.lazyImporter(this, "findCssSelector", "resource://gre/modules/css-selector.js");

const CSSLexer = require("devtools/shared/css/lexer");
const {LocalizationHelper} = require("devtools/shared/l10n");
const styleInspectorL10N =
  new LocalizationHelper("devtools/shared/locales/styleinspector.properties");

/**
 * Special values for filter, in addition to an href these values can be used
 */
exports.FILTER = {
  // show properties for all user style sheets.
  USER: "user",
  // USER, plus user-agent (i.e. browser) style sheets
  UA: "ua",
};

/**
 * Each rule has a status, the bigger the number, the better placed it is to
 * provide styling information.
 *
 * These statuses are localized inside the styleinspector.properties
 * string bundle.
 * @see csshtmltree.js RuleView._cacheStatusNames()
 */
exports.STATUS = {
  BEST: 3,
  MATCHED: 2,
  PARENT_MATCH: 1,
  UNMATCHED: 0,
  UNKNOWN: -1,
};

/**
 * Lookup a l10n string in the shared styleinspector string bundle.
 *
 * @param {String} name
 *        The key to lookup.
 * @returns {String} A localized version of the given key.
 */
exports.l10n = name => styleInspectorL10N.getStr(name);

/**
 * Is the given property sheet a content stylesheet?
 *
 * @param {CSSStyleSheet} sheet a stylesheet
 * @return {boolean} true if the given stylesheet is a content stylesheet,
 * false otherwise.
 */
exports.isContentStylesheet = function (sheet) {
  return sheet.parsingMode !== "agent";
};

/**
 * Return a shortened version of a style sheet's source.
 *
 * @param {CSSStyleSheet} sheet the DOM object for the style sheet.
 */
exports.shortSource = function (sheet) {
  // Use a string like "inline" if there is no source href
  if (!sheet || !sheet.href) {
    return exports.l10n("rule.sourceInline");
  }

  // We try, in turn, the filename, filePath, query string, whole thing
  let url = {};
  try {
    url = new URL(sheet.href);
  } catch (ex) {
    // Some UA-provided stylesheets are not valid URLs.
  }

  if (url.pathname) {
    let index = url.pathname.lastIndexOf("/");
    if (index !== -1 && index < url.pathname.length) {
      return url.pathname.slice(index + 1);
    }
    return url.pathname;
  }

  if (url.query) {
    return url.query;
  }

  let dataUrl = sheet.href.match(/^(data:[^,]*),/);
  return dataUrl ? dataUrl[1] : sheet.href;
};

const TAB_CHARS = "\t";
const SPACE_CHARS = " ";

/**
 * Prettify minified CSS text.
 * This prettifies CSS code where there is no indentation in usual places while
 * keeping original indentation as-is elsewhere.
 * @param string text The CSS source to prettify.
 * @return string Prettified CSS source
 */
function prettifyCSS(text, ruleCount) {
  if (prettifyCSS.LINE_SEPARATOR == null) {
    let os = Services.appinfo.OS;
    prettifyCSS.LINE_SEPARATOR = (os === "WINNT" ? "\r\n" : "\n");
  }

  // remove initial and terminating HTML comments and surrounding whitespace
  text = text.replace(/(?:^\s*<!--[\r\n]*)|(?:\s*-->\s*$)/g, "");
  let originalText = text;
  text = text.trim();

  // don't attempt to prettify if there's more than one line per rule.
  let lineCount = text.split("\n").length - 1;
  if (ruleCount !== null && lineCount >= ruleCount) {
    return originalText;
  }

  // We reformat the text using a simple state machine.  The
  // reformatting preserves most of the input text, changing only
  // whitespace.  The rules are:
  //
  // * After a "{" or ";" symbol, ensure there is a newline and
  //   indentation before the next non-comment, non-whitespace token.
  // * Additionally after a "{" symbol, increase the indentation.
  // * A "}" symbol ensures there is a preceding newline, and
  //   decreases the indentation level.
  // * Ensure there is whitespace before a "{".
  //
  // This approach can be confused sometimes, but should do ok on a
  // minified file.
  let indent = "";
  let indentLevel = 0;
  let tokens = CSSLexer.getCSSLexer(text);
  let result = "";
  let pushbackToken = undefined;

  // A helper function that reads tokens, looking for the next
  // non-comment, non-whitespace token.  Comment and whitespace tokens
  // are appended to |result|.  If this encounters EOF, it returns
  // null.  Otherwise it returns the last whitespace token that was
  // seen.  This function also updates |pushbackToken|.
  let readUntilSignificantToken = () => {
    while (true) {
      let token = tokens.nextToken();
      if (!token || token.tokenType !== "whitespace") {
        pushbackToken = token;
        return token;
      }
      // Saw whitespace.  Before committing to it, check the next
      // token.
      let nextToken = tokens.nextToken();
      if (!nextToken || nextToken.tokenType !== "comment") {
        pushbackToken = nextToken;
        return token;
      }
      // Saw whitespace + comment.  Update the result and continue.
      result = result + text.substring(token.startOffset, nextToken.endOffset);
    }
  };

  // State variables for readUntilNewlineNeeded.
  //
  // Starting index of the accumulated tokens.
  let startIndex;
  // Ending index of the accumulated tokens.
  let endIndex;
  // True if any non-whitespace token was seen.
  let anyNonWS;
  // True if the terminating token is "}".
  let isCloseBrace;
  // True if the token just before the terminating token was
  // whitespace.
  let lastWasWS;

  // A helper function that reads tokens until there is a reason to
  // insert a newline.  This updates the state variables as needed.
  // If this encounters EOF, it returns null.  Otherwise it returns
  // the final token read.  Note that if the returned token is "{",
  // then it will not be included in the computed start/end token
  // range.  This is used to handle whitespace insertion before a "{".
  let readUntilNewlineNeeded = () => {
    let token;
    while (true) {
      if (pushbackToken) {
        token = pushbackToken;
        pushbackToken = undefined;
      } else {
        token = tokens.nextToken();
      }
      if (!token) {
        endIndex = text.length;
        break;
      }

      // A "}" symbol must be inserted later, to deal with indentation
      // and newline.
      if (token.tokenType === "symbol" && token.text === "}") {
        isCloseBrace = true;
        break;
      } else if (token.tokenType === "symbol" && token.text === "{") {
        break;
      }

      if (token.tokenType !== "whitespace") {
        anyNonWS = true;
      }

      if (startIndex === undefined) {
        startIndex = token.startOffset;
      }
      endIndex = token.endOffset;

      if (token.tokenType === "symbol" && token.text === ";") {
        break;
      }

      lastWasWS = token.tokenType === "whitespace";
    }
    return token;
  };

  while (true) {
    // Set the initial state.
    startIndex = undefined;
    endIndex = undefined;
    anyNonWS = false;
    isCloseBrace = false;
    lastWasWS = false;

    // Read tokens until we see a reason to insert a newline.
    let token = readUntilNewlineNeeded();

    // Append any saved up text to the result, applying indentation.
    if (startIndex !== undefined) {
      if (isCloseBrace && !anyNonWS) {
        // If we saw only whitespace followed by a "}", then we don't
        // need anything here.
      } else {
        result = result + indent + text.substring(startIndex, endIndex);
        if (isCloseBrace) {
          result += prettifyCSS.LINE_SEPARATOR;
        }
      }
    }

    // Get preference of the user regarding what to use for indentation,
    // spaces or tabs.
    let tabPrefs = getTabPrefs();

    if (isCloseBrace) {
      // Even if the stylesheet contains extra closing braces, the indent level should
      // remain > 0.
      indentLevel = Math.max(0, indentLevel - 1);

      if (tabPrefs.indentWithTabs) {
        indent = TAB_CHARS.repeat(indentLevel);
      } else {
        indent = SPACE_CHARS.repeat(indentLevel);
      }
      result = result + indent + "}";
    }

    if (!token) {
      break;
    }

    if (token.tokenType === "symbol" && token.text === "{") {
      if (!lastWasWS) {
        result += " ";
      }
      result += "{";
      if (tabPrefs.indentWithTabs) {
        indent = TAB_CHARS.repeat(++indentLevel);
      } else {
        indent = SPACE_CHARS.repeat(++indentLevel);
      }
    }

    // Now it is time to insert a newline.  However first we want to
    // deal with any trailing comments.
    token = readUntilSignificantToken();

    // "Early" bail-out if the text does not appear to be minified.
    // Here we ignore the case where whitespace appears at the end of
    // the text.
    if (pushbackToken && token && token.tokenType === "whitespace" &&
        /\n/g.test(text.substring(token.startOffset, token.endOffset))) {
      return originalText;
    }

    // Finally time for that newline.
    result = result + prettifyCSS.LINE_SEPARATOR;

    // Maybe we hit EOF.
    if (!pushbackToken) {
      break;
    }
  }

  return result;
}

exports.prettifyCSS = prettifyCSS;

/**
 * Find a unique CSS selector for a given element
 * @returns a string such that ele.ownerDocument.querySelector(reply) === ele
 * and ele.ownerDocument.querySelectorAll(reply).length === 1
 */
exports.findCssSelector = findCssSelector;

/**
 * Get the full CSS path for a given element.
 * @returns a string that can be used as a CSS selector for the element. It might not
 * match the element uniquely. It does however, represent the full path from the root
 * node to the element.
 */
function getCssPath(ele) {
  ele = getRootBindingParent(ele);
  const document = ele.ownerDocument;
  if (!document || !document.contains(ele)) {
    throw new Error("getCssPath received element not inside document");
  }

  const getElementSelector = element => {
    if (!element.localName) {
      return "";
    }

    let label = element.nodeName == element.nodeName.toUpperCase()
                ? element.localName.toLowerCase()
                : element.localName;

    if (element.id) {
      label += "#" + element.id;
    }

    if (element.classList) {
      for (let cl of element.classList) {
        label += "." + cl;
      }
    }

    return label;
  };

  let paths = [];

  while (ele) {
    if (!ele || ele.nodeType !== Node.ELEMENT_NODE) {
      break;
    }

    paths.splice(0, 0, getElementSelector(ele));
    ele = ele.parentNode;
  }

  return paths.length ? paths.join(" ") : "";
}
exports.getCssPath = getCssPath;

/**
 * Get the xpath for a given element.
 * @param {DomNode} ele
 * @returns a string that can be used as an XPath to find the element uniquely.
 */
function getXPath(ele) {
  ele = getRootBindingParent(ele);
  const document = ele.ownerDocument;
  if (!document || !document.contains(ele)) {
    throw new Error("getXPath received element not inside document");
  }

  // Create a short XPath for elements with IDs.
  if (ele.id) {
    return `//*[@id="${ele.id}"]`;
  }

  // Otherwise walk the DOM up and create a part for each ancestor.
  const parts = [];

  // Use nodeName (instead of localName) so namespace prefix is included (if any).
  while (ele && ele.nodeType === Node.ELEMENT_NODE) {
    let nbOfPreviousSiblings = 0;
    let hasNextSiblings = false;

    // Count how many previous same-name siblings the element has.
    let sibling = ele.previousSibling;
    while (sibling) {
      // Ignore document type declaration.
      if (sibling.nodeType !== Node.DOCUMENT_TYPE_NODE &&
          sibling.nodeName == ele.nodeName) {
        nbOfPreviousSiblings++;
      }

      sibling = sibling.previousSibling;
    }

    // Check if the element has at least 1 next same-name sibling.
    sibling = ele.nextSibling;
    while (sibling) {
      if (sibling.nodeName == ele.nodeName) {
        hasNextSiblings = true;
        break;
      }
      sibling = sibling.nextSibling;
    }

    const prefix = ele.prefix ? ele.prefix + ":" : "";
    const nth = nbOfPreviousSiblings || hasNextSiblings
                ? `[${nbOfPreviousSiblings + 1}]` : "";

    parts.push(prefix + ele.localName + nth);

    ele = ele.parentNode;
  }

  return parts.length ? "/" + parts.reverse().join("/") : "";
}
exports.getXPath = getXPath;
