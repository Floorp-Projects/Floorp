/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { getCSSLexer } = require("devtools/shared/css/lexer");

const XHTML_NS = "http://www.w3.org/1999/xhtml";
const FONT_PREVIEW_TEXT = "Abc";
const FONT_PREVIEW_FONT_SIZE = 40;
const FONT_PREVIEW_FILLSTYLE = "black";
// Offset (in px) to avoid cutting off text edges of italic fonts.
const FONT_PREVIEW_OFFSET = 4;
// Factor used to resize the canvas in order to get better text quality.
const FONT_PREVIEW_OVERSAMPLING_FACTOR = 2;

/**
 * Helper function for getting an image preview of the given font.
 *
 * @param font {string}
 *        Name of font to preview
 * @param doc {Document}
 *        Document to use to render font
 * @param options {object}
 *        Object with options 'previewText' and 'previewFontSize'
 *
 * @return dataUrl
 *         The data URI of the font preview image
 */
function getFontPreviewData(font, doc, options) {
  options = options || {};
  const previewText = options.previewText || FONT_PREVIEW_TEXT;
  const previewTextLines = previewText.split("\n");
  const previewFontSize = options.previewFontSize || FONT_PREVIEW_FONT_SIZE;
  const fillStyle = options.fillStyle || FONT_PREVIEW_FILLSTYLE;
  const fontStyle = options.fontStyle || "";

  const canvas = doc.createElementNS(XHTML_NS, "canvas");
  const ctx = canvas.getContext("2d");
  const fontValue =
    fontStyle + " " + previewFontSize + "px " + font + ", serif";

  // Get the correct preview text measurements and set the canvas dimensions
  ctx.font = fontValue;
  ctx.fillStyle = fillStyle;
  const previewTextLinesWidths = previewTextLines.map(
    previewTextLine => ctx.measureText(previewTextLine).width
  );
  const textWidth = Math.round(Math.max(...previewTextLinesWidths));

  // The canvas width is calculated as the width of the longest line plus
  // an offset at the left and right of it.
  // The canvas height is calculated as the font size multiplied by the
  // number of lines plus an offset at the top and bottom.
  //
  // In order to get better text quality, we oversample the canvas.
  // That means, after the width and height are calculated, we increase
  // both sizes by some factor.
  const simpleCanvasWidth = textWidth + FONT_PREVIEW_OFFSET * 2;
  canvas.width = simpleCanvasWidth * FONT_PREVIEW_OVERSAMPLING_FACTOR;
  canvas.height =
    (previewFontSize * previewTextLines.length + FONT_PREVIEW_OFFSET * 2) *
    FONT_PREVIEW_OVERSAMPLING_FACTOR;

  // we have to reset these after changing the canvas size
  ctx.font = fontValue;
  ctx.fillStyle = fillStyle;

  // Oversample the canvas for better text quality
  ctx.scale(FONT_PREVIEW_OVERSAMPLING_FACTOR, FONT_PREVIEW_OVERSAMPLING_FACTOR);

  ctx.textBaseline = "top";
  ctx.textAlign = "center";
  const horizontalTextPosition = simpleCanvasWidth / 2;
  let verticalTextPosition = FONT_PREVIEW_OFFSET;
  for (let i = 0; i < previewTextLines.length; i++) {
    ctx.fillText(
      previewTextLines[i],
      horizontalTextPosition,
      verticalTextPosition
    );

    // Move vertical text position one line down
    verticalTextPosition += previewFontSize;
  }

  const dataURL = canvas.toDataURL("image/png");

  return {
    dataURL: dataURL,
    size: textWidth + FONT_PREVIEW_OFFSET * 2,
  };
}

exports.getFontPreviewData = getFontPreviewData;

/**
 * Get the text content of a rule given some CSS text, a line and a column
 * Consider the following example:
 * body {
 *  color: red;
 * }
 * p {
 *  line-height: 2em;
 *  color: blue;
 * }
 * Calling the function with the whole text above and line=4 and column=1 would
 * return "line-height: 2em; color: blue;"
 * @param {String} initialText
 * @param {Number} line (1-indexed)
 * @param {Number} column (1-indexed)
 * @return {object} An object of the form {offset: number, text: string}
 *                  The offset is the index into the input string where
 *                  the rule text started.  The text is the content of
 *                  the rule.
 */
function getRuleText(initialText, line, column) {
  if (typeof line === "undefined" || typeof column === "undefined") {
    throw new Error("Location information is missing");
  }

  const { offset: textOffset, text } = getTextAtLineColumn(
    initialText,
    line,
    column
  );
  const lexer = getCSSLexer(text);

  // Search forward for the opening brace.
  while (true) {
    const token = lexer.nextToken();
    if (!token) {
      throw new Error("couldn't find start of the rule");
    }
    if (token.tokenType === "symbol" && token.text === "{") {
      break;
    }
  }

  // Now collect text until we see the matching close brace.
  let braceDepth = 1;
  let startOffset, endOffset;
  while (true) {
    const token = lexer.nextToken();
    if (!token) {
      break;
    }
    if (startOffset === undefined) {
      startOffset = token.startOffset;
    }
    if (token.tokenType === "symbol") {
      if (token.text === "{") {
        ++braceDepth;
      } else if (token.text === "}") {
        --braceDepth;
        if (braceDepth == 0) {
          break;
        }
      }
    }
    endOffset = token.endOffset;
  }

  // If the rule was of the form "selector {" with no closing brace
  // and no properties, just return an empty string.
  if (startOffset === undefined) {
    return { offset: 0, text: "" };
  }
  // If the input didn't have any tokens between the braces (e.g.,
  // "div {}"), then the endOffset won't have been set yet; so account
  // for that here.
  if (endOffset === undefined) {
    endOffset = startOffset;
  }

  // Note that this approach will preserve comments, despite the fact
  // that cssTokenizer skips them.
  return {
    offset: textOffset + startOffset,
    text: text.substring(startOffset, endOffset),
  };
}

exports.getRuleText = getRuleText;

/**
 * Return the offset and substring of |text| that starts at the given
 * line and column.
 * @param {String} text
 * @param {Number} line (1-indexed)
 * @param {Number} column (1-indexed)
 * @return {object} An object of the form {offset: number, text: string},
 *                  where the offset is the offset into the input string
 *                  where the text starts, and where text is the text.
 */
function getTextAtLineColumn(text, line, column) {
  let offset;
  if (line > 1) {
    const rx = new RegExp(
      "(?:[^\\r\\n\\f]*(?:\\r\\n|\\n|\\r|\\f)){" + (line - 1) + "}"
    );
    offset = rx.exec(text)[0].length;
  } else {
    offset = 0;
  }
  offset += column - 1;
  return { offset: offset, text: text.substr(offset) };
}

exports.getTextAtLineColumn = getTextAtLineColumn;
