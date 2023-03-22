/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/* eslint-disable complexity */

var acorn = require("acorn");
var sourceMap = require("source-map");
const NEWLINE_CODE = 10;

// If any of these tokens are seen before a "[" token, we know that "[" token
// is the start of an array literal, rather than a property access.
//
// The only exception is "}", which would need to be disambiguated by
// parsing. The majority of the time, an open bracket following a closing
// curly is going to be an array literal, so we brush the complication under
// the rug, and handle the ambiguity by always assuming that it will be an
// array literal.
const PRE_ARRAY_LITERAL_TOKENS = new Set([
  "typeof",
  "void",
  "delete",
  "case",
  "do",
  "=",
  "in",
  "{",
  "*",
  "/",
  "%",
  "else",
  ";",
  "++",
  "--",
  "+",
  "-",
  "~",
  "!",
  ":",
  "?",
  ">>",
  ">>>",
  "<<",
  "||",
  "&&",
  "<",
  ">",
  "<=",
  ">=",
  "instanceof",
  "&",
  "^",
  "|",
  "==",
  "!=",
  "===",
  "!==",
  ",",
  "}",
]);

/**
 * Determines if we think that the given token starts an array literal.
 *
 * @param Object token
 *        The token we want to determine if it is an array literal.
 * @param Object lastToken
 *        The last token we added to the pretty printed results.
 *
 * @returns Boolean
 *          True if we believe it is an array literal, false otherwise.
 */
function isArrayLiteral(token, lastToken) {
  if (token.type.label != "[") {
    return false;
  }
  if (!lastToken) {
    return true;
  }
  if (lastToken.type.isAssign) {
    return true;
  }
  return PRE_ARRAY_LITERAL_TOKENS.has(
    lastToken.type.keyword || lastToken.type.label
  );
}

// If any of these tokens are followed by a token on a new line, we know that
// ASI cannot happen.
const PREVENT_ASI_AFTER_TOKENS = new Set([
  // Binary operators
  "*",
  "/",
  "%",
  "+",
  "-",
  "<<",
  ">>",
  ">>>",
  "<",
  ">",
  "<=",
  ">=",
  "instanceof",
  "in",
  "==",
  "!=",
  "===",
  "!==",
  "&",
  "^",
  "|",
  "&&",
  "||",
  ",",
  ".",
  "=",
  "*=",
  "/=",
  "%=",
  "+=",
  "-=",
  "<<=",
  ">>=",
  ">>>=",
  "&=",
  "^=",
  "|=",
  // Unary operators
  "delete",
  "void",
  "typeof",
  "~",
  "!",
  "new",
  // Function calls and grouped expressions
  "(",
]);

// If any of these tokens are on a line after the token before it, we know
// that ASI cannot happen.
const PREVENT_ASI_BEFORE_TOKENS = new Set([
  // Binary operators
  "*",
  "/",
  "%",
  "<<",
  ">>",
  ">>>",
  "<",
  ">",
  "<=",
  ">=",
  "instanceof",
  "in",
  "==",
  "!=",
  "===",
  "!==",
  "&",
  "^",
  "|",
  "&&",
  "||",
  ",",
  ".",
  "=",
  "*=",
  "/=",
  "%=",
  "+=",
  "-=",
  "<<=",
  ">>=",
  ">>>=",
  "&=",
  "^=",
  "|=",
  // Function calls
  "(",
]);

/**
 * Determine if a token can look like an identifier. More precisely,
 * this determines if the token may end or start with a character from
 * [A-Za-z0-9_].
 *
 * @param Object token
 *        The token we are looking at.
 *
 * @returns Boolean
 *          True if identifier-like.
 */
function isIdentifierLike(token) {
  const ttl = token.type.label;
  return (
    ttl == "name" || ttl == "num" || ttl == "privateId" || !!token.type.keyword
  );
}

/**
 * Determines if Automatic Semicolon Insertion (ASI) occurs between these
 * tokens.
 *
 * @param Object token
 *        The current token.
 * @param Object lastToken
 *        The last token we added to the pretty printed results.
 *
 * @returns Boolean
 *          True if we believe ASI occurs.
 */
function isASI(token, lastToken) {
  if (!lastToken) {
    return false;
  }
  if (token.loc.start.line === lastToken.loc.start.line) {
    return false;
  }
  if (
    lastToken.type.keyword == "return" ||
    lastToken.type.keyword == "yield" ||
    (lastToken.type.label == "name" && lastToken.value == "yield")
  ) {
    return true;
  }
  if (
    PREVENT_ASI_AFTER_TOKENS.has(lastToken.type.label || lastToken.type.keyword)
  ) {
    return false;
  }
  if (PREVENT_ASI_BEFORE_TOKENS.has(token.type.label || token.type.keyword)) {
    return false;
  }
  return true;
}

/**
 * Determine if we should add a newline after the given token.
 *
 * @param Object token
 *        The token we are looking at.
 * @param Array stack
 *        The stack of open parens/curlies/brackets/etc.
 *
 * @returns Boolean
 *          True if we should add a newline.
 */
function isLineDelimiter(token, stack) {
  if (token.isArrayLiteral) {
    return true;
  }
  const ttl = token.type.label;
  const top = stack.at(-1);
  return (
    (ttl == ";" && top != "(") ||
    ttl == "{" ||
    (ttl == "," && top != "(") ||
    (ttl == ":" && (top == "case" || top == "default"))
  );
}

/**
 * Append the necessary whitespace to the result after we have added the given
 * token.
 *
 * @param Object token
 *        The token that was just added to the result.
 * @param Function write
 *        The function to write to the pretty printed results.
 * @param Array stack
 *        The stack of open parens/curlies/brackets/etc.
 *
 * @returns Boolean
 *          Returns true if we added a newline to result, false in all other
 *          cases.
 */
function appendNewline(token, write, stack) {
  if (isLineDelimiter(token, stack)) {
    write("\n");
    return true;
  }
  return false;
}

/**
 * Determines if we need to add a space after the token we are about to add.
 *
 * @param Object token
 *        The token we are about to add to the pretty printed code.
 * @param Object [lastToken]
 *        Optional last token added to the pretty printed code.
 */
function needsSpaceAfter(token, lastToken) {
  if (lastToken && needsSpaceBetweenTokens(token, lastToken)) {
    return true;
  }

  if (token.type.isAssign) {
    return true;
  }
  if (token.type.binop != null && lastToken) {
    return true;
  }
  if (token.type.label == "?") {
    return true;
  }

  return false;
}

function needsSpaceBeforeLastToken(lastToken) {
  if (lastToken.type.isLoop) {
    return true;
  }
  if (lastToken.type.isAssign) {
    return true;
  }
  if (lastToken.type.binop != null) {
    return true;
  }

  const lastTokenTypeLabel = lastToken.type.label;
  if (lastTokenTypeLabel == "?") {
    return true;
  }
  if (lastTokenTypeLabel == ":") {
    return true;
  }
  if (lastTokenTypeLabel == ",") {
    return true;
  }
  if (lastTokenTypeLabel == ";") {
    return true;
  }
  if (lastTokenTypeLabel == "${") {
    return true;
  }
  return false;
}

function isBreakContinueOrReturnStatement(lastTokenKeyword) {
  return (
    lastTokenKeyword == "break" ||
    lastTokenKeyword == "continue" ||
    lastTokenKeyword == "return"
  );
}

function needsSpaceBeforeLastTokenKeywordAfterNotDot(lastTokenKeyword) {
  return (
    lastTokenKeyword != "debugger" &&
    lastTokenKeyword != "null" &&
    lastTokenKeyword != "true" &&
    lastTokenKeyword != "false" &&
    lastTokenKeyword != "this" &&
    lastTokenKeyword != "default"
  );
}

function needsSpaceBeforeClosingParen(tokenTypeLabel) {
  return (
    tokenTypeLabel != ")" &&
    tokenTypeLabel != "]" &&
    tokenTypeLabel != ";" &&
    tokenTypeLabel != "," &&
    tokenTypeLabel != "."
  );
}

/**
 * Determines if we need to add a space between the last token we added and
 * the token we are about to add.
 *
 * @param Object token
 *        The token we are about to add to the pretty printed code.
 * @param Object lastToken
 *        The last token added to the pretty printed code.
 */
function needsSpaceBetweenTokens(token, lastToken) {
  if (needsSpaceBeforeLastToken(lastToken)) {
    return true;
  }

  const ltt = lastToken.type.label;
  if (ltt == "num" && token.type.label == ".") {
    return true;
  }

  const ltk = lastToken.type.keyword;
  const ttl = token.type.label;
  if (ltk != null && ttl != ".") {
    if (isBreakContinueOrReturnStatement(ltk)) {
      return ttl != ";";
    }
    if (needsSpaceBeforeLastTokenKeywordAfterNotDot(ltk)) {
      return true;
    }
  }

  if (ltt == ")" && needsSpaceBeforeClosingParen(ttl)) {
    return true;
  }

  if (isIdentifierLike(token) && isIdentifierLike(lastToken)) {
    // We must emit a space to avoid merging the tokens.
    return true;
  }

  if (token.type.label == "{" && lastToken.type.label == "name") {
    return true;
  }

  return false;
}

function needsSpaceBeforeClosingCurlyBracket(tokenTypeKeyword) {
  return (
    tokenTypeKeyword == "else" ||
    tokenTypeKeyword == "catch" ||
    tokenTypeKeyword == "finally"
  );
}

function needsLineBreakBeforeClosingCurlyBracket(tokenTypeLabel) {
  return (
    tokenTypeLabel != "(" &&
    tokenTypeLabel != ";" &&
    tokenTypeLabel != "," &&
    tokenTypeLabel != ")" &&
    tokenTypeLabel != "." &&
    tokenTypeLabel != "template" &&
    tokenTypeLabel != "`"
  );
}

/**
 * Add the required whitespace before this token, whether that is a single
 * space, newline, and/or the indent on fresh lines.
 *
 * @param Object token
 *        The token we are about to add to the pretty printed code.
 * @param Object lastToken
 *        The last token we added to the pretty printed code.
 * @param Boolean addedNewline
 *        Whether we added a newline after adding the last token to the pretty
 *        printed code.
 * @param Boolean addedSpace
 *        Whether we added a space after adding the last token to the pretty
 *        printed code. This only happens if an inline comment was printed
 *        since the last token.
 * @param Function write
 *        The function to write pretty printed code.
 * @param Object options
 *        The options object.
 * @param Number indentLevel
 *        The number of indents deep we are.
 * @param Array stack
 *        The stack of open curlies, brackets, etc.
 */
function prependWhiteSpace(
  token,
  lastToken,
  addedNewline,
  addedSpace,
  write,
  options,
  indentLevel,
  stack
) {
  const ttk = token.type.keyword;
  const ttl = token.type.label;
  let newlineAdded = addedNewline;
  let spaceAdded = addedSpace;
  const ltt = lastToken?.type?.label;

  // Handle whitespace and newlines after "}" here instead of in
  // `isLineDelimiter` because it is only a line delimiter some of the
  // time. For example, we don't want to put "else if" on a new line after
  // the first if's block.
  if (lastToken && ltt == "}") {
    if (
      (ttk == "while" && stack.at(-1) == "do") ||
      needsSpaceBeforeClosingCurlyBracket(ttk)
    ) {
      write(" ");
      spaceAdded = true;
    } else if (needsLineBreakBeforeClosingCurlyBracket(ttl)) {
      write("\n");
      newlineAdded = true;
    }
  }

  if (
    (ttl == ":" && stack.at(-1) == "?") ||
    (ttl == "}" && stack.at(-1) == "${")
  ) {
    write(" ");
    spaceAdded = true;
  }

  if (lastToken && ltt != "}" && ltt != "." && ttk == "else") {
    write(" ");
    spaceAdded = true;
  }

  function ensureNewline() {
    if (!newlineAdded) {
      write("\n");
      newlineAdded = true;
    }
  }

  if (isASI(token, lastToken)) {
    ensureNewline();
  }

  if (decrementsIndent(ttl, stack)) {
    ensureNewline();
  }

  if (newlineAdded) {
    if (ttk == "case" || ttk == "default") {
      write(options.indent.repeat(indentLevel - 1));
    } else {
      write(options.indent.repeat(indentLevel));
    }
  } else if (!spaceAdded && needsSpaceAfter(token, lastToken)) {
    write(" ");
    spaceAdded = true;
  }
}

const escapeCharacters = {
  // Backslash
  "\\": "\\\\",
  // Newlines
  "\n": "\\n",
  // Carriage return
  "\r": "\\r",
  // Tab
  "\t": "\\t",
  // Vertical tab
  "\v": "\\v",
  // Form feed
  "\f": "\\f",
  // Null character
  "\0": "\\x00",
  // Line separator
  "\u2028": "\\u2028",
  // Paragraph separator
  "\u2029": "\\u2029",
  // Single quotes
  "'": "\\'",
};

// eslint-disable-next-line prefer-template
const regExpString = "(" + Object.values(escapeCharacters).join("|") + ")";
const escapeCharactersRegExp = new RegExp(regExpString, "g");

function sanitizerReplaceFunc(_, c) {
  return escapeCharacters[c];
}

/**
 * Make sure that we output the escaped character combination inside string
 * literals instead of various problematic characters.
 */
function sanitize(str) {
  return str.replace(escapeCharactersRegExp, sanitizerReplaceFunc);
}

/**
 * Add the given token to the pretty printed results.
 *
 * @param Object token
 *        The token to add.
 * @param Function write
 *        The function to write pretty printed code.
 */
function addToken(token, write) {
  if (token.type.label == "string") {
    write(
      `'${sanitize(token.value)}'`,
      token.loc.start.line,
      token.loc.start.column,
      true
    );
  } else if (token.type.label == "regexp") {
    write(
      String(token.value.value),
      token.loc.start.line,
      token.loc.start.column,
      true
    );
  } else {
    let value;
    if (token.value != null) {
      value = token.value;
      if (token.type.label === "privateId") {
        value = `#${value}`;
      }
    } else {
      value = token.type.label;
    }
    write(String(value), token.loc.start.line, token.loc.start.column, true);
  }
}

/**
 * Returns true if the given token type belongs on the stack.
 */
function belongsOnStack(token) {
  const ttl = token.type.label;
  const ttk = token.type.keyword;
  return (
    ttl == "{" ||
    ttl == "(" ||
    ttl == "[" ||
    ttl == "?" ||
    ttl == "${" ||
    ttk == "do" ||
    ttk == "switch" ||
    ttk == "case" ||
    ttk == "default"
  );
}

/**
 * Returns true if the given token should cause us to pop the stack.
 */
function shouldStackPop(token, stack) {
  const ttl = token.type.label;
  const ttk = token.type.keyword;
  const top = stack.at(-1);
  return (
    ttl == "]" ||
    ttl == ")" ||
    ttl == "}" ||
    (ttl == ":" && (top == "case" || top == "default" || top == "?")) ||
    (ttk == "while" && top == "do")
  );
}

/**
 * Returns true if the given token type should cause us to decrement the
 * indent level.
 */
function decrementsIndent(tokenType, stack) {
  const top = stack.at(-1);
  return (
    (tokenType == "}" && top != "${") || (tokenType == "]" && top == "[\n")
  );
}

/**
 * Returns true if the given token should cause us to increment the indent
 * level.
 */
function incrementsIndent(token) {
  return (
    token.type.label == "{" ||
    token.isArrayLiteral ||
    token.type.keyword == "switch"
  );
}

/**
 * Add a comment to the pretty printed code.
 *
 * @param Function write
 *        The function to write pretty printed code.
 * @param Number indentLevel
 *        The number of indents deep we are.
 * @param Object options
 *        The options object.
 * @param Boolean block
 *        True if the comment is a multiline block style comment.
 * @param String text
 *        The text of the comment.
 * @param Number line
 *        The line number to comment appeared on.
 * @param Number column
 *        The column number the comment appeared on.
 * @param Object nextToken
 *        The next token if any.
 *
 * @returns Boolean newlineAdded
 *        True if a newline was added.
 */
function addComment(
  write,
  indentLevel,
  options,
  block,
  text,
  line,
  column,
  nextToken
) {
  const indentString = options.indent.repeat(indentLevel);
  const needNewLineAfter =
    !block || !(nextToken && nextToken.loc.start.line == line);

  if (block) {
    const commentLinesText = text
      .split(new RegExp(`/\n${indentString}/`, "g"))
      .join(`\n${indentString}`);

    write(
      `${indentString}/*${commentLinesText}*/${needNewLineAfter ? "\n" : " "}`
    );
  } else {
    write(`${indentString}//${text}\n`);
  }

  return needNewLineAfter;
}

/**
 * The main function.
 *
 * @param {String} input: The ugly JS code we want to pretty print.
 * @param {Object} options: Provides configurability of the pretty printing.
 * @param {String} options.url: The URL string of the ugly JS code.
 * @param {String} options.indent: The string to indent code by.
 * @param {SourceMapGenerator} options.sourceMapGenerator: An optional sourceMapGenerator
 *                             the mappings will be added to.
 * @param {Boolean} options.prefixWithNewLine: When true, the pretty printed code will start
 *                  with a line break
 * @param {Integer} options.originalStartLine: The line the passed script starts at (1-based).
 *                  This is used for inline scripts where we need to account for the lines
 *                  before the script tag
 * @param {Integer} options.originalStartColumn: The column the passed script starts at (1-based).
 *                  This is used for inline scripts where we need to account for the position
 *                  of the script tag within the line.
 * @param {Integer} options.generatedStartLine: The line where the pretty printed script
 *                  will start at (1-based). This is used for pretty printing HTML file,
 *                  where we might have handle previous inline scripts that impact the
 *                  position of this script.
 *
 * @returns {Object}
 *          An object with the following properties:
 *            - code: The pretty printed code string.
 *            - map: A SourceMapGenerator instance.
 */
export function prettyFast(input, options = {}) {
  // The level of indents deep we are.
  let indentLevel = 0;

  const {
    url: file,
    originalStartLine,
    originalStartColumn,
    prefixWithNewLine,
    generatedStartLine,
  } = options;

  // We will handle mappings between ugly and pretty printed code in this SourceMapGenerator.
  const sourceMapGenerator =
    options.sourceMapGenerator ||
    new sourceMap.SourceMapGenerator({
      file,
    });

  let currentCode = "";
  let currentLine = 1;
  let currentColumn = 0;

  const hasOriginalStartLine = "originalStartLine" in options;
  const hasOriginalStartColumn = "originalStartColumn" in options;
  const hasGeneratedStartLine = "generatedStartLine" in options;

  /**
   * Write a pretty printed string to the prettified string and for tokens, add their
   * mapping to the SourceMapGenerator.
   *
   * @param String str
   *        The string to be added to the result.
   * @param Number line
   *        The line number the string came from in the ugly source.
   * @param Number column
   *        The column number the string came from in the ugly source.
   * @param Boolean isToken
   *        Set to true when writing tokens, so we can differentiate them from the
   *        whitespace we add.
   */
  const write = (str, line, column, isToken) => {
    currentCode += str;
    if (isToken) {
      sourceMapGenerator.addMapping({
        source: file,
        // We need to swap original and generated locations, as the prettified text should
        // be seen by the sourcemap service as the "original" one.
        generated: {
          // originalStartLine is 1-based, and here we just want to offset by a number of
          // lines, so we need to decrement it
          line: hasOriginalStartLine ? line + (originalStartLine - 1) : line,
          // We only need to adjust the column number if we're looking at the first line, to
          // account for the html text before the opening <script> tag.
          column:
            line == 1 && hasOriginalStartColumn
              ? column + originalStartColumn
              : column,
        },
        original: {
          // generatedStartLine is 1-based, and here we just want to offset by a number of
          // lines, so we need to decrement it.
          line: hasGeneratedStartLine
            ? currentLine + (generatedStartLine - 1)
            : currentLine,
          column: currentColumn,
        },
        name: null,
      });
    }

    for (let idx = 0, length = str.length; idx < length; idx++) {
      if (str.charCodeAt(idx) === NEWLINE_CODE) {
        currentLine++;
        currentColumn = 0;
      } else {
        currentColumn++;
      }
    }
  };

  // Add the initial new line if needed
  if (prefixWithNewLine) {
    write("\n");
  }

  // Whether or not we added a newline on after we added the last token.
  let addedNewline = false;

  // Whether or not we added a space after we added the last token.
  let addedSpace = false;

  // Shorthand for token.type.label, so we don't have to repeatedly access
  // properties.
  let ttl;

  // Shorthand for token.type.keyword, so we don't have to repeatedly access
  // properties.
  let ttk;

  // The last token we added to the pretty printed code.
  let lastToken;

  // Stack of token types/keywords that can affect whether we want to add a
  // newline or a space. We can make that decision based on what token type is
  // on the top of the stack. For example, a comma in a parameter list should
  // be followed by a space, while a comma in an object literal should be
  // followed by a newline.
  //
  // Strings that go on the stack:
  //
  //   - "{"
  //   - "("
  //   - "["
  //   - "[\n"
  //   - "do"
  //   - "?"
  //   - "switch"
  //   - "case"
  //   - "default"
  //
  // The difference between "[" and "[\n" is that "[\n" is used when we are
  // treating "[" and "]" tokens as line delimiters and should increment and
  // decrement the indent level when we find them.
  const stack = [];

  // Pass through acorn's tokenizer and append tokens and comments into a
  // single queue to process.  For example, the source file:
  //
  //     foo
  //     // a
  //     // b
  //     bar
  //
  // After this process, tokenQueue has the following token stream:
  //
  //     [ foo, '// a', '// b', bar]
  const tokenQueue = getTokens(input, options);

  for (let i = 0, len = tokenQueue.length; i < len; i++) {
    const token = tokenQueue[i];
    const nextToken = tokenQueue[i + 1];

    if (token.comment) {
      let commentIndentLevel = indentLevel;
      if (lastToken?.loc?.end?.line == token.loc.start.line) {
        commentIndentLevel = 0;
        write(" ");
      }
      addedNewline = addComment(
        write,
        commentIndentLevel,
        options,
        token.block,
        token.text,
        token.loc.start.line,
        token.loc.start.column,
        nextToken
      );
      addedSpace = !addedNewline;
      continue;
    }

    ttk = token.type.keyword;

    if (ttk && lastToken?.type?.label == ".") {
      token.type = acorn.tokTypes.name;
    }

    ttl = token.type.label;

    if (ttl == "eof") {
      if (!addedNewline) {
        write("\n");
      }
      break;
    }

    token.isArrayLiteral = isArrayLiteral(token, lastToken);

    if (belongsOnStack(token)) {
      if (token.isArrayLiteral) {
        stack.push("[\n");
      } else {
        stack.push(ttl || ttk);
      }
    }

    if (decrementsIndent(ttl, stack)) {
      indentLevel--;
      if (ttl == "}" && stack.at(-2) == "switch") {
        indentLevel--;
      }
    }

    prependWhiteSpace(
      token,
      lastToken,
      addedNewline,
      addedSpace,
      write,
      options,
      indentLevel,
      stack
    );
    addToken(token, write);
    addedSpace = false;

    // If the next token is going to be a comment starting on the same line,
    // then no need to add one here
    if (
      !nextToken ||
      !nextToken.comment ||
      token.loc.end.line != nextToken.loc.start.line
    ) {
      addedNewline = appendNewline(token, write, stack);
    }

    if (shouldStackPop(token, stack)) {
      stack.pop();
      if (ttl == "}" && stack.at(-1) == "switch") {
        stack.pop();
      }
    }

    if (incrementsIndent(token)) {
      indentLevel++;
    }

    // Acorn's tokenizer re-uses tokens, so we have to copy the last token on
    // every iteration. We follow acorn's lead here, and reuse the lastToken
    // object the same way that acorn reuses the token object. This allows us
    // to avoid allocations and minimize GC pauses.
    if (!lastToken) {
      lastToken = { loc: { start: {}, end: {} } };
    }
    lastToken.start = token.start;
    lastToken.end = token.end;
    lastToken.loc.start.line = token.loc.start.line;
    lastToken.loc.start.column = token.loc.start.column;
    lastToken.loc.end.line = token.loc.end.line;
    lastToken.loc.end.column = token.loc.end.column;
    lastToken.type = token.type;
    lastToken.value = token.value;
    lastToken.isArrayLiteral = token.isArrayLiteral;
  }

  return { code: currentCode, map: sourceMapGenerator };
}

/**
 * Returns the tokens computed with acorn.
 *
 * @param String input
 *        The JS code we want the tokens of.
 * @param Object options
 * @param String options.url
 *        The URL string of the ugly JS code.
 * @param String options.ecmaVersion
 * @returns Array<Object>
 */
function getTokens(input, options) {
  const tokens = [];

  const res = acorn.tokenizer(input, {
    locations: true,
    ecmaVersion: options.ecmaVersion || "latest",
    onComment(block, text, start, end, startLoc, endLoc) {
      tokens.push({
        type: {},
        comment: true,
        block,
        text,
        loc: { start: startLoc, end: endLoc },
      });
    },
  });

  for (;;) {
    const token = res.getToken();
    tokens.push(token);
    if (token.type.label == "eof") {
      break;
    }
  }

  return tokens;
}
