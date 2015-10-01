/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Cc, Ci, Cu} = require("chrome");
const {colorUtils} = require("devtools/shared/css-color");
const {Services} = Cu.import("resource://gre/modules/Services.jsm", {});

const HTML_NS = "http://www.w3.org/1999/xhtml";

const MAX_ITERATIONS = 100;

const BEZIER_KEYWORDS = ["linear", "ease-in-out", "ease-in", "ease-out",
                         "ease"];

// Functions that accept a color argument.
const COLOR_TAKING_FUNCTIONS = ["linear-gradient",
                                "-moz-linear-gradient",
                                "repeating-linear-gradient",
                                "-moz-repeating-linear-gradient",
                                "radial-gradient",
                                "-moz-radial-gradient",
                                "repeating-radial-gradient",
                                "-moz-repeating-radial-gradient",
                                "drop-shadow"];

loader.lazyGetter(this, "DOMUtils", function() {
  return Cc["@mozilla.org/inspector/dom-utils;1"].getService(Ci.inIDOMUtils);
});

/**
 * This module is used to process text for output by developer tools. This means
 * linking JS files with the debugger, CSS files with the style editor, JS
 * functions with the debugger, placing color swatches next to colors and
 * adding doorhanger previews where possible (images, angles, lengths,
 * border radius, cubic-bezier etc.).
 *
 * Usage:
 *   const {require} =
 *      Cu.import("resource://gre/modules/devtools/shared/Loader.jsm", {});
 *   const {OutputParser} = require("devtools/shared/output-parser");
 *
 *   let parser = new OutputParser(document);
 *
 *   parser.parseCssProperty("color", "red"); // Returns document fragment.
 */
function OutputParser(document) {
  this.parsed = [];
  this.doc = document;
  this.colorSwatches = new WeakMap();
  this._onSwatchMouseDown = this._onSwatchMouseDown.bind(this);
}

exports.OutputParser = OutputParser;

OutputParser.prototype = {
  /**
   * Parse a CSS property value given a property name.
   *
   * @param  {String} name
   *         CSS Property Name
   * @param  {String} value
   *         CSS Property value
   * @param  {Object} [options]
   *         Options object. For valid options and default values see
   *         _mergeOptions().
   * @return {DocumentFragment}
   *         A document fragment containing color swatches etc.
   */
  parseCssProperty: function(name, value, options={}) {
    options = this._mergeOptions(options);

    options.expectCubicBezier =
      safeCssPropertySupportsType(name, DOMUtils.TYPE_TIMING_FUNCTION);
    options.expectFilter = name === "filter";
    options.supportsColor =
      safeCssPropertySupportsType(name, DOMUtils.TYPE_COLOR) ||
      safeCssPropertySupportsType(name, DOMUtils.TYPE_GRADIENT);

    if (this._cssPropertySupportsValue(name, value)) {
      return this._parse(value, options);
    }
    this._appendTextNode(value);

    return this._toDOM();
  },

  /**
   * Given an initial FUNCTION token, read tokens from |tokenStream|
   * and collect all the (non-comment) text.  Return the collected
   * text.  The function token and the close paren are included in the
   * result.
   *
   * @param  {CSSToken} initialToken
   *         The FUNCTION token.
   * @param  {String} text
   *         The original CSS text.
   * @param  {CSSLexer} tokenStream
   *         The token stream from which to read.
   * @return {String}
   *         The text of body of the function call.
   */
  _collectFunctionText: function(initialToken, text, tokenStream) {
    let result = text.substring(initialToken.startOffset,
                                initialToken.endOffset);
    let depth = 1;
    while (depth > 0) {
      let token = tokenStream.nextToken();
      if (!token) {
        break;
      }
      if (token.tokenType === "comment") {
        continue;
      }
      result += text.substring(token.startOffset, token.endOffset);
      if (token.tokenType === "symbol") {
        if (token.text === "(") {
          ++depth;
        } else if (token.text === ")") {
          --depth;
        }
      } else if (token.tokenType === "function") {
        ++depth;
      }
    }
    return result;
  },

  /**
   * Parse a string.
   *
   * @param  {String} text
   *         Text to parse.
   * @param  {Object} [options]
   *         Options object. For valid options and default values see
   *         _mergeOptions().
   * @return {DocumentFragment}
   *         A document fragment.
   */
  _parse: function(text, options={}) {
    text = text.trim();
    this.parsed.length = 0;

    let tokenStream = DOMUtils.getCSSLexer(text);
    let i = 0;
    let parenDepth = 0;
    let outerMostFunctionTakesColor = false;

    let colorOK = function() {
      return options.supportsColor ||
        (options.expectFilter && parenDepth === 1 &&
         outerMostFunctionTakesColor);
    };

    while (true) {
      let token = tokenStream.nextToken();
      if (!token) {
        break;
      }
      if (token.tokenType === "comment") {
        continue;
      }

      // Prevent this loop from slowing down the browser with too
      // many nodes being appended into output. In practice it is very unlikely
      // that this will ever happen.
      i++;
      if (i > MAX_ITERATIONS) {
        this._appendTextNode(text.substring(token.startOffset,
                                            token.endOffset));
        continue;
      }

      switch (token.tokenType) {
        case "function": {
          if (COLOR_TAKING_FUNCTIONS.indexOf(token.text) >= 0) {
            // The function can accept a color argument, and we know
            // it isn't special in some other way.  So, we let it
            // through to the ordinary parsing loop so that colors
            // can be handled in a single place.
            this._appendTextNode(text.substring(token.startOffset,
                                                token.endOffset));
            if (parenDepth === 0) {
              outerMostFunctionTakesColor = true;
            }
            ++parenDepth;
          } else {
            let functionText = this._collectFunctionText(token, text,
                                                         tokenStream);

            if (options.expectCubicBezier && token.text === "cubic-bezier") {
              this._appendCubicBezier(functionText, options);
            } else if (colorOK() && DOMUtils.isValidCSSColor(functionText)) {
              this._appendColor(functionText, options);
            } else {
              this._appendTextNode(functionText);
            }
          }
          break;
        }

        case "ident":
          if (options.expectCubicBezier &&
              BEZIER_KEYWORDS.indexOf(token.text) >= 0) {
            this._appendCubicBezier(token.text, options);
          } else if (colorOK() && DOMUtils.isValidCSSColor(token.text)) {
            this._appendColor(token.text, options);
          } else {
            this._appendTextNode(text.substring(token.startOffset,
                                                token.endOffset));
          }
          break;

        case "id":
        case "hash": {
          let original = text.substring(token.startOffset, token.endOffset);
          if (colorOK() && DOMUtils.isValidCSSColor(original)) {
            this._appendColor(original, options);
          } else {
            this._appendTextNode(original);
          }
          break;
        }

        case "url":
        case "bad_url":
          this._appendURL(text.substring(token.startOffset, token.endOffset),
                          token.text, options);
          break;

        case "symbol":
          if (token.text === "(") {
            ++parenDepth;
          } else if (token.token === ")") {
            --parenDepth;
          }
          // falls through
        default:
          this._appendTextNode(text.substring(token.startOffset,
                                              token.endOffset));
          break;
      }
    }

    let result = this._toDOM();

    if (options.expectFilter && !options.filterSwatch) {
      result = this._wrapFilter(text, options, result);
    }

    return result;
  },

  /**
   * Append a cubic-bezier timing function value to the output
   *
   * @param {String} bezier
   *        The cubic-bezier timing function
   * @param {Object} options
   *        Options object. For valid options and default values see
   *        _mergeOptions()
   */
  _appendCubicBezier: function(bezier, options) {
    let container = this._createNode("span", {
       "data-bezier": bezier
    });

    if (options.bezierSwatchClass) {
      let swatch = this._createNode("span", {
        class: options.bezierSwatchClass
      });
      container.appendChild(swatch);
    }

    let value = this._createNode("span", {
      class: options.bezierClass
    }, bezier);

    container.appendChild(value);
    this.parsed.push(container);
  },

  /**
   * Check if a CSS property supports a specific value.
   *
   * @param  {String} name
   *         CSS Property name to check
   * @param  {String} value
   *         CSS Property value to check
   */
  _cssPropertySupportsValue: function(name, value) {
    return DOMUtils.cssPropertyIsValid(name, value);
  },

  /**
   * Tests if a given colorObject output by CssColor is valid for parsing.
   * Valid means it's really a color, not any of the CssColor SPECIAL_VALUES
   * except transparent
   */
  _isValidColor: function(colorObj) {
    return colorObj.valid &&
      (!colorObj.specialValue || colorObj.specialValue === "transparent");
  },

  /**
   * Append a color to the output.
   *
   * @param  {String} color
   *         Color to append
   * @param  {Object} [options]
   *         Options object. For valid options and default values see
   *         _mergeOptions().
   */
  _appendColor: function(color, options={}) {
    let colorObj = new colorUtils.CssColor(color);

    if (this._isValidColor(colorObj)) {
      let container = this._createNode("span", {
         "data-color": color
      });

      if (options.colorSwatchClass) {
        let swatch = this._createNode("span", {
          class: options.colorSwatchClass,
          style: "background-color:" + color
        });
        this.colorSwatches.set(swatch, colorObj);
        swatch.addEventListener("mousedown", this._onSwatchMouseDown, false);
        container.appendChild(swatch);
      }

      if (options.defaultColorType) {
        color = colorObj.toString();
        container.dataset.color = color;
      }

      let value = this._createNode("span", {
        class: options.colorClass
      }, color);

      container.appendChild(value);
      this.parsed.push(container);
    } else {
      this._appendTextNode(color);
    }
  },

  /**
   * Wrap some existing nodes in a filter editor.
   *
   * @param {String} filters
   *        The full text of the "filter" property.
   * @param {object} options
   *        The options object passed to parseCssProperty().
   * @param {object} nodes
   *        Nodes created by _toDOM().
   *
   * @returns {object}
   *        A new node that supplies a filter swatch and that wraps |nodes|.
   */
  _wrapFilter: function(filters, options, nodes) {
    let container = this._createNode("span", {
      "data-filters": filters
    });

    if (options.filterSwatchClass) {
      let swatch = this._createNode("span", {
        class: options.filterSwatchClass
      });
      container.appendChild(swatch);
    }

    let value = this._createNode("span", {
      class: options.filterClass
    });
    value.appendChild(nodes);
    container.appendChild(value);

    return container;
  },

  _onSwatchMouseDown: function(event) {
    // Prevent text selection in the case of shift-click or double-click.
    event.preventDefault();

    if (!event.shiftKey) {
      return;
    }

    let swatch = event.target;
    let color = this.colorSwatches.get(swatch);
    let val = color.nextColorUnit();

    swatch.nextElementSibling.textContent = val;
  },

  /**
   * A helper function that sanitizes a possibly-unterminated URL.
   */
  _sanitizeURL: function(url) {
    // Re-lex the URL and add any needed termination characters.
    let urlTokenizer = DOMUtils.getCSSLexer(url);
    // Just read until EOF; there will only be a single token.
    while (urlTokenizer.nextToken()) {
      // Nothing.
    }

    return urlTokenizer.performEOFFixup(url, true);
  },

  /**
   * Append a URL to the output.
   *
   * @param  {String} match
   *         Complete match that may include "url(xxx)"
   * @param  {String} url
   *         Actual URL
   * @param  {Object} [options]
   *         Options object. For valid options and default values see
   *         _mergeOptions().
   */
  _appendURL: function(match, url, options) {
    if (options.urlClass) {
      // Sanitize the URL.  Note that if we modify the URL, we just
      // leave the termination characters.  This isn't strictly
      // "as-authored", but it makes a bit more sense.
      match = this._sanitizeURL(match);
      // This regexp matches a URL token.  It puts the "url(", any
      // leading whitespace, and any opening quote into |leader|; the
      // URL text itself into |body|, and any trailing quote, trailing
      // whitespace, and the ")" into |trailer|.  We considered adding
      // functionality for this to CSSLexer, in some way, but this
      // seemed simpler on the whole.
      let [, leader, , body, trailer] =
        /^(url\([ \t\r\n\f]*(["']?))(.*?)(\2[ \t\r\n\f]*\))$/i.exec(match);

      this._appendTextNode(leader);

      let href = url;
      if (options.baseURI) {
        href = options.baseURI.resolve(url);
      }

      this._appendNode("a", {
        target: "_blank",
        class: options.urlClass,
        href: href
      }, body);

      this._appendTextNode(trailer);
    } else {
      this._appendTextNode(match);
    }
  },

  /**
   * Create a node.
   *
   * @param  {String} tagName
   *         Tag type e.g. "div"
   * @param  {Object} attributes
   *         e.g. {class: "someClass", style: "cursor:pointer"};
   * @param  {String} [value]
   *         If a value is included it will be appended as a text node inside
   *         the tag. This is useful e.g. for span tags.
   * @return {Node} Newly created Node.
   */
  _createNode: function(tagName, attributes, value="") {
    let node = this.doc.createElementNS(HTML_NS, tagName);
    let attrs = Object.getOwnPropertyNames(attributes);

    for (let attr of attrs) {
      if (attributes[attr]) {
        node.setAttribute(attr, attributes[attr]);
      }
    }

    if (value) {
      let textNode = this.doc.createTextNode(value);
      node.appendChild(textNode);
    }

    return node;
  },

  /**
   * Append a node to the output.
   *
   * @param  {String} tagName
   *         Tag type e.g. "div"
   * @param  {Object} attributes
   *         e.g. {class: "someClass", style: "cursor:pointer"};
   * @param  {String} [value]
   *         If a value is included it will be appended as a text node inside
   *         the tag. This is useful e.g. for span tags.
   */
  _appendNode: function(tagName, attributes, value="") {
    let node = this._createNode(tagName, attributes, value);
    this.parsed.push(node);
  },

  /**
   * Append a text node to the output. If the previously output item was a text
   * node then we append the text to that node.
   *
   * @param  {String} text
   *         Text to append
   */
  _appendTextNode: function(text) {
    let lastItem = this.parsed[this.parsed.length - 1];
    if (typeof lastItem === "string") {
      this.parsed[this.parsed.length - 1] = lastItem + text;
    } else {
      this.parsed.push(text);
    }
  },

  /**
   * Take all output and append it into a single DocumentFragment.
   *
   * @return {DocumentFragment}
   *         Document Fragment
   */
  _toDOM: function() {
    let frag = this.doc.createDocumentFragment();

    for (let item of this.parsed) {
      if (typeof item === "string") {
        frag.appendChild(this.doc.createTextNode(item));
      } else {
        frag.appendChild(item);
      }
    }

    this.parsed.length = 0;
    return frag;
  },

  /**
   * Merges options objects. Default values are set here.
   *
   * @param  {Object} overrides
   *         The option values to override e.g. _mergeOptions({colors: false})
   *
   *         Valid options are:
   *           - defaultColorType: true // Convert colors to the default type
   *                                    // selected in the options panel.
   *           - colorSwatchClass: ""   // The class to use for color swatches.
   *           - colorClass: ""         // The class to use for the color value
   *                                    // that follows the swatch.
   *           - bezierSwatchClass: ""  // The class to use for bezier swatches.
   *           - bezierClass: ""        // The class to use for the bezier value
   *                                    // that follows the swatch.
   *           - supportsColor: false   // Does the CSS property support colors?
   *           - urlClass: ""           // The class to be used for url() links.
   *           - baseURI: ""            // A string or nsIURI used to resolve
   *                                    // relative links.
   *           - filterSwatch: false    // A special case for parsing a
   *                                    // "filter" property, causing the
   *                                    // parser to skip the call to
   *                                    // _wrapFilter.  Used only for
   *                                    // previewing with the filter swatch.
   * @return {Object}
   *         Overridden options object
   */
  _mergeOptions: function(overrides) {
    let defaults = {
      defaultColorType: true,
      colorSwatchClass: "",
      colorClass: "",
      bezierSwatchClass: "",
      bezierClass: "",
      supportsColor: false,
      urlClass: "",
      baseURI: "",
      filterSwatch: false
    };

    if (typeof overrides.baseURI === "string") {
      overrides.baseURI = Services.io.newURI(overrides.baseURI, null, null);
    }

    for (let item in overrides) {
      defaults[item] = overrides[item];
    }
    return defaults;
  }
};

/**
 * A wrapper for DOMUtils.cssPropertySupportsType that ignores invalid
 * properties.
 *
 * @param {String} name The property name.
 * @param {number} type The type tested for support.
 * @return {Boolean} Whether the property supports the type.
 *        If the property is unknown, false is returned.
 */
function safeCssPropertySupportsType(name, type) {
  try {
    return DOMUtils.cssPropertySupportsType(name, type);
  } catch(e) {
    return false;
  }
}
