/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This file contains functions to retrieve docs content from
 * MDN (developer.mozilla.org) for particular items, and to display
 * the content in a tooltip.
 *
 * At the moment it only supports fetching content for CSS properties,
 * but it might support other types of content in the future
 * (Web APIs, for example).
 *
 * It's split into two parts:
 *
 * - functions like getCssDocs that just fetch content from MDN,
 * without any constraints on what to do with the content. If you
 * want to embed the content in some custom way, use this.
 *
 * - the MdnDocsWidget class, that manages and updates a tooltip
 * document whose content is taken from MDN. If you want to embed
 * the content in a tooltip, use this in conjunction with Tooltip.js.
 */

"use strict";

const Services = require("Services");
const defer = require("devtools/shared/defer");
const {getCSSLexer} = require("devtools/shared/css-lexer");
const EventEmitter = require("devtools/shared/event-emitter");
const {gDevTools} = require("devtools/client/framework/devtools");

const XHTML_NS = "http://www.w3.org/1999/xhtml";

// Parameters for the XHR request
// see https://developer.mozilla.org/en-US/docs/MDN/Kuma/API#Document_parameters
const XHR_PARAMS = "?raw&macros";
// URL for the XHR request
var XHR_CSS_URL = "https://developer.mozilla.org/en-US/docs/Web/CSS/";

// Parameters for the link to MDN in the tooltip, so
// so we know which MDN visits come from this feature
const PAGE_LINK_PARAMS =
  "?utm_source=mozilla&utm_medium=firefox-inspector&utm_campaign=default";
// URL for the page link omits locale, so a locale-specific page will be loaded
var PAGE_LINK_URL = "https://developer.mozilla.org/docs/Web/CSS/";
exports.PAGE_LINK_URL = PAGE_LINK_URL;

const PROPERTY_NAME_COLOR = "theme-fg-color5";
const PROPERTY_VALUE_COLOR = "theme-fg-color1";
const COMMENT_COLOR = "theme-comment";

/**
 * Turns a string containing a series of CSS declarations into
 * a series of DOM nodes, with classes applied to provide syntax
 * highlighting.
 *
 * It uses the CSS tokenizer to generate a stream of CSS tokens.
 * https://mxr.mozilla.org/mozilla-central/source/dom/webidl/CSSLexer.webidl
 * lists all the token types.
 *
 * - "whitespace", "comment", and "symbol" tokens are appended as TEXT nodes,
 * and will inherit the default style for text.
 *
 * - "ident" tokens that we think are property names are considered to be
 * a property name, and are appended as SPAN nodes with a distinct color class.
 *
 * - "ident" nodes which we do not think are property names, and nodes
 * of all other types ("number", "url", "percentage", ...) are considered
 * to be part of a property value, and are appended as SPAN nodes with
 * a different color class.
 *
 * @param {Document} doc
 * Used to create nodes.
 *
 * @param {String} syntaxText
 * The CSS input. This is assumed to consist of a series of
 * CSS declarations, with trailing semicolons.
 *
 * @param {DOM node} syntaxSection
 * This is the parent for the output nodes. Generated nodes
 * are appended to this as children.
 */
function appendSyntaxHighlightedCSS(cssText, parentElement) {
  let doc = parentElement.ownerDocument;
  let identClass = PROPERTY_NAME_COLOR;
  let lexer = getCSSLexer(cssText);

  /**
   * Create a SPAN node with the given text content and class.
   */
  function createStyledNode(textContent, className) {
    let newNode = doc.createElementNS(XHTML_NS, "span");
    newNode.classList.add(className);
    newNode.textContent = textContent;
    return newNode;
  }

  /**
   * If the symbol is ":", we will expect the next
   * "ident" token to be part of a property value.
   *
   * If the symbol is ";", we will expect the next
   * "ident" token to be a property name.
   */
  function updateIdentClass(tokenText) {
    if (tokenText === ":") {
      identClass = PROPERTY_VALUE_COLOR;
    } else if (tokenText === ";") {
      identClass = PROPERTY_NAME_COLOR;
    }
  }

  /**
   * Create the appropriate node for this token type.
   *
   * If this token is a symbol, also update our expectations
   * for what the next "ident" token represents.
   */
  function tokenToNode(token, tokenText) {
    switch (token.tokenType) {
      case "ident":
        return createStyledNode(tokenText, identClass);
      case "symbol":
        updateIdentClass(tokenText);
        return doc.createTextNode(tokenText);
      case "whitespace":
        return doc.createTextNode(tokenText);
      case "comment":
        return createStyledNode(tokenText, COMMENT_COLOR);
      default:
        return createStyledNode(tokenText, PROPERTY_VALUE_COLOR);
    }
  }

  let token = lexer.nextToken();
  while (token) {
    let tokenText = cssText.slice(token.startOffset, token.endOffset);
    let newNode = tokenToNode(token, tokenText);
    parentElement.appendChild(newNode);
    token = lexer.nextToken();
  }
}

exports.appendSyntaxHighlightedCSS = appendSyntaxHighlightedCSS;

/**
 * Fetch an MDN page.
 *
 * @param {string} pageUrl
 * URL of the page to fetch.
 *
 * @return {promise}
 * The promise is resolved with the page as an XML document.
 *
 * The promise is rejected with an error message if
 * we could not load the page.
 */
function getMdnPage(pageUrl) {
  let deferred = defer();

  let xhr = new XMLHttpRequest();

  xhr.addEventListener("load", onLoaded, false);
  xhr.addEventListener("error", onError, false);

  xhr.open("GET", pageUrl);
  xhr.responseType = "document";
  xhr.send();

  function onLoaded(e) {
    if (xhr.status != 200) {
      deferred.reject({page: pageUrl, status: xhr.status});
    } else {
      deferred.resolve(xhr.responseXML);
    }
  }

  function onError(e) {
    deferred.reject({page: pageUrl, status: xhr.status});
  }

  return deferred.promise;
}

/**
 * Gets some docs for the given CSS property.
 * Loads an MDN page for the property and gets some
 * information about the property.
 *
 * @param {string} cssProperty
 * The property for which we want docs.
 *
 * @return {promise}
 * The promise is resolved with an object containing:
 * - summary: a short summary of the property
 * - syntax: some example syntax
 *
 * The promise is rejected with an error message if
 * we could not load the page.
 */
function getCssDocs(cssProperty) {
  let deferred = defer();
  let pageUrl = XHR_CSS_URL + cssProperty + XHR_PARAMS;

  getMdnPage(pageUrl).then(parseDocsFromResponse, handleRejection);

  function parseDocsFromResponse(responseDocument) {
    let theDocs = {};
    theDocs.summary = getSummary(responseDocument);
    theDocs.syntax = getSyntax(responseDocument);
    if (theDocs.summary || theDocs.syntax) {
      deferred.resolve(theDocs);
    } else {
      deferred.reject("Couldn't find the docs in the page.");
    }
  }

  function handleRejection(e) {
    deferred.reject(e.status);
  }

  return deferred.promise;
}

exports.getCssDocs = getCssDocs;

/**
 * The MdnDocsWidget is used by tooltip code that needs to display docs
 * from MDN in a tooltip.
 *
 * In the constructor, the widget does some general setup that's not
 * dependent on the particular item we need docs for.
 *
 * After that, when the tooltip code needs to display docs for an item, it
 * asks the widget to retrieve the docs and update the document with them.
 *
 * @param {Element} tooltipContainer
 * A DOM element where the MdnDocs widget markup should be created.
 */
function MdnDocsWidget(tooltipContainer) {
  EventEmitter.decorate(this);

  tooltipContainer.innerHTML =
    `<header>
       <h1 class="mdn-property-name theme-fg-color5"></h1>
     </header>
     <div class="mdn-property-info">
       <div class="mdn-summary"></div>
       <pre class="mdn-syntax devtools-monospace"></pre>
     </div>
     <footer>
       <a class="mdn-visit-page theme-link" href="#">Visit MDN (placeholder)</a>
     </footer>`;

  // fetch all the bits of the document that we will manipulate later
  this.elements = {
    heading: tooltipContainer.querySelector(".mdn-property-name"),
    summary: tooltipContainer.querySelector(".mdn-summary"),
    syntax: tooltipContainer.querySelector(".mdn-syntax"),
    info: tooltipContainer.querySelector(".mdn-property-info"),
    linkToMdn: tooltipContainer.querySelector(".mdn-visit-page")
  };

  // get the localized string for the link text
  this.elements.linkToMdn.textContent =
    l10n.strings.GetStringFromName("docsTooltip.visitMDN");

  // listen for clicks and open in the browser window instead
  let mainWindow = Services.wm.getMostRecentWindow(gDevTools.chromeWindowType);
  this.elements.linkToMdn.addEventListener("click", (e) => {
    e.stopPropagation();
    e.preventDefault();
    mainWindow.openUILinkIn(e.target.href, "tab");
    this.emit("visitlink");
  });
}

exports.MdnDocsWidget = MdnDocsWidget;

MdnDocsWidget.prototype = {
  /**
   * This is called just before the tooltip is displayed, and is
   * passed the CSS property for which we want to display help.
   *
   * Its job is to make sure the document contains the docs
   * content for that CSS property.
   *
   * First, it initializes the document, setting the things it can
   * set synchronously, resetting the things it needs to get
   * asynchronously, and making sure the throbber is throbbing.
   *
   * Then it tries to get the content asynchronously, updating
   * the document with the content or with an error message.
   *
   * It returns immediately, so the caller can display the tooltip
   * without waiting for the asynch operation to complete.
   *
   * @param {string} propertyName
   * The name of the CSS property for which we need to display help.
   */
  loadCssDocs: function (propertyName) {
    /**
     * Do all the setup we can do synchronously, and get the document in
     * a state where it can be displayed while we are waiting for the
     * MDN docs content to be retrieved.
     */
    function initializeDocument(propName) {
      // set property name heading
      elements.heading.textContent = propName;

      // set link target
      elements.linkToMdn.setAttribute("href",
        PAGE_LINK_URL + propName + PAGE_LINK_PARAMS);

      // clear docs summary and syntax
      elements.summary.textContent = "";
      while (elements.syntax.firstChild) {
        elements.syntax.firstChild.remove();
      }

      // reset the scroll position
      elements.info.scrollTop = 0;
      elements.info.scrollLeft = 0;

      // show the throbber
      elements.info.classList.add("devtools-throbber");
    }

    /**
     * This is called if we successfully got the docs content.
     * Finishes setting up the tooltip content, and disables the throbber.
     */
    function finalizeDocument({summary, syntax}) {
      // set docs summary and syntax
      elements.summary.textContent = summary;
      appendSyntaxHighlightedCSS(syntax, elements.syntax);

      // hide the throbber
      elements.info.classList.remove("devtools-throbber");

      deferred.resolve(this);
    }

    /**
     * This is called if we failed to get the docs content.
     * Sets the content to contain an error message, and disables the throbber.
     */
    function gotError(error) {
      // show error message
      elements.summary.textContent =
        l10n.strings.GetStringFromName("docsTooltip.loadDocsError");

      // hide the throbber
      elements.info.classList.remove("devtools-throbber");

      // although gotError is called when there's an error, we have handled
      // the error, so call resolve not reject.
      deferred.resolve(this);
    }

    let deferred = defer();
    let elements = this.elements;

    initializeDocument(propertyName);
    getCssDocs(propertyName).then(finalizeDocument, gotError);

    return deferred.promise;
  },

  destroy: function () {
    this.elements = null;
  }
};

/**
 * L10N utility class
 */
function L10N() {}
L10N.prototype = {};

var l10n = new L10N();

loader.lazyGetter(L10N.prototype, "strings", () => {
  return Services.strings.createBundle(
    "chrome://devtools/locale/inspector.properties");
});

/**
 * Test whether a node is all whitespace.
 *
 * @return {boolean}
 * True if the node all whitespace, otherwise false.
 */
function isAllWhitespace(node) {
  return !(/[^\t\n\r ]/.test(node.textContent));
}

/**
 * Test whether a node is a comment or whitespace node.
 *
 * @return {boolean}
 * True if the node is a comment node or is all whitespace, otherwise false.
 */
function isIgnorable(node) {
  // Comment nodes (8), text nodes (3) or whitespace
  return (node.nodeType == 8) ||
         ((node.nodeType == 3) && isAllWhitespace(node));
}

/**
 * Get the next node, skipping comments and whitespace.
 *
 * @return {node}
 * The next sibling node that is not a comment or whitespace, or null if
 * there isn't one.
 */
function nodeAfter(sib) {
  while ((sib = sib.nextSibling)) {
    if (!isIgnorable(sib)) {
      return sib;
    }
  }
  return null;
}

/**
 * Test whether the argument `node` is a node whose tag is `tagName`.
 *
 * @param {node} node
 * The code to test. May be null.
 *
 * @param {string} tagName
 * The tag name to test against.
 *
 * @return {boolean}
 * True if the node is not null and has the tag name `tagName`,
 * otherwise false.
 */
function hasTagName(node, tagName) {
  return node && node.tagName &&
         node.tagName.toLowerCase() == tagName.toLowerCase();
}

/**
 * Given an MDN page, get the "summary" portion.
 *
 * This is the textContent of the first non-whitespace
 * element in the #Summary section of the document.
 *
 * It's expected to be a <P> element.
 *
 * @param {Document} mdnDocument
 * The document in which to look for the "summary" section.
 *
 * @return {string}
 * The summary section as a string, or null if it could not be found.
 */
function getSummary(mdnDocument) {
  let summary = mdnDocument.getElementById("Summary");
  if (!hasTagName(summary, "H2")) {
    return null;
  }

  let firstParagraph = nodeAfter(summary);
  if (!hasTagName(firstParagraph, "P")) {
    return null;
  }

  return firstParagraph.textContent;
}

/**
 * Given an MDN page, get the "syntax" portion.
 *
 * First we get the #Syntax section of the document. The syntax
 * section we want is somewhere inside there.
 *
 * If the page is in the old structure, then the *first two*
 * non-whitespace elements in the #Syntax section will be <PRE>
 * nodes, and the second of these will be the syntax section.
 *
 * If the page is in the new structure, then the only the *first*
 * non-whitespace element in the #Syntax section will be a <PRE>
 * node, and it will be the syntax section.
 *
 * @param {Document} mdnDocument
 * The document in which to look for the "syntax" section.
 *
 * @return {string}
 * The syntax section as a string, or null if it could not be found.
 */
function getSyntax(mdnDocument) {
  let syntax = mdnDocument.getElementById("Syntax");
  if (!hasTagName(syntax, "H2")) {
    return null;
  }

  let firstParagraph = nodeAfter(syntax);
  if (!hasTagName(firstParagraph, "PRE")) {
    return null;
  }

  let secondParagraph = nodeAfter(firstParagraph);
  if (hasTagName(secondParagraph, "PRE")) {
    return secondParagraph.textContent;
  }
  return firstParagraph.textContent;
}

/**
 * Use a different URL for CSS docs pages. Used only for testing.
 *
 * @param {string} baseUrl
 * The baseURL to use.
 */
function setBaseCssDocsUrl(baseUrl) {
  PAGE_LINK_URL = baseUrl;
  XHR_CSS_URL = baseUrl;
}

exports.setBaseCssDocsUrl = setBaseCssDocsUrl;
