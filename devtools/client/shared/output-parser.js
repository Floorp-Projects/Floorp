/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const { angleUtils } = require("devtools/client/shared/css-angle");
const { colorUtils } = require("devtools/shared/css/color");
const { getCSSLexer } = require("devtools/shared/css/lexer");
const EventEmitter = require("devtools/shared/event-emitter");
const { appendText } = require("devtools/client/inspector/shared/utils");

const STYLE_INSPECTOR_PROPERTIES =
  "devtools/shared/locales/styleinspector.properties";
const { LocalizationHelper } = require("devtools/shared/l10n");
const STYLE_INSPECTOR_L10N = new LocalizationHelper(STYLE_INSPECTOR_PROPERTIES);

// Functions that accept an angle argument.
const ANGLE_TAKING_FUNCTIONS = [
  "linear-gradient",
  "-moz-linear-gradient",
  "repeating-linear-gradient",
  "-moz-repeating-linear-gradient",
  "conic-gradient",
  "repeating-conic-gradient",
  "rotate",
  "rotateX",
  "rotateY",
  "rotateZ",
  "rotate3d",
  "skew",
  "skewX",
  "skewY",
  "hue-rotate",
];
// All cubic-bezier CSS timing-function names.
const BEZIER_KEYWORDS = [
  "linear",
  "ease-in-out",
  "ease-in",
  "ease-out",
  "ease",
];
// Functions that accept a color argument.
const COLOR_TAKING_FUNCTIONS = [
  "linear-gradient",
  "-moz-linear-gradient",
  "repeating-linear-gradient",
  "-moz-repeating-linear-gradient",
  "radial-gradient",
  "-moz-radial-gradient",
  "repeating-radial-gradient",
  "-moz-repeating-radial-gradient",
  "conic-gradient",
  "repeating-conic-gradient",
  "drop-shadow",
];
// Functions that accept a shape argument.
const BASIC_SHAPE_FUNCTIONS = ["polygon", "circle", "ellipse", "inset"];

const BACKDROP_FILTER_ENABLED = Services.prefs.getBoolPref(
  "layout.css.backdrop-filter.enabled"
);
const HTML_NS = "http://www.w3.org/1999/xhtml";

/**
 * This module is used to process CSS text declarations and output DOM fragments (to be
 * appended to panels in DevTools) for CSS values decorated with additional UI and
 * functionality.
 *
 * For example:
 * - attaching swatches for values instrumented with specialized tools: colors, timing
 * functions (cubic-bezier), filters, shapes, display values (flex/grid), etc.
 * - adding previews where possible (images, fonts, CSS transforms).
 * - converting between color types on Shift+click on their swatches.
 *
 * Usage:
 *   const OutputParser = require("devtools/client/shared/output-parser");
 *   const parser = new OutputParser(document, cssProperties);
 *   parser.parseCssProperty("color", "red"); // Returns document fragment.
 *
 * @param {Document} document
 *        Used to create DOM nodes.
 * @param {Object} cssProperties
 *        Instance of CssProperties, an object which provides an interface for
 *        working with the database of supported CSS properties and values.
 *        Two items are of interest from this object:
 *        - supportsTypes - A function that returns a boolean when asked if a css
 *          property name supports a given css type.  The function is
 *          executed like supportsType("color", "timing-function")
 *        - supportsCssColor4ColorFunction - A function for checking
 *          the supporting of css-color-4 color function.
 */
function OutputParser(
  document,
  { supportsType, supportsCssColor4ColorFunction }
) {
  this.parsed = [];
  this.doc = document;
  this.supportsType = supportsType;
  this.colorSwatches = new WeakMap();
  this.angleSwatches = new WeakMap();
  this._onColorSwatchMouseDown = this._onColorSwatchMouseDown.bind(this);
  this._onAngleSwatchMouseDown = this._onAngleSwatchMouseDown.bind(this);

  this.cssColor4 = supportsCssColor4ColorFunction();
}

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
  parseCssProperty: function(name, value, options = {}) {
    options = this._mergeOptions(options);

    options.expectCubicBezier = this.supportsType(name, "timing-function");
    options.expectDisplay = name === "display";
    options.expectFilter =
      name === "filter" ||
      (BACKDROP_FILTER_ENABLED && name === "backdrop-filter");
    options.expectShape = name === "clip-path" || name === "shape-outside";
    options.expectFont = name === "font-family";
    options.supportsColor =
      this.supportsType(name, "color") ||
      this.supportsType(name, "gradient") ||
      (name.startsWith("--") && colorUtils.isValidCSSColor(value));

    // The filter property is special in that we want to show the
    // swatch even if the value is invalid, because this way the user
    // can easily use the editor to fix it.
    if (options.expectFilter || this._cssPropertySupportsValue(name, value)) {
      return this._parse(value, options);
    }
    this._appendTextNode(value);

    return this._toDOM();
  },

  /**
   * Read tokens from |tokenStream| and collect all the (non-comment)
   * text. Return the collected texts and variable data (if any).
   * Stop when an unmatched closing paren is seen.
   * If |stopAtComma| is true, then also stop when a top-level
   * (unparenthesized) comma is seen.
   *
   * @param  {String} text
   *         The original source text.
   * @param  {CSSLexer} tokenStream
   *         The token stream from which to read.
   * @param  {Object} options
   *         The options object in use; @see _mergeOptions.
   * @param  {Boolean} stopAtComma
   *         If true, stop at a comma.
   * @return {Object}
   *         An object of the form {tokens, functionData, sawComma, sawVariable}.
   *         |tokens| is a list of the non-comment, non-whitespace tokens
   *         that were seen. The stopping token (paren or comma) will not
   *         be included.
   *         |functionData| is a list of parsed strings and nodes that contain the
   *         data between the matching parenthesis. The stopping token's text will
   *         not be included.
   *         |sawComma| is true if the stop was due to a comma, or false otherwise.
   *         |sawVariable| is true if a variable was seen while parsing the text.
   */
  _parseMatchingParens: function(text, tokenStream, options, stopAtComma) {
    let depth = 1;
    const functionData = [];
    const tokens = [];
    let sawVariable = false;

    while (depth > 0) {
      const token = tokenStream.nextToken();
      if (!token) {
        break;
      }
      if (token.tokenType === "comment") {
        continue;
      }

      if (token.tokenType === "symbol") {
        if (stopAtComma && depth === 1 && token.text === ",") {
          return { tokens, functionData, sawComma: true, sawVariable };
        } else if (token.text === "(") {
          ++depth;
        } else if (token.text === ")") {
          --depth;
          if (depth === 0) {
            break;
          }
        }
      } else if (
        token.tokenType === "function" &&
        token.text === "var" &&
        options.getVariableValue
      ) {
        sawVariable = true;
        const { node } = this._parseVariable(token, text, tokenStream, options);
        functionData.push(node);
      } else if (token.tokenType === "function") {
        ++depth;
      }

      if (
        token.tokenType !== "function" ||
        token.text !== "var" ||
        !options.getVariableValue
      ) {
        functionData.push(text.substring(token.startOffset, token.endOffset));
      }

      if (token.tokenType !== "whitespace") {
        tokens.push(token);
      }
    }

    return { tokens, functionData, sawComma: false, sawVariable };
  },

  /**
   * Parse var() use and return a variable node to be added to the output state.
   * This will read tokens up to and including the ")" that closes the "var("
   * invocation.
   *
   * @param  {CSSToken} initialToken
   *         The "var(" token that was already seen.
   * @param  {String} text
   *         The original input text.
   * @param  {CSSLexer} tokenStream
   *         The token stream from which to read.
   * @param  {Object} options
   *         The options object in use; @see _mergeOptions.
   * @return {Object}
   *         - node: A node for the variable, with the appropriate text and
   *           title. Eg. a span with "var(--var1)" as the textContent
   *           and a title for --var1 like "--var1 = 10" or
   *           "--var1 is not set".
   *         - value: The value for the variable.
   */
  _parseVariable: function(initialToken, text, tokenStream, options) {
    // Handle the "var(".
    const varText = text.substring(
      initialToken.startOffset,
      initialToken.endOffset
    );
    const variableNode = this._createNode("span", {}, varText);

    // Parse the first variable name within the parens of var().
    const {
      tokens,
      functionData,
      sawComma,
      sawVariable,
    } = this._parseMatchingParens(text, tokenStream, options, true);

    const result = sawVariable ? "" : functionData.join("");

    // Display options for the first and second argument in the var().
    const firstOpts = {};
    const secondOpts = {};

    let varValue;

    // Get the variable value if it is in use.
    if (tokens && tokens.length === 1) {
      varValue = options.getVariableValue(tokens[0].text);
    }

    // Get the variable name.
    const varName = text.substring(tokens[0].startOffset, tokens[0].endOffset);

    if (typeof varValue === "string") {
      // The variable value is valid, set the variable name's title of the first argument
      // in var() to display the variable name and value.
      firstOpts["data-variable"] = STYLE_INSPECTOR_L10N.getFormatStr(
        "rule.variableValue",
        varName,
        varValue
      );
      firstOpts.class = options.matchedVariableClass;
      secondOpts.class = options.unmatchedVariableClass;
    } else {
      // The variable name is not valid, mark it unmatched.
      firstOpts.class = options.unmatchedVariableClass;
      firstOpts["data-variable"] = STYLE_INSPECTOR_L10N.getFormatStr(
        "rule.variableUnset",
        varName
      );
    }

    variableNode.appendChild(this._createNode("span", firstOpts, result));

    // If we saw a ",", then append it and show the remainder using
    // the correct highlighting.
    if (sawComma) {
      variableNode.appendChild(this.doc.createTextNode(","));

      // Parse the text up until the close paren, being sure to
      // disable the special case for filter.
      const subOptions = Object.assign({}, options);
      subOptions.expectFilter = false;
      const saveParsed = this.parsed;
      this.parsed = [];
      const rest = this._doParse(text, subOptions, tokenStream, true);
      this.parsed = saveParsed;

      const span = this._createNode("span", secondOpts);
      span.appendChild(rest);
      variableNode.appendChild(span);
    }
    variableNode.appendChild(this.doc.createTextNode(")"));

    return { node: variableNode, value: varValue };
  },

  /**
   * The workhorse for @see _parse. This parses some CSS text,
   * stopping at EOF; or optionally when an umatched close paren is
   * seen.
   *
   * @param  {String} text
   *         The original input text.
   * @param  {Object} options
   *         The options object in use; @see _mergeOptions.
   * @param  {CSSLexer} tokenStream
   *         The token stream from which to read
   * @param  {Boolean} stopAtCloseParen
   *         If true, stop at an umatched close paren.
   * @return {DocumentFragment}
   *         A document fragment.
   */
  // eslint-disable-next-line complexity
  _doParse: function(text, options, tokenStream, stopAtCloseParen) {
    let parenDepth = stopAtCloseParen ? 1 : 0;
    let outerMostFunctionTakesColor = false;
    let fontFamilyNameParts = [];
    let previousWasBang = false;

    const colorOK = function() {
      return (
        options.supportsColor ||
        (options.expectFilter &&
          parenDepth === 1 &&
          outerMostFunctionTakesColor)
      );
    };

    const angleOK = function(angle) {
      return new angleUtils.CssAngle(angle).valid;
    };

    let spaceNeeded = false;
    let done = false;

    while (!done) {
      const token = tokenStream.nextToken();
      if (!token) {
        if (options.expectFont && fontFamilyNameParts.length !== 0) {
          this._appendFontFamily(fontFamilyNameParts.join(""), options);
        }
        break;
      }

      if (token.tokenType === "comment") {
        // This doesn't change spaceNeeded, because we didn't emit
        // anything to the output.
        continue;
      }

      switch (token.tokenType) {
        case "function": {
          if (
            COLOR_TAKING_FUNCTIONS.includes(token.text) ||
            ANGLE_TAKING_FUNCTIONS.includes(token.text)
          ) {
            // The function can accept a color or an angle argument, and we know
            // it isn't special in some other way. So, we let it
            // through to the ordinary parsing loop so that the value
            // can be handled in a single place.
            this._appendTextNode(
              text.substring(token.startOffset, token.endOffset)
            );
            if (parenDepth === 0) {
              outerMostFunctionTakesColor = COLOR_TAKING_FUNCTIONS.includes(
                token.text
              );
            }
            ++parenDepth;
          } else if (token.text === "var" && options.getVariableValue) {
            const { node: variableNode, value } = this._parseVariable(
              token,
              text,
              tokenStream,
              options
            );
            if (
              value &&
              colorOK() &&
              colorUtils.isValidCSSColor(value, this.cssColor4)
            ) {
              this._appendColor(value, {
                ...options,
                variableContainer: variableNode,
              });
            } else {
              this.parsed.push(variableNode);
            }
          } else {
            const { functionData, sawVariable } = this._parseMatchingParens(
              text,
              tokenStream,
              options
            );

            const functionName = text.substring(
              token.startOffset,
              token.endOffset
            );

            if (sawVariable) {
              // If function contains variable, we need to add both strings
              // and nodes.
              this._appendTextNode(functionName);
              for (const data of functionData) {
                if (typeof data === "string") {
                  this._appendTextNode(data);
                } else if (data) {
                  this.parsed.push(data);
                }
              }
              this._appendTextNode(")");
            } else {
              // If no variable in function, join the text together and add
              // to DOM accordingly.
              const functionText = functionName + functionData.join("") + ")";

              if (options.expectCubicBezier && token.text === "cubic-bezier") {
                this._appendCubicBezier(functionText, options);
              } else if (
                colorOK() &&
                colorUtils.isValidCSSColor(functionText, this.cssColor4)
              ) {
                this._appendColor(functionText, options);
              } else if (
                options.expectShape &&
                BASIC_SHAPE_FUNCTIONS.includes(token.text)
              ) {
                this._appendShape(functionText, options);
              } else {
                this._appendTextNode(functionText);
              }
            }
          }
          break;
        }

        case "ident":
          if (
            options.expectCubicBezier &&
            BEZIER_KEYWORDS.includes(token.text)
          ) {
            this._appendCubicBezier(token.text, options);
          } else if (this._isDisplayFlex(text, token, options)) {
            this._appendHighlighterToggle(token.text, options.flexClass);
          } else if (this._isDisplayGrid(text, token, options)) {
            this._appendHighlighterToggle(token.text, options.gridClass);
          } else if (
            colorOK() &&
            colorUtils.isValidCSSColor(token.text, this.cssColor4)
          ) {
            this._appendColor(token.text, options);
          } else if (angleOK(token.text)) {
            this._appendAngle(token.text, options);
          } else if (options.expectFont && !previousWasBang) {
            // We don't append the identifier if the previous token
            // was equal to '!', since in that case we expect the
            // identifier to be equal to 'important'.
            fontFamilyNameParts.push(token.text);
          } else {
            this._appendTextNode(
              text.substring(token.startOffset, token.endOffset)
            );
          }
          break;

        case "id":
        case "hash": {
          const original = text.substring(token.startOffset, token.endOffset);
          if (
            colorOK() &&
            colorUtils.isValidCSSColor(original, this.cssColor4)
          ) {
            if (spaceNeeded) {
              // Insert a space to prevent token pasting when a #xxx
              // color is changed to something like rgb(...).
              this._appendTextNode(" ");
            }
            this._appendColor(original, options);
          } else {
            this._appendTextNode(original);
          }
          break;
        }
        case "dimension":
          const value = text.substring(token.startOffset, token.endOffset);
          if (angleOK(value)) {
            this._appendAngle(value, options);
          } else {
            this._appendTextNode(value);
          }
          break;
        case "url":
        case "bad_url":
          this._appendURL(
            text.substring(token.startOffset, token.endOffset),
            token.text,
            options
          );
          break;

        case "string":
          if (options.expectFont) {
            fontFamilyNameParts.push(
              text.substring(token.startOffset, token.endOffset)
            );
          } else {
            this._appendTextNode(
              text.substring(token.startOffset, token.endOffset)
            );
          }
          break;

        case "whitespace":
          if (options.expectFont) {
            fontFamilyNameParts.push(" ");
          } else {
            this._appendTextNode(
              text.substring(token.startOffset, token.endOffset)
            );
          }
          break;

        case "symbol":
          if (token.text === "(") {
            ++parenDepth;
          } else if (token.text === ")") {
            --parenDepth;

            if (stopAtCloseParen && parenDepth === 0) {
              done = true;
              break;
            }

            if (parenDepth === 0) {
              outerMostFunctionTakesColor = false;
            }
          } else if (
            (token.text === "," || token.text === "!") &&
            options.expectFont &&
            fontFamilyNameParts.length !== 0
          ) {
            this._appendFontFamily(fontFamilyNameParts.join(""), options);
            fontFamilyNameParts = [];
          }
        // falls through
        default:
          this._appendTextNode(
            text.substring(token.startOffset, token.endOffset)
          );
          break;
      }

      // If this token might possibly introduce token pasting when
      // color-cycling, require a space.
      spaceNeeded =
        token.tokenType === "ident" ||
        token.tokenType === "at" ||
        token.tokenType === "id" ||
        token.tokenType === "hash" ||
        token.tokenType === "number" ||
        token.tokenType === "dimension" ||
        token.tokenType === "percentage" ||
        token.tokenType === "dimension";
      previousWasBang = token.tokenType === "symbol" && token.text === "!";
    }

    let result = this._toDOM();

    if (options.expectFilter && !options.filterSwatch) {
      result = this._wrapFilter(text, options, result);
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
  _parse: function(text, options = {}) {
    text = text.trim();
    this.parsed.length = 0;

    const tokenStream = getCSSLexer(text);
    return this._doParse(text, options, tokenStream, false);
  },

  /**
   * Returns true if it's a "display: [inline-]flex" token.
   *
   * @param  {String} text
   *         The parsed text.
   * @param  {Object} token
   *         The parsed token.
   * @param  {Object} options
   *         The options given to _parse.
   */
  _isDisplayFlex: function(text, token, options) {
    return (
      options.expectDisplay &&
      (token.text === "flex" || token.text === "inline-flex")
    );
  },

  /**
   * Returns true if it's a "display: [inline-]grid" token.
   *
   * @param  {String} text
   *         The parsed text.
   * @param  {Object} token
   *         The parsed token.
   * @param  {Object} options
   *         The options given to _parse.
   */
  _isDisplayGrid: function(text, token, options) {
    return (
      options.expectDisplay &&
      (token.text === "grid" || token.text === "inline-grid")
    );
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
    const container = this._createNode("span", {
      "data-bezier": bezier,
    });

    if (options.bezierSwatchClass) {
      const swatch = this._createNode("span", {
        class: options.bezierSwatchClass,
        tabindex: "0",
        role: "button",
      });
      container.appendChild(swatch);
    }

    const value = this._createNode(
      "span",
      {
        class: options.bezierClass,
      },
      bezier
    );

    container.appendChild(value);
    this.parsed.push(container);
  },

  /**
   * Append a Flexbox|Grid highlighter toggle icon next to the value in a
   * "display: [inline-]flex" or "display: [inline-]grid" declaration.
   *
   * @param {String} text
   *        The text value to append
   * @param {String} className
   *        The class name for the toggle span
   */
  _appendHighlighterToggle: function(text, className) {
    const container = this._createNode("span", {});

    const toggle = this._createNode("span", {
      class: className,
    });

    const value = this._createNode("span", {});
    value.textContent = text;

    container.appendChild(toggle);
    container.appendChild(value);
    this.parsed.push(container);
  },

  /**
   * Append a CSS shapes highlighter toggle next to the value, and parse the value
   * into spans, each containing a point that can be hovered over.
   *
   * @param {String} shape
   *        The shape text value to append
   * @param {Object} options
   *        Options object. For valid options and default values see
   *        _mergeOptions()
   */
  _appendShape: function(shape, options) {
    const shapeTypes = [
      {
        prefix: "polygon(",
        coordParser: this._addPolygonPointNodes.bind(this),
      },
      {
        prefix: "circle(",
        coordParser: this._addCirclePointNodes.bind(this),
      },
      {
        prefix: "ellipse(",
        coordParser: this._addEllipsePointNodes.bind(this),
      },
      {
        prefix: "inset(",
        coordParser: this._addInsetPointNodes.bind(this),
      },
    ];

    const container = this._createNode("span", {});

    const toggle = this._createNode("span", {
      class: options.shapeSwatchClass,
      tabindex: "0",
      role: "button",
    });

    for (const { prefix, coordParser } of shapeTypes) {
      if (shape.includes(prefix)) {
        const coordsBegin = prefix.length;
        const coordsEnd = shape.lastIndexOf(")");
        let valContainer = this._createNode("span", {
          class: options.shapeClass,
        });

        container.appendChild(toggle);

        appendText(valContainer, shape.substring(0, coordsBegin));

        const coordsString = shape.substring(coordsBegin, coordsEnd);
        valContainer = coordParser(coordsString, valContainer);

        appendText(valContainer, shape.substring(coordsEnd));
        container.appendChild(valContainer);
      }
    }

    this.parsed.push(container);
  },

  /**
   * Parse the given polygon coordinates and create a span for each coordinate pair,
   * adding it to the given container node.
   *
   * @param {String} coords
   *        The string of coordinate pairs.
   * @param {Node} container
   *        The node to which spans containing points are added.
   * @returns {Node} The container to which spans have been added.
   */
  // eslint-disable-next-line complexity
  _addPolygonPointNodes: function(coords, container) {
    const tokenStream = getCSSLexer(coords);
    let token = tokenStream.nextToken();
    let coord = "";
    let i = 0;
    let depth = 0;
    let isXCoord = true;
    let fillRule = false;
    let coordNode = this._createNode("span", {
      class: "ruleview-shape-point",
      "data-point": `${i}`,
    });

    while (token) {
      if (token.tokenType === "symbol" && token.text === ",") {
        // Comma separating coordinate pairs; add coordNode to container and reset vars
        if (!isXCoord) {
          // Y coord not added to coordNode yet
          const node = this._createNode(
            "span",
            {
              class: "ruleview-shape-point",
              "data-point": `${i}`,
              "data-pair": isXCoord ? "x" : "y",
            },
            coord
          );
          coordNode.appendChild(node);
          coord = "";
          isXCoord = !isXCoord;
        }

        if (fillRule) {
          // If the last text added was a fill-rule, do not increment i.
          fillRule = false;
        } else {
          container.appendChild(coordNode);
          i++;
        }
        appendText(
          container,
          coords.substring(token.startOffset, token.endOffset)
        );
        coord = "";
        depth = 0;
        isXCoord = true;
        coordNode = this._createNode("span", {
          class: "ruleview-shape-point",
          "data-point": `${i}`,
        });
      } else if (token.tokenType === "symbol" && token.text === "(") {
        depth++;
        coord += coords.substring(token.startOffset, token.endOffset);
      } else if (token.tokenType === "symbol" && token.text === ")") {
        depth--;
        coord += coords.substring(token.startOffset, token.endOffset);
      } else if (token.tokenType === "whitespace" && coord === "") {
        // Whitespace at beginning of coord; add to container
        appendText(
          container,
          coords.substring(token.startOffset, token.endOffset)
        );
      } else if (token.tokenType === "whitespace" && depth === 0) {
        // Whitespace signifying end of coord
        const node = this._createNode(
          "span",
          {
            class: "ruleview-shape-point",
            "data-point": `${i}`,
            "data-pair": isXCoord ? "x" : "y",
          },
          coord
        );
        coordNode.appendChild(node);
        appendText(
          coordNode,
          coords.substring(token.startOffset, token.endOffset)
        );
        coord = "";
        isXCoord = !isXCoord;
      } else if (
        token.tokenType === "number" ||
        token.tokenType === "dimension" ||
        token.tokenType === "percentage" ||
        token.tokenType === "function"
      ) {
        if (isXCoord && coord && depth === 0) {
          // Whitespace is not necessary between x/y coords.
          const node = this._createNode(
            "span",
            {
              class: "ruleview-shape-point",
              "data-point": `${i}`,
              "data-pair": "x",
            },
            coord
          );
          coordNode.appendChild(node);
          isXCoord = false;
          coord = "";
        }

        coord += coords.substring(token.startOffset, token.endOffset);
        if (token.tokenType === "function") {
          depth++;
        }
      } else if (
        token.tokenType === "ident" &&
        (token.text === "nonzero" || token.text === "evenodd")
      ) {
        // A fill-rule (nonzero or evenodd).
        appendText(
          container,
          coords.substring(token.startOffset, token.endOffset)
        );
        fillRule = true;
      } else {
        coord += coords.substring(token.startOffset, token.endOffset);
      }
      token = tokenStream.nextToken();
    }

    // Add coords if any are left over
    if (coord) {
      const node = this._createNode(
        "span",
        {
          class: "ruleview-shape-point",
          "data-point": `${i}`,
          "data-pair": isXCoord ? "x" : "y",
        },
        coord
      );
      coordNode.appendChild(node);
      container.appendChild(coordNode);
    }
    return container;
  },

  /**
   * Parse the given circle coordinates and populate the given container appropriately
   * with a separate span for the center point.
   *
   * @param {String} coords
   *        The circle definition.
   * @param {Node} container
   *        The node to which the definition is added.
   * @returns {Node} The container to which the definition has been added.
   */
  // eslint-disable-next-line complexity
  _addCirclePointNodes: function(coords, container) {
    const tokenStream = getCSSLexer(coords);
    let token = tokenStream.nextToken();
    let depth = 0;
    let coord = "";
    let point = "radius";
    const centerNode = this._createNode("span", {
      class: "ruleview-shape-point",
      "data-point": "center",
    });
    while (token) {
      if (token.tokenType === "symbol" && token.text === "(") {
        depth++;
        coord += coords.substring(token.startOffset, token.endOffset);
      } else if (token.tokenType === "symbol" && token.text === ")") {
        depth--;
        coord += coords.substring(token.startOffset, token.endOffset);
      } else if (token.tokenType === "whitespace" && coord === "") {
        // Whitespace at beginning of coord; add to container
        appendText(
          container,
          coords.substring(token.startOffset, token.endOffset)
        );
      } else if (
        token.tokenType === "whitespace" &&
        point === "radius" &&
        depth === 0
      ) {
        // Whitespace signifying end of radius
        const node = this._createNode(
          "span",
          {
            class: "ruleview-shape-point",
            "data-point": "radius",
          },
          coord
        );
        container.appendChild(node);
        appendText(
          container,
          coords.substring(token.startOffset, token.endOffset)
        );
        point = "cx";
        coord = "";
        depth = 0;
      } else if (token.tokenType === "whitespace" && depth === 0) {
        // Whitespace signifying end of cx/cy
        const node = this._createNode(
          "span",
          {
            class: "ruleview-shape-point",
            "data-point": "center",
            "data-pair": point === "cx" ? "x" : "y",
          },
          coord
        );
        centerNode.appendChild(node);
        appendText(
          centerNode,
          coords.substring(token.startOffset, token.endOffset)
        );
        point = point === "cx" ? "cy" : "cx";
        coord = "";
        depth = 0;
      } else if (token.tokenType === "ident" && token.text === "at") {
        // "at"; Add radius to container if not already done so
        if (point === "radius" && coord) {
          const node = this._createNode(
            "span",
            {
              class: "ruleview-shape-point",
              "data-point": "radius",
            },
            coord
          );
          container.appendChild(node);
        }
        appendText(
          container,
          coords.substring(token.startOffset, token.endOffset)
        );
        point = "cx";
        coord = "";
        depth = 0;
      } else if (
        token.tokenType === "number" ||
        token.tokenType === "dimension" ||
        token.tokenType === "percentage" ||
        token.tokenType === "function"
      ) {
        if (point === "cx" && coord && depth === 0) {
          // Center coords don't require whitespace between x/y. So if current point is
          // cx, we have the cx coord, and depth is 0, then this token is actually cy.
          // Add cx to centerNode and set point to cy.
          const node = this._createNode(
            "span",
            {
              class: "ruleview-shape-point",
              "data-point": "center",
              "data-pair": "x",
            },
            coord
          );
          centerNode.appendChild(node);
          point = "cy";
          coord = "";
        }

        coord += coords.substring(token.startOffset, token.endOffset);
        if (token.tokenType === "function") {
          depth++;
        }
      } else {
        coord += coords.substring(token.startOffset, token.endOffset);
      }
      token = tokenStream.nextToken();
    }

    // Add coords if any are left over.
    if (coord) {
      if (point === "radius") {
        const node = this._createNode(
          "span",
          {
            class: "ruleview-shape-point",
            "data-point": "radius",
          },
          coord
        );
        container.appendChild(node);
      } else {
        const node = this._createNode(
          "span",
          {
            class: "ruleview-shape-point",
            "data-point": "center",
            "data-pair": point === "cx" ? "x" : "y",
          },
          coord
        );
        centerNode.appendChild(node);
      }
    }

    if (centerNode.textContent) {
      container.appendChild(centerNode);
    }
    return container;
  },

  /**
   * Parse the given ellipse coordinates and populate the given container appropriately
   * with a separate span for each point
   *
   * @param {String} coords
   *        The ellipse definition.
   * @param {Node} container
   *        The node to which the definition is added.
   * @returns {Node} The container to which the definition has been added.
   */
  // eslint-disable-next-line complexity
  _addEllipsePointNodes: function(coords, container) {
    const tokenStream = getCSSLexer(coords);
    let token = tokenStream.nextToken();
    let depth = 0;
    let coord = "";
    let point = "rx";
    const centerNode = this._createNode("span", {
      class: "ruleview-shape-point",
      "data-point": "center",
    });
    while (token) {
      if (token.tokenType === "symbol" && token.text === "(") {
        depth++;
        coord += coords.substring(token.startOffset, token.endOffset);
      } else if (token.tokenType === "symbol" && token.text === ")") {
        depth--;
        coord += coords.substring(token.startOffset, token.endOffset);
      } else if (token.tokenType === "whitespace" && coord === "") {
        // Whitespace at beginning of coord; add to container
        appendText(
          container,
          coords.substring(token.startOffset, token.endOffset)
        );
      } else if (token.tokenType === "whitespace" && depth === 0) {
        if (point === "rx" || point === "ry") {
          // Whitespace signifying end of rx/ry
          const node = this._createNode(
            "span",
            {
              class: "ruleview-shape-point",
              "data-point": point,
            },
            coord
          );
          container.appendChild(node);
          appendText(
            container,
            coords.substring(token.startOffset, token.endOffset)
          );
          point = point === "rx" ? "ry" : "cx";
          coord = "";
          depth = 0;
        } else {
          // Whitespace signifying end of cx/cy
          const node = this._createNode(
            "span",
            {
              class: "ruleview-shape-point",
              "data-point": "center",
              "data-pair": point === "cx" ? "x" : "y",
            },
            coord
          );
          centerNode.appendChild(node);
          appendText(
            centerNode,
            coords.substring(token.startOffset, token.endOffset)
          );
          point = point === "cx" ? "cy" : "cx";
          coord = "";
          depth = 0;
        }
      } else if (token.tokenType === "ident" && token.text === "at") {
        // "at"; Add radius to container if not already done so
        if (point === "ry" && coord) {
          const node = this._createNode(
            "span",
            {
              class: "ruleview-shape-point",
              "data-point": "ry",
            },
            coord
          );
          container.appendChild(node);
        }
        appendText(
          container,
          coords.substring(token.startOffset, token.endOffset)
        );
        point = "cx";
        coord = "";
        depth = 0;
      } else if (
        token.tokenType === "number" ||
        token.tokenType === "dimension" ||
        token.tokenType === "percentage" ||
        token.tokenType === "function"
      ) {
        if (point === "rx" && coord && depth === 0) {
          // Radius coords don't require whitespace between x/y.
          const node = this._createNode(
            "span",
            {
              class: "ruleview-shape-point",
              "data-point": "rx",
            },
            coord
          );
          container.appendChild(node);
          point = "ry";
          coord = "";
        }
        if (point === "cx" && coord && depth === 0) {
          // Center coords don't require whitespace between x/y.
          const node = this._createNode(
            "span",
            {
              class: "ruleview-shape-point",
              "data-point": "center",
              "data-pair": "x",
            },
            coord
          );
          centerNode.appendChild(node);
          point = "cy";
          coord = "";
        }

        coord += coords.substring(token.startOffset, token.endOffset);
        if (token.tokenType === "function") {
          depth++;
        }
      } else {
        coord += coords.substring(token.startOffset, token.endOffset);
      }
      token = tokenStream.nextToken();
    }

    // Add coords if any are left over.
    if (coord) {
      if (point === "rx" || point === "ry") {
        const node = this._createNode(
          "span",
          {
            class: "ruleview-shape-point",
            "data-point": point,
          },
          coord
        );
        container.appendChild(node);
      } else {
        const node = this._createNode(
          "span",
          {
            class: "ruleview-shape-point",
            "data-point": "center",
            "data-pair": point === "cx" ? "x" : "y",
          },
          coord
        );
        centerNode.appendChild(node);
      }
    }

    if (centerNode.textContent) {
      container.appendChild(centerNode);
    }
    return container;
  },

  /**
   * Parse the given inset coordinates and populate the given container appropriately.
   *
   * @param {String} coords
   *        The inset definition.
   * @param {Node} container
   *        The node to which the definition is added.
   * @returns {Node} The container to which the definition has been added.
   */
  // eslint-disable-next-line complexity
  _addInsetPointNodes: function(coords, container) {
    const insetPoints = ["top", "right", "bottom", "left"];
    const tokenStream = getCSSLexer(coords);
    let token = tokenStream.nextToken();
    let depth = 0;
    let coord = "";
    let i = 0;
    let round = false;
    // nodes is an array containing all the coordinate spans. otherText is an array of
    // arrays, each containing the text that should be inserted into container before
    // the node with the same index. i.e. all elements of otherText[i] is inserted
    // into container before nodes[i].
    const nodes = [];
    const otherText = [[]];

    while (token) {
      if (round) {
        // Everything that comes after "round" should just be plain text
        otherText[i].push(coords.substring(token.startOffset, token.endOffset));
      } else if (token.tokenType === "symbol" && token.text === "(") {
        depth++;
        coord += coords.substring(token.startOffset, token.endOffset);
      } else if (token.tokenType === "symbol" && token.text === ")") {
        depth--;
        coord += coords.substring(token.startOffset, token.endOffset);
      } else if (token.tokenType === "whitespace" && coord === "") {
        // Whitespace at beginning of coord; add to container
        otherText[i].push(coords.substring(token.startOffset, token.endOffset));
      } else if (token.tokenType === "whitespace" && depth === 0) {
        // Whitespace signifying end of coord; create node and push to nodes
        const node = this._createNode(
          "span",
          {
            class: "ruleview-shape-point",
          },
          coord
        );
        nodes.push(node);
        i++;
        coord = "";
        otherText[i] = [coords.substring(token.startOffset, token.endOffset)];
        depth = 0;
      } else if (
        token.tokenType === "number" ||
        token.tokenType === "dimension" ||
        token.tokenType === "percentage" ||
        token.tokenType === "function"
      ) {
        if (coord && depth === 0) {
          // Inset coords don't require whitespace between each coord.
          const node = this._createNode(
            "span",
            {
              class: "ruleview-shape-point",
            },
            coord
          );
          nodes.push(node);
          i++;
          coord = "";
          otherText[i] = [];
        }

        coord += coords.substring(token.startOffset, token.endOffset);
        if (token.tokenType === "function") {
          depth++;
        }
      } else if (token.tokenType === "ident" && token.text === "round") {
        if (coord && depth === 0) {
          // Whitespace is not necessary before "round"; create a new node for the coord
          const node = this._createNode(
            "span",
            {
              class: "ruleview-shape-point",
            },
            coord
          );
          nodes.push(node);
          i++;
          coord = "";
          otherText[i] = [];
        }
        round = true;
        otherText[i].push(coords.substring(token.startOffset, token.endOffset));
      } else {
        coord += coords.substring(token.startOffset, token.endOffset);
      }
      token = tokenStream.nextToken();
    }

    // Take care of any leftover text
    if (coord) {
      if (round) {
        otherText[i].push(coord);
      } else {
        const node = this._createNode(
          "span",
          {
            class: "ruleview-shape-point",
          },
          coord
        );
        nodes.push(node);
      }
    }

    // insetPoints contains the 4 different possible inset points in the order they are
    // defined. By taking the modulo of the index in insetPoints with the number of nodes,
    // we can get which node represents each point (e.g. if there is only 1 node, it
    // represents all 4 points). The exception is "left" when there are 3 nodes. In that
    // case, it is nodes[1] that represents the left point rather than nodes[0].
    for (let j = 0; j < 4; j++) {
      const point = insetPoints[j];
      const nodeIndex =
        point === "left" && nodes.length === 3 ? 1 : j % nodes.length;
      nodes[nodeIndex].classList.add(point);
    }

    nodes.forEach((node, j, array) => {
      for (const text of otherText[j]) {
        appendText(container, text);
      }
      container.appendChild(node);
    });

    // Add text that comes after the last node, if any exists
    if (otherText[nodes.length]) {
      for (const text of otherText[nodes.length]) {
        appendText(container, text);
      }
    }

    return container;
  },

  /**
   * Append a angle value to the output
   *
   * @param {String} angle
   *        angle to append
   * @param {Object} options
   *        Options object. For valid options and default values see
   *        _mergeOptions()
   */
  _appendAngle: function(angle, options) {
    const angleObj = new angleUtils.CssAngle(angle);
    const container = this._createNode("span", {
      "data-angle": angle,
    });

    if (options.angleSwatchClass) {
      const swatch = this._createNode("span", {
        class: options.angleSwatchClass,
        tabindex: "0",
        role: "button",
      });
      this.angleSwatches.set(swatch, angleObj);
      swatch.addEventListener("mousedown", this._onAngleSwatchMouseDown);

      // Add click listener to stop event propagation when shift key is pressed
      // in order to prevent the value input to be focused.
      // Bug 711942 will add a tooltip to edit angle values and we should
      // be able to move this listener to Tooltip.js when it'll be implemented.
      swatch.addEventListener("click", function(event) {
        if (event.shiftKey) {
          event.stopPropagation();
        }
      });
      EventEmitter.decorate(swatch);
      container.appendChild(swatch);
    }

    const value = this._createNode(
      "span",
      {
        class: options.angleClass,
      },
      angle
    );

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
    // Checking pair as a CSS declaration string to account for "!important" in value.
    const declaration = `${name}:${value}`;
    return this.doc.defaultView.CSS.supports(declaration);
  },

  /**
   * Tests if a given colorObject output by CssColor is valid for parsing.
   * Valid means it's really a color, not any of the CssColor SPECIAL_VALUES
   * except transparent
   */
  _isValidColor: function(colorObj) {
    return (
      colorObj.valid &&
      (!colorObj.specialValue || colorObj.specialValue === "transparent")
    );
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
  _appendColor: function(color, options = {}) {
    const colorObj = new colorUtils.CssColor(color, this.cssColor4);

    if (this._isValidColor(colorObj)) {
      const container = this._createNode("span", {
        "data-color": color,
      });

      if (options.colorSwatchClass) {
        let attributes = {
          class: options.colorSwatchClass,
          style: "background-color:" + color,
        };

        // Color swatches next to values trigger the color editor everywhere aside from
        // the Computed panel where values are read-only.
        if (!options.colorSwatchClass.startsWith("computed-")) {
          attributes = { ...attributes, tabindex: "0", role: "button" };
        }

        // The swatch is a <span> instead of a <button> intentionally. See Bug 1597125.
        // It is made keyboard accessible via `tabindex` and has keydown handlers
        // attached for pressing SPACE and RETURN in SwatchBasedEditorTooltip.js
        const swatch = this._createNode("span", attributes);
        this.colorSwatches.set(swatch, colorObj);
        swatch.addEventListener("mousedown", this._onColorSwatchMouseDown);
        EventEmitter.decorate(swatch);

        container.appendChild(swatch);
      }

      if (!options.defaultColorType) {
        // If we're not being asked to convert the color to the default color type
        // specified by the user, then force the CssColor instance to be set to the type
        // of the current color.
        // Not having a type means that the default color type will be automatically used.
        colorObj.colorUnit = colorUtils.classifyColor(color);
      }
      color = colorObj.toString();
      container.dataset.color = color;

      // Next we create the markup to show the value of the property.
      if (options.variableContainer) {
        // If we are creating a color swatch for a CSS variable we simply reuse
        // the markup created for the variableContainer.
        if (options.colorClass) {
          options.variableContainer.classList.add(options.colorClass);
        }
        container.appendChild(options.variableContainer);
      } else {
        // Otherwise we create a new element with the `color` as textContent.
        const value = this._createNode(
          "span",
          {
            class: options.colorClass,
          },
          color
        );

        container.appendChild(value);
      }

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
    const container = this._createNode("span", {
      "data-filters": filters,
    });

    if (options.filterSwatchClass) {
      const swatch = this._createNode("span", {
        class: options.filterSwatchClass,
        tabindex: "0",
        role: "button",
      });
      container.appendChild(swatch);
    }

    const value = this._createNode("span", {
      class: options.filterClass,
    });
    value.appendChild(nodes);
    container.appendChild(value);

    return container;
  },

  _onColorSwatchMouseDown: function(event) {
    if (!event.shiftKey) {
      return;
    }

    // Prevent click event to be fired to not show the tooltip
    event.stopPropagation();

    const swatch = event.target;
    const color = this.colorSwatches.get(swatch);
    const val = color.nextColorUnit();

    swatch.nextElementSibling.textContent = val;
    swatch.parentNode.dataset.color = val;
    swatch.emit("unit-change", val);
  },

  _onAngleSwatchMouseDown: function(event) {
    if (!event.shiftKey) {
      return;
    }

    event.stopPropagation();

    const swatch = event.target;
    const angle = this.angleSwatches.get(swatch);
    const val = angle.nextAngleUnit();

    swatch.nextElementSibling.textContent = val;
    swatch.emit("unit-change", val);
  },

  /**
   * A helper function that sanitizes a possibly-unterminated URL.
   */
  _sanitizeURL: function(url) {
    // Re-lex the URL and add any needed termination characters.
    const urlTokenizer = getCSSLexer(url);
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
      const urlParts = /^(url\([ \t\r\n\f]*(["']?))(.*?)(\2[ \t\r\n\f]*\))$/i.exec(
        match
      );

      // Bail out if that didn't match anything.
      if (!urlParts) {
        this._appendTextNode(match);
        return;
      }

      const [, leader, , body, trailer] = urlParts;

      this._appendTextNode(leader);

      let href = url;
      if (options.baseURI) {
        try {
          href = new URL(url, options.baseURI).href;
        } catch (e) {
          // Ignore.
        }
      }

      this._appendNode(
        "a",
        {
          target: "_blank",
          class: options.urlClass,
          href: href,
        },
        body
      );

      this._appendTextNode(trailer);
    } else {
      this._appendTextNode(match);
    }
  },

  /**
   * Append a font family to the output.
   *
   * @param  {String} fontFamily
   *         Font family to append
   * @param  {Object} options
   *         Options object. For valid options and default values see
   *         _mergeOptions().
   */
  _appendFontFamily: function(fontFamily, options) {
    let spanContents = fontFamily;
    let quoteChar = null;
    let trailingWhitespace = false;

    // Before appending the actual font-family span, we need to trim
    // down the actual contents by removing any whitespace before and
    // after, and any quotation characters in the passed string.  Any
    // such characters are preserved in the actual output, but just
    // not inside the span element.

    if (spanContents[0] === " ") {
      this._appendTextNode(" ");
      spanContents = spanContents.slice(1);
    }

    if (spanContents[spanContents.length - 1] === " ") {
      spanContents = spanContents.slice(0, -1);
      trailingWhitespace = true;
    }

    if (spanContents[0] === "'" || spanContents[0] === '"') {
      quoteChar = spanContents[0];
    }

    if (quoteChar) {
      this._appendTextNode(quoteChar);
      spanContents = spanContents.slice(1, -1);
    }

    this._appendNode(
      "span",
      {
        class: options.fontFamilyClass,
      },
      spanContents
    );

    if (quoteChar) {
      this._appendTextNode(quoteChar);
    }

    if (trailingWhitespace) {
      this._appendTextNode(" ");
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
  _createNode: function(tagName, attributes, value = "") {
    const node = this.doc.createElementNS(HTML_NS, tagName);
    const attrs = Object.getOwnPropertyNames(attributes);

    for (const attr of attrs) {
      if (attributes[attr]) {
        node.setAttribute(attr, attributes[attr]);
      }
    }

    if (value) {
      const textNode = this.doc.createTextNode(value);
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
  _appendNode: function(tagName, attributes, value = "") {
    const node = this._createNode(tagName, attributes, value);
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
    const lastItem = this.parsed[this.parsed.length - 1];
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
    const frag = this.doc.createDocumentFragment();

    for (const item of this.parsed) {
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
   *           - angleClass: ""         // The class to use for the angle value
   *                                    // that follows the swatch.
   *           - angleSwatchClass: ""   // The class to use for angle swatches.
   *           - bezierClass: ""        // The class to use for the bezier value
   *                                    // that follows the swatch.
   *           - bezierSwatchClass: ""  // The class to use for bezier swatches.
   *           - colorClass: ""         // The class to use for the color value
   *                                    // that follows the swatch.
   *           - colorSwatchClass: ""   // The class to use for color swatches.
   *           - filterSwatch: false    // A special case for parsing a
   *                                    // "filter" property, causing the
   *                                    // parser to skip the call to
   *                                    // _wrapFilter.  Used only for
   *                                    // previewing with the filter swatch.
   *           - flexClass: ""          // The class to use for the flex icon.
   *           - gridClass: ""          // The class to use for the grid icon.
   *           - shapeClass: ""         // The class to use for the shape value
   *                                    // that follows the swatch.
   *           - shapeSwatchClass: ""   // The class to use for the shape swatch.
   *           - supportsColor: false   // Does the CSS property support colors?
   *           - urlClass: ""           // The class to be used for url() links.
   *           - fontFamilyClass: ""    // The class to be used for font families.
   *           - baseURI: undefined     // A string used to resolve
   *                                    // relative links.
   *           - getVariableValue       // A function taking a single
   *                                    // argument, the name of a variable.
   *                                    // This should return the variable's
   *                                    // value, if it is in use; or null.
   *           - unmatchedVariableClass: ""
   *                                    // The class to use for a component
   *                                    // of a "var(...)" that is not in
   *                                    // use.
   * @return {Object}
   *         Overridden options object
   */
  _mergeOptions: function(overrides) {
    const defaults = {
      defaultColorType: true,
      angleClass: "",
      angleSwatchClass: "",
      bezierClass: "",
      bezierSwatchClass: "",
      colorClass: "",
      colorSwatchClass: "",
      filterSwatch: false,
      flexClass: "",
      gridClass: "",
      shapeClass: "",
      shapeSwatchClass: "",
      supportsColor: false,
      urlClass: "",
      fontFamilyClass: "",
      baseURI: undefined,
      getVariableValue: null,
      unmatchedVariableClass: null,
    };

    for (const item in overrides) {
      defaults[item] = overrides[item];
    }
    return defaults;
  },
};

module.exports = OutputParser;
