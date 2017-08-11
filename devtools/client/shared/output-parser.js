/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {angleUtils} = require("devtools/client/shared/css-angle");
const {colorUtils} = require("devtools/shared/css/color");
const {getCSSLexer} = require("devtools/shared/css/lexer");
const EventEmitter = require("devtools/shared/old-event-emitter");
const {
  ANGLE_TAKING_FUNCTIONS,
  BASIC_SHAPE_FUNCTIONS,
  BEZIER_KEYWORDS,
  COLOR_TAKING_FUNCTIONS,
  CSS_TYPES
} = require("devtools/shared/css/properties-db");
const {appendText} = require("devtools/client/inspector/shared/utils");
const Services = require("Services");

const HTML_NS = "http://www.w3.org/1999/xhtml";
const CSS_GRID_ENABLED_PREF = "layout.css.grid.enabled";
const CSS_SHAPES_ENABLED_PREF = "devtools.inspector.shapesHighlighter.enabled";

/**
 * This module is used to process text for output by developer tools. This means
 * linking JS files with the debugger, CSS files with the style editor, JS
 * functions with the debugger, placing color swatches next to colors and
 * adding doorhanger previews where possible (images, angles, lengths,
 * border radius, cubic-bezier etc.).
 *
 * Usage:
 *   const OutputParser = require("devtools/client/shared/output-parser");
 *
 *   let parser = new OutputParser(document, supportsType);
 *
 *   parser.parseCssProperty("color", "red"); // Returns document fragment.
 *
 * @param {Document} document Used to create DOM nodes.
 * @param {Function} supportsTypes - A function that returns a boolean when asked if a css
 *                   property name supports a given css type.
 *                   The function is executed like supportsType("color", CSS_TYPES.COLOR)
 *                   where CSS_TYPES is defined in devtools/shared/css/properties-db.js
 * @param {Function} isValidOnClient - A function that checks if a css property
 *                   name/value combo is valid.
 * @param {Function} supportsCssColor4ColorFunction - A function for checking
 *                   the supporting of css-color-4 color function.
 */
function OutputParser(document,
                      {supportsType, isValidOnClient, supportsCssColor4ColorFunction}) {
  this.parsed = [];
  this.doc = document;
  this.supportsType = supportsType;
  this.isValidOnClient = isValidOnClient;
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
  parseCssProperty: function (name, value, options = {}) {
    options = this._mergeOptions(options);

    options.expectCubicBezier = this.supportsType(name, CSS_TYPES.TIMING_FUNCTION);
    options.expectDisplay = name === "display";
    options.expectFilter = name === "filter";
    options.expectShape = name === "clip-path" || name === "shape-outside";
    options.supportsColor = this.supportsType(name, CSS_TYPES.COLOR) ||
                            this.supportsType(name, CSS_TYPES.GRADIENT);

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
  _collectFunctionText: function (initialToken, text, tokenStream) {
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
  _parse: function (text, options = {}) {
    text = text.trim();
    this.parsed.length = 0;

    let tokenStream = getCSSLexer(text);
    let parenDepth = 0;
    let outerMostFunctionTakesColor = false;

    let colorOK = function () {
      return options.supportsColor ||
        (options.expectFilter && parenDepth === 1 &&
         outerMostFunctionTakesColor);
    };

    let angleOK = function (angle) {
      return (new angleUtils.CssAngle(angle)).valid;
    };

    let spaceNeeded = false;
    let token = tokenStream.nextToken();
    while (token) {
      if (token.tokenType === "comment") {
        // This doesn't change spaceNeeded, because we didn't emit
        // anything to the output.
        token = tokenStream.nextToken();
        continue;
      }

      switch (token.tokenType) {
        case "function": {
          if (COLOR_TAKING_FUNCTIONS.includes(token.text) ||
              ANGLE_TAKING_FUNCTIONS.includes(token.text)) {
            // The function can accept a color or an angle argument, and we know
            // it isn't special in some other way. So, we let it
            // through to the ordinary parsing loop so that the value
            // can be handled in a single place.
            this._appendTextNode(text.substring(token.startOffset,
                                                token.endOffset));
            if (parenDepth === 0) {
              outerMostFunctionTakesColor = COLOR_TAKING_FUNCTIONS.includes(
                token.text);
            }
            ++parenDepth;
          } else {
            let functionText = this._collectFunctionText(token, text,
                                                         tokenStream);

            if (options.expectCubicBezier && token.text === "cubic-bezier") {
              this._appendCubicBezier(functionText, options);
            } else if (colorOK() &&
                       colorUtils.isValidCSSColor(functionText, this.cssColor4)) {
              this._appendColor(functionText, options);
            } else if (options.expectShape &&
                       Services.prefs.getBoolPref(CSS_SHAPES_ENABLED_PREF) &&
                       BASIC_SHAPE_FUNCTIONS.includes(token.text)) {
              this._appendShape(functionText, options);
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
          } else if (Services.prefs.getBoolPref(CSS_GRID_ENABLED_PREF) &&
                     this._isDisplayGrid(text, token, options)) {
            this._appendGrid(token.text, options);
          } else if (colorOK() &&
                     colorUtils.isValidCSSColor(token.text, this.cssColor4)) {
            this._appendColor(token.text, options);
          } else if (angleOK(token.text)) {
            this._appendAngle(token.text, options);
          } else {
            this._appendTextNode(text.substring(token.startOffset,
                                                token.endOffset));
          }
          break;

        case "id":
        case "hash": {
          let original = text.substring(token.startOffset, token.endOffset);
          if (colorOK() && colorUtils.isValidCSSColor(original, this.cssColor4)) {
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
          let value = text.substring(token.startOffset, token.endOffset);
          if (angleOK(value)) {
            this._appendAngle(value, options);
          } else {
            this._appendTextNode(value);
          }
          break;
        case "url":
        case "bad_url":
          this._appendURL(text.substring(token.startOffset, token.endOffset),
                          token.text, options);
          break;

        case "symbol":
          if (token.text === "(") {
            ++parenDepth;
          } else if (token.text === ")") {
            --parenDepth;
            if (parenDepth === 0) {
              outerMostFunctionTakesColor = false;
            }
          }
          // falls through
        default:
          this._appendTextNode(
            text.substring(token.startOffset, token.endOffset));
          break;
      }

      // If this token might possibly introduce token pasting when
      // color-cycling, require a space.
      spaceNeeded = (token.tokenType === "ident" || token.tokenType === "at" ||
                     token.tokenType === "id" || token.tokenType === "hash" ||
                     token.tokenType === "number" || token.tokenType === "dimension" ||
                     token.tokenType === "percentage" || token.tokenType === "dimension");

      token = tokenStream.nextToken();
    }

    let result = this._toDOM();

    if (options.expectFilter && !options.filterSwatch) {
      result = this._wrapFilter(text, options, result);
    }

    return result;
  },

  /**
   * Return true if it's a display:[inline-]grid token.
   *
   * @param  {String} text
   *         the parsed text.
   * @param  {Object} token
   *         the parsed token.
   * @param  {Object} options
   *         the options given to _parse.
   */
  _isDisplayGrid: function (text, token, options) {
    return options.expectDisplay &&
      (token.text === "grid" || token.text === "inline-grid");
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
  _appendCubicBezier: function (bezier, options) {
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
   * Append a CSS Grid highlighter toggle icon next to the value in a
   * 'display: grid' declaration
   *
   * @param {String} grid
   *        The grid text value to append
   * @param {Object} options
   *        Options object. For valid options and default values see
   *        _mergeOptions()
   */
  _appendGrid: function (grid, options) {
    let container = this._createNode("span", {});

    let toggle = this._createNode("span", {
      class: options.gridClass
    });

    let value = this._createNode("span", {});
    value.textContent = grid;

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
  _appendShape: function (shape, options) {
    const shapeTypes = [{
      prefix: "polygon(",
      coordParser: this._addPolygonPointNodes.bind(this)
    }, {
      prefix: "circle(",
      coordParser: this._addCirclePointNodes.bind(this)
    }, {
      prefix: "ellipse(",
      coordParser: this._addEllipsePointNodes.bind(this)
    }, {
      prefix: "inset(",
      coordParser: this._addInsetPointNodes.bind(this)
    }];

    let container = this._createNode("span", {});

    let toggle = this._createNode("span", {
      class: options.shapeClass
    });

    for (let { prefix, coordParser } of shapeTypes) {
      if (shape.includes(prefix)) {
        let coordsBegin = prefix.length;
        let coordsEnd = shape.lastIndexOf(")");
        let valContainer = this._createNode("span", {});

        container.appendChild(toggle);

        appendText(valContainer, shape.substring(0, coordsBegin));

        let coordsString = shape.substring(coordsBegin, coordsEnd);
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
  _addPolygonPointNodes: function (coords, container) {
    let tokenStream = getCSSLexer(coords);
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
          let node = this._createNode("span", {
            class: "ruleview-shape-point",
            "data-point": `${i}`,
            "data-pair": (isXCoord) ? "x" : "y"
          }, coord);
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
        appendText(container, coords.substring(token.startOffset, token.endOffset));
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
        appendText(container, coords.substring(token.startOffset, token.endOffset));
      } else if (token.tokenType === "whitespace" && depth === 0) {
        // Whitespace signifying end of coord
        let node = this._createNode("span", {
          class: "ruleview-shape-point",
          "data-point": `${i}`,
          "data-pair": (isXCoord) ? "x" : "y"
        }, coord);
        coordNode.appendChild(node);
        appendText(coordNode, coords.substring(token.startOffset, token.endOffset));
        coord = "";
        isXCoord = !isXCoord;
      } else if ((token.tokenType === "number" || token.tokenType === "dimension" ||
                  token.tokenType === "percentage" || token.tokenType === "function")) {
        if (isXCoord && coord && depth === 0) {
          // Whitespace is not necessary between x/y coords.
          let node = this._createNode("span", {
            class: "ruleview-shape-point",
            "data-point": `${i}`,
            "data-pair": "x"
          }, coord);
          coordNode.appendChild(node);
          isXCoord = false;
          coord = "";
        }

        coord += coords.substring(token.startOffset, token.endOffset);
        if (token.tokenType === "function") {
          depth++;
        }
      } else if (token.tokenType === "ident" &&
                 (token.text === "nonzero" || token.text === "evenodd")) {
        // A fill-rule (nonzero or evenodd).
        appendText(container, coords.substring(token.startOffset, token.endOffset));
        fillRule = true;
      } else {
        coord += coords.substring(token.startOffset, token.endOffset);
      }
      token = tokenStream.nextToken();
    }

    // Add coords if any are left over
    if (coord) {
      let node = this._createNode("span", {
        class: "ruleview-shape-point",
        "data-point": `${i}`,
        "data-pair": (isXCoord) ? "x" : "y"
      }, coord);
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
  _addCirclePointNodes: function (coords, container) {
    let tokenStream = getCSSLexer(coords);
    let token = tokenStream.nextToken();
    let depth = 0;
    let coord = "";
    let point = "radius";
    let centerNode = this._createNode("span", {
      class: "ruleview-shape-point",
      "data-point": "center"
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
        appendText(container, coords.substring(token.startOffset, token.endOffset));
      } else if (token.tokenType === "whitespace" && point === "radius" && depth === 0) {
        // Whitespace signifying end of radius
        let node = this._createNode("span", {
          class: "ruleview-shape-point",
          "data-point": "radius"
        }, coord);
        container.appendChild(node);
        appendText(container, coords.substring(token.startOffset, token.endOffset));
        point = "cx";
        coord = "";
        depth = 0;
      } else if (token.tokenType === "whitespace" && depth === 0) {
        // Whitespace signifying end of cx/cy
        let node = this._createNode("span", {
          class: "ruleview-shape-point",
          "data-point": "center",
          "data-pair": (point === "cx") ? "x" : "y"
        }, coord);
        centerNode.appendChild(node);
        appendText(centerNode, coords.substring(token.startOffset, token.endOffset));
        point = (point === "cx") ? "cy" : "cx";
        coord = "";
        depth = 0;
      } else if (token.tokenType === "ident" && token.text === "at") {
        // "at"; Add radius to container if not already done so
        if (point === "radius" && coord) {
          let node = this._createNode("span", {
            class: "ruleview-shape-point",
            "data-point": "radius"
          }, coord);
          container.appendChild(node);
        }
        appendText(container, coords.substring(token.startOffset, token.endOffset));
        point = "cx";
        coord = "";
        depth = 0;
      } else if ((token.tokenType === "number" || token.tokenType === "dimension" ||
                  token.tokenType === "percentage" || token.tokenType === "function")) {
        if (point === "cx" && coord && depth === 0) {
          // Center coords don't require whitespace between x/y. So if current point is
          // cx, we have the cx coord, and depth is 0, then this token is actually cy.
          // Add cx to centerNode and set point to cy.
          let node = this._createNode("span", {
            class: "ruleview-shape-point",
            "data-point": "center",
            "data-pair": "x"
          }, coord);
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
        let node = this._createNode("span", {
          class: "ruleview-shape-point",
          "data-point": "radius"
        }, coord);
        container.appendChild(node);
      } else {
        let node = this._createNode("span", {
          class: "ruleview-shape-point",
          "data-point": "center",
          "data-pair": (point === "cx") ? "x" : "y"
        }, coord);
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
  _addEllipsePointNodes: function (coords, container) {
    let tokenStream = getCSSLexer(coords);
    let token = tokenStream.nextToken();
    let depth = 0;
    let coord = "";
    let point = "rx";
    let centerNode = this._createNode("span", {
      class: "ruleview-shape-point",
      "data-point": "center"
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
        appendText(container, coords.substring(token.startOffset, token.endOffset));
      } else if (token.tokenType === "whitespace" && depth === 0) {
        if (point === "rx" || point === "ry") {
          // Whitespace signifying end of rx/ry
          let node = this._createNode("span", {
            class: "ruleview-shape-point",
            "data-point": point,
          }, coord);
          container.appendChild(node);
          appendText(container, coords.substring(token.startOffset, token.endOffset));
          point = (point === "rx") ? "ry" : "cx";
          coord = "";
          depth = 0;
        } else {
          // Whitespace signifying end of cx/cy
          let node = this._createNode("span", {
            class: "ruleview-shape-point",
            "data-point": "center",
            "data-pair": (point === "cx") ? "x" : "y"
          }, coord);
          centerNode.appendChild(node);
          appendText(centerNode, coords.substring(token.startOffset, token.endOffset));
          point = (point === "cx") ? "cy" : "cx";
          coord = "";
          depth = 0;
        }
      } else if (token.tokenType === "ident" && token.text === "at") {
        // "at"; Add radius to container if not already done so
        if (point === "ry" && coord) {
          let node = this._createNode("span", {
            class: "ruleview-shape-point",
            "data-point": "ry"
          }, coord);
          container.appendChild(node);
        }
        appendText(container, coords.substring(token.startOffset, token.endOffset));
        point = "cx";
        coord = "";
        depth = 0;
      } else if ((token.tokenType === "number" || token.tokenType === "dimension" ||
                  token.tokenType === "percentage" || token.tokenType === "function")) {
        if (point === "rx" && coord && depth === 0) {
          // Radius coords don't require whitespace between x/y.
          let node = this._createNode("span", {
            class: "ruleview-shape-point",
            "data-point": "rx",
          }, coord);
          container.appendChild(node);
          point = "ry";
          coord = "";
        }
        if (point === "cx" && coord && depth === 0) {
          // Center coords don't require whitespace between x/y.
          let node = this._createNode("span", {
            class: "ruleview-shape-point",
            "data-point": "center",
            "data-pair": "x"
          }, coord);
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
        let node = this._createNode("span", {
          class: "ruleview-shape-point",
          "data-point": point
        }, coord);
        container.appendChild(node);
      } else {
        let node = this._createNode("span", {
          class: "ruleview-shape-point",
          "data-point": "center",
          "data-pair": (point === "cx") ? "x" : "y"
        }, coord);
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
  _addInsetPointNodes: function (coords, container) {
    const insetPoints = ["top", "right", "bottom", "left"];
    let tokenStream = getCSSLexer(coords);
    let token = tokenStream.nextToken();
    let depth = 0;
    let coord = "";
    let i = 0;
    let round = false;
    // nodes is an array containing all the coordinate spans. otherText is an array of
    // arrays, each containing the text that should be inserted into container before
    // the node with the same index. i.e. all elements of otherText[i] is inserted
    // into container before nodes[i].
    let nodes = [];
    let otherText = [[]];

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
        let node = this._createNode("span", {
          class: "ruleview-shape-point"
        }, coord);
        nodes.push(node);
        i++;
        coord = "";
        otherText[i] = [coords.substring(token.startOffset, token.endOffset)];
        depth = 0;
      } else if ((token.tokenType === "number" || token.tokenType === "dimension" ||
                  token.tokenType === "percentage" || token.tokenType === "function")) {
        if (coord && depth === 0) {
          // Inset coords don't require whitespace between each coord.
          let node = this._createNode("span", {
            class: "ruleview-shape-point",
          }, coord);
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
          let node = this._createNode("span", {
            class: "ruleview-shape-point",
          }, coord);
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
        let node = this._createNode("span", {
          class: "ruleview-shape-point",
        }, coord);
        nodes.push(node);
      }
    }

    // insetPoints contains the 4 different possible inset points in the order they are
    // defined. By taking the modulo of the index in insetPoints with the number of nodes,
    // we can get which node represents each point (e.g. if there is only 1 node, it
    // represents all 4 points). The exception is "left" when there are 3 nodes. In that
    // case, it is nodes[1] that represents the left point rather than nodes[0].
    for (let j = 0; j < 4; j++) {
      let point = insetPoints[j];
      let nodeIndex = (point === "left" && nodes.length === 3) ? 1 : j % nodes.length;
      nodes[nodeIndex].classList.add(point);
    }

    nodes.forEach((node, j, array) => {
      for (let text of otherText[j]) {
        appendText(container, text);
      }
      container.appendChild(node);
    });

    // Add text that comes after the last node, if any exists
    if (otherText[nodes.length]) {
      for (let text of otherText[nodes.length]) {
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
  _appendAngle: function (angle, options) {
    let angleObj = new angleUtils.CssAngle(angle);
    let container = this._createNode("span", {
      "data-angle": angle
    });

    if (options.angleSwatchClass) {
      let swatch = this._createNode("span", {
        class: options.angleSwatchClass
      });
      this.angleSwatches.set(swatch, angleObj);
      swatch.addEventListener("mousedown", this._onAngleSwatchMouseDown);

      // Add click listener to stop event propagation when shift key is pressed
      // in order to prevent the value input to be focused.
      // Bug 711942 will add a tooltip to edit angle values and we should
      // be able to move this listener to Tooltip.js when it'll be implemented.
      swatch.addEventListener("click", function (event) {
        if (event.shiftKey) {
          event.stopPropagation();
        }
      });
      EventEmitter.decorate(swatch);
      container.appendChild(swatch);
    }

    let value = this._createNode("span", {
      class: options.angleClass
    }, angle);

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
  _cssPropertySupportsValue: function (name, value) {
    return this.isValidOnClient(name, value, this.doc);
  },

  /**
   * Tests if a given colorObject output by CssColor is valid for parsing.
   * Valid means it's really a color, not any of the CssColor SPECIAL_VALUES
   * except transparent
   */
  _isValidColor: function (colorObj) {
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
  _appendColor: function (color, options = {}) {
    let colorObj = new colorUtils.CssColor(color, this.cssColor4);

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
        swatch.addEventListener("mousedown", this._onColorSwatchMouseDown);
        EventEmitter.decorate(swatch);
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
  _wrapFilter: function (filters, options, nodes) {
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

  _onColorSwatchMouseDown: function (event) {
    if (!event.shiftKey) {
      return;
    }

    // Prevent click event to be fired to not show the tooltip
    event.stopPropagation();

    let swatch = event.target;
    let color = this.colorSwatches.get(swatch);
    let val = color.nextColorUnit();

    swatch.nextElementSibling.textContent = val;
    swatch.emit("unit-change", val);
  },

  _onAngleSwatchMouseDown: function (event) {
    if (!event.shiftKey) {
      return;
    }

    event.stopPropagation();

    let swatch = event.target;
    let angle = this.angleSwatches.get(swatch);
    let val = angle.nextAngleUnit();

    swatch.nextElementSibling.textContent = val;
    swatch.emit("unit-change", val);
  },

  /**
   * A helper function that sanitizes a possibly-unterminated URL.
   */
  _sanitizeURL: function (url) {
    // Re-lex the URL and add any needed termination characters.
    let urlTokenizer = getCSSLexer(url);
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
  _appendURL: function (match, url, options) {
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
        try {
          href = new URL(url, options.baseURI).href;
        } catch (e) {
          // Ignore.
        }
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
  _createNode: function (tagName, attributes, value = "") {
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
  _appendNode: function (tagName, attributes, value = "") {
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
  _appendTextNode: function (text) {
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
  _toDOM: function () {
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
   *           - gridClass: ""          // The class to use for the grid icon.
   *           - shapeClass: ""         // The class to use for the shape icon.
   *           - supportsColor: false   // Does the CSS property support colors?
   *           - urlClass: ""           // The class to be used for url() links.
   *           - baseURI: undefined     // A string used to resolve
   *                                    // relative links.
   * @return {Object}
   *         Overridden options object
   */
  _mergeOptions: function (overrides) {
    let defaults = {
      defaultColorType: true,
      angleClass: "",
      angleSwatchClass: "",
      bezierClass: "",
      bezierSwatchClass: "",
      colorClass: "",
      colorSwatchClass: "",
      filterSwatch: false,
      gridClass: "",
      shapeClass: "",
      supportsColor: false,
      urlClass: "",
      baseURI: undefined,
    };

    for (let item in overrides) {
      defaults[item] = overrides[item];
    }
    return defaults;
  }
};

module.exports = OutputParser;
