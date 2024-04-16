/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file holds various CSS parsing and rewriting utilities.
// Some entry points of note are:
// parseDeclarations - parse a CSS rule into declarations
// parsePseudoClassesAndAttributes - parse selector and extract
//     pseudo-classes
// parseSingleValue - parse a single CSS property value

"use strict";

const { getCSSLexer } = require("resource://devtools/shared/css/lexer.js");

loader.lazyRequireGetter(
  this,
  "CSS_ANGLEUNIT",
  "resource://devtools/shared/css/constants.js",
  true
);

const SELECTOR_ATTRIBUTE = (exports.SELECTOR_ATTRIBUTE = 1);
const SELECTOR_ELEMENT = (exports.SELECTOR_ELEMENT = 2);
const SELECTOR_PSEUDO_CLASS = (exports.SELECTOR_PSEUDO_CLASS = 3);
const CSS_BLOCKS = { "(": ")", "[": "]" };

// When commenting out a declaration, we put this character into the
// comment opener so that future parses of the commented text know to
// bypass the property name validity heuristic.
const COMMENT_PARSING_HEURISTIC_BYPASS_CHAR =
  (exports.COMMENT_PARSING_HEURISTIC_BYPASS_CHAR = "!");

/**
 * A generator function that lexes a CSS source string, yielding the
 * CSS tokens.  Comment tokens are dropped.
 *
 * @param {String} CSS source string
 * @yield {CSSToken} The next CSSToken that is lexed
 * @see CSSToken for details about the returned tokens
 */
function* cssTokenizer(string) {
  const lexer = getCSSLexer(string);
  while (true) {
    const token = lexer.nextToken();
    if (!token) {
      break;
    }
    // None of the existing consumers want comments.
    if (token.tokenType !== "comment") {
      yield token;
    }
  }
}

/**
 * Pass |string| to the CSS lexer and return an array of all the
 * returned tokens.  Comment tokens are not included.  In addition to
 * the usual information, each token will have starting and ending
 * line and column information attached.  Specifically, each token
 * has an additional "loc" attribute.  This attribute is an object
 * of the form {line: L, column: C}.  Lines and columns are both zero
 * based.
 *
 * It's best not to add new uses of this function.  In general it is
 * simpler and better to use the CSSToken offsets, rather than line
 * and column.  Also, this function lexes the entire input string at
 * once, rather than lazily yielding a token stream.  Use
 * |cssTokenizer| or |getCSSLexer| instead.
 *
 * @param{String} string The input string.
 * @return {Array} An array of tokens (@see CSSToken) that have
 *        line and column information.
 */
function cssTokenizerWithLineColumn(string) {
  const lexer = getCSSLexer(string);
  const result = [];
  let prevToken = undefined;
  while (true) {
    const token = lexer.nextToken();
    const lineNumber = lexer.lineNumber;
    const columnNumber = lexer.columnNumber;

    if (prevToken) {
      prevToken.loc.end = {
        line: lineNumber,
        column: columnNumber,
      };
    }

    if (!token) {
      break;
    }

    if (token.tokenType === "comment") {
      // We've already dealt with the previous token's location.
      prevToken = undefined;
    } else {
      const startLoc = {
        line: lineNumber,
        column: columnNumber,
      };
      token.loc = { start: startLoc };

      result.push(token);
      prevToken = token;
    }
  }

  return result;
}

/**
 * Escape a comment body.  Find the comment start and end strings in a
 * string and inserts backslashes so that the resulting text can
 * itself be put inside a comment.
 *
 * @param {String} inputString
 *                 input string
 * @return {String} the escaped result
 */
function escapeCSSComment(inputString) {
  const result = inputString.replace(/\/(\\*)\*/g, "/\\$1*");
  return result.replace(/\*(\\*)\//g, "*\\$1/");
}

/**
 * Un-escape a comment body.  This undoes any comment escaping that
 * was done by escapeCSSComment.  That is, given input like "/\*
 * comment *\/", it will strip the backslashes.
 *
 * @param {String} inputString
 *                 input string
 * @return {String} the un-escaped result
 */
function unescapeCSSComment(inputString) {
  const result = inputString.replace(/\/\\(\\*)\*/g, "/$1*");
  return result.replace(/\*\\(\\*)\//g, "*$1/");
}

/**
 * A helper function for @see parseDeclarations that handles parsing
 * of comment text.  This wraps a recursive call to parseDeclarations
 * with the processing needed to ensure that offsets in the result
 * refer back to the original, unescaped, input string.
 *
 * @param {Function} isCssPropertyKnown
 *        A function to check if the CSS property is known. This is either an
 *        internal server function or from the CssPropertiesFront.
 * @param {String} commentText The text of the comment, without the
 *                             delimiters.
 * @param {Number} startOffset The offset of the comment opener
 *                             in the original text.
 * @param {Number} endOffset The offset of the comment closer
 *                           in the original text.
 * @return {array} Array of declarations of the same form as returned
 *                 by parseDeclarations.
 */
function parseCommentDeclarations(
  isCssPropertyKnown,
  commentText,
  startOffset,
  endOffset
) {
  let commentOverride = false;
  if (commentText === "") {
    return [];
  } else if (commentText[0] === COMMENT_PARSING_HEURISTIC_BYPASS_CHAR) {
    // This is the special sign that the comment was written by
    // rewriteDeclarations and so we should bypass the usual
    // heuristic.
    commentOverride = true;
    commentText = commentText.substring(1);
  }

  const rewrittenText = unescapeCSSComment(commentText);

  // We might have rewritten an embedded comment.  For example
  // /\* ... *\/ would turn into /* ... */.
  // This rewriting is necessary for proper lexing, but it means
  // that the offsets we get back can be off.  So now we compute
  // a map so that we can rewrite offsets later.  The map is the same
  // length as |rewrittenText| and tells us how to map an index
  // into |rewrittenText| to an index into |commentText|.
  //
  // First, we find the location of each comment starter or closer in
  // |rewrittenText|.  At these spots we put a 1 into |rewrites|.
  // Then we walk the array again, using the elements to compute a
  // delta, which we use to make the final mapping.
  //
  // Note we allocate one extra entry because we can see an ending
  // offset that is equal to the length.
  const rewrites = new Array(rewrittenText.length + 1).fill(0);

  const commentRe = /\/\\*\*|\*\\*\//g;
  while (true) {
    const matchData = commentRe.exec(rewrittenText);
    if (!matchData) {
      break;
    }
    rewrites[matchData.index] = 1;
  }

  let delta = 0;
  for (let i = 0; i <= rewrittenText.length; ++i) {
    delta += rewrites[i];
    // |startOffset| to add the offset from the comment starter, |+2|
    // for the length of the "/*", then |i| and |delta| as described
    // above.
    rewrites[i] = startOffset + 2 + i + delta;
    if (commentOverride) {
      ++rewrites[i];
    }
  }

  // Note that we pass "false" for parseComments here.  It doesn't
  // seem worthwhile to support declarations in comments-in-comments
  // here, as there's no way to generate those using the tools, and
  // users would be crazy to write such things.
  const newDecls = parseDeclarationsInternal(
    isCssPropertyKnown,
    rewrittenText,
    false,
    true,
    commentOverride
  );
  for (const decl of newDecls) {
    decl.offsets[0] = rewrites[decl.offsets[0]];
    decl.offsets[1] = rewrites[decl.offsets[1]];
    decl.colonOffsets[0] = rewrites[decl.colonOffsets[0]];
    decl.colonOffsets[1] = rewrites[decl.colonOffsets[1]];
    decl.commentOffsets = [startOffset, endOffset];
  }
  return newDecls;
}

/**
 * A helper function for parseDeclarationsInternal that creates a new
 * empty declaration.
 *
 * @return {object} an empty declaration of the form returned by
 *                  parseDeclarations
 */
function getEmptyDeclaration() {
  return {
    name: "",
    value: "",
    priority: "",
    terminator: "",
    offsets: [undefined, undefined],
    colonOffsets: false,
  };
}

/**
 * Like trim, but only trims CSS-allowed whitespace.
 */
function cssTrim(str) {
  const match = /^[ \t\r\n\f]*(.*?)[ \t\r\n\f]*$/.exec(str);
  if (match) {
    return match[1];
  }
  return str;
}

/**
 * A helper function that does all the parsing work for
 * parseDeclarations.  This is separate because it has some arguments
 * that don't make sense in isolation.
 *
 * The return value and arguments are like parseDeclarations, with
 * these additional arguments.
 *
 * @param {Function} isCssPropertyKnown
 *        Function to check if the CSS property is known.
 * @param {Boolean} inComment
 *        If true, assume that this call is parsing some text
 *        which came from a comment in another declaration.
 *        In this case some heuristics are used to avoid parsing
 *        text which isn't obviously a series of declarations.
 * @param {Boolean} commentOverride
 *        This only makes sense when inComment=true.
 *        When true, assume that the comment was generated by
 *        rewriteDeclarations, and skip the usual name-checking
 *        heuristic.
 */
// eslint-disable-next-line complexity
function parseDeclarationsInternal(
  isCssPropertyKnown,
  inputString,
  parseComments,
  inComment,
  commentOverride
) {
  if (inputString === null || inputString === undefined) {
    throw new Error("empty input string");
  }

  const lexer = getCSSLexer(inputString);

  let declarations = [getEmptyDeclaration()];
  let lastProp = declarations[0];

  // This tracks the various CSS blocks the current token is in currently.
  // This is a stack we push to when a block is opened, and we pop from when a block is
  // closed. Within a block, colons and semicolons don't advance the way they do outside
  // of blocks.
  let currentBlocks = [];

  // This tracks the "!important" parsing state.  The states are:
  // 0 - haven't seen anything
  // 1 - have seen "!", looking for "important" next (possibly after
  //     whitespace).
  // 2 - have seen "!important"
  let importantState = 0;
  // This is true if we saw whitespace or comments between the "!" and
  // the "important".
  let importantWS = false;

  // This tracks the nesting parsing state
  let isInNested = false;
  let nestingLevel = 0;

  let current = "";

  const resetStateForNextDeclaration = () => {
    current = "";
    currentBlocks = [];
    importantState = 0;
    importantWS = false;
    declarations.push(getEmptyDeclaration());
    lastProp = declarations.at(-1);
  };

  while (true) {
    const token = lexer.nextToken();
    if (!token) {
      break;
    }

    // Update the start and end offsets of the declaration, but only
    // when we see a significant token.
    if (token.tokenType !== "whitespace" && token.tokenType !== "comment") {
      if (lastProp.offsets[0] === undefined) {
        lastProp.offsets[0] = token.startOffset;
      }
      lastProp.offsets[1] = token.endOffset;
    } else if (
      lastProp.name &&
      !current &&
      !importantState &&
      !lastProp.priority &&
      lastProp.colonOffsets[1]
    ) {
      // Whitespace appearing after the ":" is attributed to it.
      lastProp.colonOffsets[1] = token.endOffset;
    } else if (importantState === 1) {
      importantWS = true;
    }

    if (
      // If we're not already in a nested rule
      !isInNested &&
      token.tokenType === "symbol" &&
      // and there's an opening curly bracket
      token.text == "{" &&
      // and we're not inside a function or an attribute
      !currentBlocks.length
    ) {
      // Assume we're encountering a nested rule.
      isInNested = true;
      nestingLevel = 1;

      continue;
    } else if (isInNested) {
      if (token.tokenType === "symbol") {
        if (token.text == "{") {
          nestingLevel++;
        }
        if (token.text == "}") {
          nestingLevel--;
        }
      }

      // If we were in a nested rule, and we saw the last closing curly bracket,
      // reset the state to parse possible declarations declared after the nested rule.
      if (nestingLevel === 0) {
        isInNested = false;
        // We need to remove the previous pending declaration and reset the state
        declarations.pop();
        resetStateForNextDeclaration();
      }
      continue;
    } else if (
      token.tokenType === "symbol" &&
      CSS_BLOCKS[currentBlocks.at(-1)] === token.text
    ) {
      // Closing the last block that was opened.
      currentBlocks.pop();
      current += token.text;
    } else if (token.tokenType === "symbol" && CSS_BLOCKS[token.text]) {
      // Opening a new block.
      currentBlocks.push(token.text);
      current += token.text;
    } else if (token.tokenType === "function") {
      // Opening a function is like opening a new block, so push one to the stack.
      currentBlocks.push("(");
      current += token.text + "(";
    } else if (token.tokenType === "symbol" && token.text === ":") {
      // Either way, a "!important" we've seen is no longer valid now.
      importantState = 0;
      importantWS = false;
      if (!lastProp.name) {
        // Set the current declaration name if there's no name yet
        lastProp.name = cssTrim(current);
        lastProp.colonOffsets = [token.startOffset, token.endOffset];
        current = "";
        currentBlocks = [];

        // When parsing a comment body, if the left-hand-side is not a
        // valid property name, then drop it and stop parsing.
        if (
          inComment &&
          !commentOverride &&
          !isCssPropertyKnown(lastProp.name)
        ) {
          lastProp.name = null;
          break;
        }
      } else {
        // Otherwise, just append ':' to the current value (declaration value
        // with colons)
        current += ":";
      }
    } else if (
      token.tokenType === "symbol" &&
      token.text === ";" &&
      !currentBlocks.length
    ) {
      lastProp.terminator = "";
      // When parsing a comment, if the name hasn't been set, then we
      // have probably just seen an ordinary semicolon used in text,
      // so drop this and stop parsing.
      if (inComment && !lastProp.name) {
        current = "";
        currentBlocks = [];
        break;
      }
      if (importantState === 2) {
        lastProp.priority = "important";
      } else if (importantState === 1) {
        current += "!";
        if (importantWS) {
          current += " ";
        }
      }
      lastProp.value = cssTrim(current);
      resetStateForNextDeclaration();
    } else if (token.tokenType === "ident") {
      if (token.text === "important" && importantState === 1) {
        importantState = 2;
      } else {
        if (importantState > 0) {
          current += "!";
          if (importantWS) {
            current += " ";
          }
          if (importantState === 2) {
            current += "important ";
          }
          importantState = 0;
          importantWS = false;
        }
        // Re-escape the token to avoid dequoting problems.
        // See bug 1287620.
        current += CSS.escape(token.text);
      }
    } else if (token.tokenType === "symbol" && token.text === "!") {
      importantState = 1;
    } else if (token.tokenType === "whitespace") {
      if (current !== "") {
        current = current.trimEnd() + " ";
      }
    } else if (token.tokenType === "comment") {
      if (parseComments && !lastProp.name && !lastProp.value) {
        const commentText = inputString.substring(
          token.startOffset + 2,
          token.endOffset - 2
        );
        const newDecls = parseCommentDeclarations(
          isCssPropertyKnown,
          commentText,
          token.startOffset,
          token.endOffset
        );

        // Insert the new declarations just before the final element.
        const lastDecl = declarations.pop();
        declarations = [...declarations, ...newDecls, lastDecl];
      } else {
        current = current.trimEnd() + " ";
      }
    } else {
      if (importantState > 0) {
        current += "!";
        if (importantWS) {
          current += " ";
        }
        if (importantState === 2) {
          current += "important ";
        }
        importantState = 0;
        importantWS = false;
      }
      current += inputString.substring(token.startOffset, token.endOffset);
    }
  }

  // Handle whatever trailing properties or values might still be there
  if (current) {
    // If nested rule doesn't have closing bracket
    if (isInNested && nestingLevel > 0) {
      // We need to remove the previous (nested) pending declaration
      declarations.pop();
    } else if (!lastProp.name) {
      // Ignore this case in comments.
      if (!inComment) {
        // Trailing property found, e.g. p1:v1;p2:v2;p3
        lastProp.name = cssTrim(current);
      }
    } else {
      // Trailing value found, i.e. value without an ending ;
      if (importantState === 2) {
        lastProp.priority = "important";
      } else if (importantState === 1) {
        current += "!";
      }
      lastProp.value = cssTrim(current);
      const terminator = lexer.performEOFFixup("", true);
      lastProp.terminator = terminator + ";";
      // If the input was unterminated, attribute the remainder to
      // this property.  This avoids some bad behavior when rewriting
      // an unterminated comment.
      if (terminator) {
        lastProp.offsets[1] = inputString.length;
      }
    }
  }

  // Remove declarations that have neither a name nor a value
  declarations = declarations.filter(prop => prop.name || prop.value);

  return declarations;
}

/**
 * Returns an array of CSS declarations given a string.
 * For example, parseDeclarations(isCssPropertyKnown, "width: 1px; height: 1px")
 * would return:
 * [{name:"width", value: "1px"}, {name: "height", "value": "1px"}]
 *
 * The input string is assumed to only contain declarations so { and }
 * characters will be treated as part of either the property or value,
 * depending where it's found.
 *
 * @param {Function} isCssPropertyKnown
 *        A function to check if the CSS property is known. This is either an
 *        internal server function or from the CssPropertiesFront.
 *        that are supported by the server.
 * @param {String} inputString
 *        An input string of CSS
 * @param {Boolean} parseComments
 *        If true, try to parse the contents of comments as well.
 *        A comment will only be parsed if it occurs outside of
 *        the body of some other declaration.
 * @return {Array} an array of objects with the following signature:
 *         [{"name": string, "value": string, "priority": string,
 *           "terminator": string,
 *           "offsets": [start, end], "colonOffsets": [start, end]},
 *          ...]
 *         Here, "offsets" holds the offsets of the start and end
 *         of the declaration text, in a form suitable for use with
 *         String.substring.
 *         "terminator" is a string to use to terminate the declaration,
 *         usually "" to mean no additional termination is needed.
 *         "colonOffsets" holds the start and end locations of the
 *         ":" that separates the property name from the value.
 *         If the declaration appears in a comment, then there will
 *         be an additional {"commentOffsets": [start, end] property
 *         on the object, which will hold the offsets of the start
 *         and end of the enclosing comment.
 */
function parseDeclarations(
  isCssPropertyKnown,
  inputString,
  parseComments = false
) {
  return parseDeclarationsInternal(
    isCssPropertyKnown,
    inputString,
    parseComments,
    false,
    false
  );
}

/**
 * Like @see parseDeclarations, but removes properties that do not
 * have a name.
 */
function parseNamedDeclarations(
  isCssPropertyKnown,
  inputString,
  parseComments = false
) {
  return parseDeclarations(
    isCssPropertyKnown,
    inputString,
    parseComments
  ).filter(item => !!item.name);
}

/**
 * Returns an array of the parsed CSS selector value and type given a string.
 *
 * The components making up the CSS selector can be extracted into 3 different
 * types: element, attribute and pseudoclass. The object that is appended to
 * the returned array contains the value related to one of the 3 types described
 * along with the actual type.
 *
 * The following are the 3 types that can be returned in the object signature:
 * (1) SELECTOR_ATTRIBUTE
 * (2) SELECTOR_ELEMENT
 * (3) SELECTOR_PSEUDO_CLASS
 *
 * @param {String} value
 *        The CSS selector text.
 * @return {Array} an array of objects with the following signature:
 *         [{ "value": string, "type": integer }, ...]
 */
// eslint-disable-next-line complexity
function parsePseudoClassesAndAttributes(value) {
  if (!value) {
    throw new Error("empty input string");
  }

  const tokens = cssTokenizer(value);
  const result = [];
  let current = "";
  let functionCount = 0;
  let hasAttribute = false;
  let hasColon = false;

  for (const token of tokens) {
    if (token.tokenType === "ident") {
      current += value.substring(token.startOffset, token.endOffset);

      if (hasColon && !functionCount) {
        if (current) {
          result.push({ value: current, type: SELECTOR_PSEUDO_CLASS });
        }

        current = "";
        hasColon = false;
      }
    } else if (token.tokenType === "symbol" && token.text === ":") {
      if (!hasColon) {
        if (current) {
          result.push({ value: current, type: SELECTOR_ELEMENT });
        }

        current = "";
        hasColon = true;
      }

      current += token.text;
    } else if (token.tokenType === "function") {
      current += value.substring(token.startOffset, token.endOffset);
      functionCount++;
    } else if (token.tokenType === "symbol" && token.text === ")") {
      current += token.text;

      if (hasColon && functionCount == 1) {
        if (current) {
          result.push({ value: current, type: SELECTOR_PSEUDO_CLASS });
        }

        current = "";
        functionCount--;
        hasColon = false;
      } else {
        functionCount--;
      }
    } else if (token.tokenType === "symbol" && token.text === "[") {
      if (!hasAttribute && !functionCount) {
        if (current) {
          result.push({ value: current, type: SELECTOR_ELEMENT });
        }

        current = "";
        hasAttribute = true;
      }

      current += token.text;
    } else if (token.tokenType === "symbol" && token.text === "]") {
      current += token.text;

      if (hasAttribute && !functionCount) {
        if (current) {
          result.push({ value: current, type: SELECTOR_ATTRIBUTE });
        }

        current = "";
        hasAttribute = false;
      }
    } else {
      current += value.substring(token.startOffset, token.endOffset);
    }
  }

  if (current) {
    result.push({ value: current, type: SELECTOR_ELEMENT });
  }

  return result;
}

/**
 * Expects a single CSS value to be passed as the input and parses the value
 * and priority.
 *
 * @param {Function} isCssPropertyKnown
 *        A function to check if the CSS property is known. This is either an
 *        internal server function or from the CssPropertiesFront.
 *        that are supported by the server.
 * @param {String} value
 *        The value from the text editor.
 * @return {Object} an object with 'value' and 'priority' properties.
 */
function parseSingleValue(isCssPropertyKnown, value) {
  const declaration = parseDeclarations(
    isCssPropertyKnown,
    "a: " + value + ";"
  )[0];
  return {
    value: declaration ? declaration.value : "",
    priority: declaration ? declaration.priority : "",
  };
}

/**
 * Convert an angle value to degree.
 *
 * @param {Number} angleValue The angle value.
 * @param {CSS_ANGLEUNIT} angleUnit The angleValue's angle unit.
 * @return {Number} An angle value in degree.
 */
function getAngleValueInDegrees(angleValue, angleUnit) {
  switch (angleUnit) {
    case CSS_ANGLEUNIT.deg:
      return angleValue;
    case CSS_ANGLEUNIT.grad:
      return angleValue * 0.9;
    case CSS_ANGLEUNIT.rad:
      return (angleValue * 180) / Math.PI;
    case CSS_ANGLEUNIT.turn:
      return angleValue * 360;
    default:
      throw new Error("No matched angle unit.");
  }
}

exports.cssTokenizer = cssTokenizer;
exports.cssTokenizerWithLineColumn = cssTokenizerWithLineColumn;
exports.escapeCSSComment = escapeCSSComment;
exports.unescapeCSSComment = unescapeCSSComment;
exports.parseDeclarations = parseDeclarations;
exports.parseNamedDeclarations = parseNamedDeclarations;
// parseCommentDeclarations is exported for testing.
exports._parseCommentDeclarations = parseCommentDeclarations;
exports.parsePseudoClassesAndAttributes = parsePseudoClassesAndAttributes;
exports.parseSingleValue = parseSingleValue;
exports.getAngleValueInDegrees = getAngleValueInDegrees;
