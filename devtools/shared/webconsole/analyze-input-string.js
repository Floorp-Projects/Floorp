/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const STATE_NORMAL = Symbol("STATE_NORMAL");
const STATE_QUOTE = Symbol("STATE_QUOTE");
const STATE_DQUOTE = Symbol("STATE_DQUOTE");
const STATE_TEMPLATE_LITERAL = Symbol("STATE_TEMPLATE_LITERAL");
const STATE_ESCAPE_QUOTE = Symbol("STATE_ESCAPE_QUOTE");
const STATE_ESCAPE_DQUOTE = Symbol("STATE_ESCAPE_DQUOTE");
const STATE_ESCAPE_TEMPLATE_LITERAL = Symbol("STATE_ESCAPE_TEMPLATE_LITERAL");
const STATE_SLASH = Symbol("STATE_SLASH");
const STATE_INLINE_COMMENT = Symbol("STATE_INLINE_COMMENT");
const STATE_MULTILINE_COMMENT = Symbol("STATE_MULTILINE_COMMENT");
const STATE_MULTILINE_COMMENT_CLOSE = Symbol("STATE_MULTILINE_COMMENT_CLOSE");
const STATE_QUESTION_MARK = Symbol("STATE_QUESTION_MARK");

const OPEN_BODY = "{[(".split("");
const CLOSE_BODY = "}])".split("");
const OPEN_CLOSE_BODY = {
  "{": "}",
  "[": "]",
  "(": ")",
};

const NO_AUTOCOMPLETE_PREFIXES = ["var", "const", "let", "function", "class"];
const OPERATOR_CHARS_SET = new Set(";,:=<>+-*%|&^~!".split(""));

/**
 * Analyses a given string to find the last statement that is interesting for
 * later completion.
 *
 * @param   string str
 *          A string to analyse.
 *
 * @returns object
 *          If there was an error in the string detected, then a object like
 *
 *            { err: "ErrorMesssage" }
 *
 *          is returned, otherwise a object like
 *
 *            {
 *              state: STATE_NORMAL|STATE_QUOTE|STATE_DQUOTE,
 *              lastStatement: the last statement in the string,
 *              isElementAccess: boolean that indicates if the lastStatement has an open
 *                               element access (e.g. `x["match`).
 *              isPropertyAccess: boolean indicating if we are accessing property
 *                                (e.g `true` in `var a = {b: 1};a.b`)
 *              matchProp: The part of the expression that should match the properties
 *                         on the mainExpression (e.g. `que` when expression is `document.body.que`)
 *              mainExpression: The part of the expression before any property access,
 *                              (e.g. `a.b` if expression is `a.b.`)
 *              expressionBeforePropertyAccess: The part of the expression before property access
 *                                              (e.g `var a = {b: 1};a` if expression is `var a = {b: 1};a.b`)
 *            }
 */
// eslint-disable-next-line complexity
exports.analyzeInputString = function (str, timeout = 2500) {
  // work variables.
  const bodyStack = [];
  let state = STATE_NORMAL;
  let previousNonWhitespaceChar;
  let lastStatement = "";
  let currentIndex = -1;
  let dotIndex;
  let pendingWhitespaceChars = "";
  const startingTime = Date.now();

  // Use a string iterator in order to handle character with a length >= 2 (e.g. 😎).
  for (const c of str) {
    // We are possibly dealing with a very large string that would take a long time to
    // analyze (and freeze the process). If the function has been running for more than
    // a given time, we stop the analysis (this isn't too bad because the only
    // consequence is that we won't provide autocompletion items).
    if (Date.now() - startingTime > timeout) {
      return {
        err: "timeout",
      };
    }

    currentIndex += 1;
    let resetLastStatement = false;
    const isWhitespaceChar = c.trim() === "";
    switch (state) {
      // Normal JS state.
      case STATE_NORMAL:
        if (lastStatement.endsWith("?.") && /\d/.test(c)) {
          // If the current char is a number, the engine will consider we're not
          // performing an optional chaining, but a ternary (e.g. x ?.4 : 2).
          lastStatement = "";
        }

        // Storing the index of dot of the input string
        if (c === ".") {
          dotIndex = currentIndex;
        }

        // If the last characters were spaces, and the current one is not.
        if (pendingWhitespaceChars && !isWhitespaceChar) {
          // If we have a legitimate property/element access, or potential optional
          // chaining call, we append the spaces.
          if (c === "[" || c === "." || c === "?") {
            lastStatement = lastStatement + pendingWhitespaceChars;
          } else {
            // if not, we can be sure the statement was over, and we can start a new one.
            lastStatement = "";
          }
          pendingWhitespaceChars = "";
        }

        if (c == '"') {
          state = STATE_DQUOTE;
        } else if (c == "'") {
          state = STATE_QUOTE;
        } else if (c == "`") {
          state = STATE_TEMPLATE_LITERAL;
        } else if (c == "/") {
          state = STATE_SLASH;
        } else if (c == "?") {
          state = STATE_QUESTION_MARK;
        } else if (OPERATOR_CHARS_SET.has(c)) {
          // If the character is an operator, we can update the current statement.
          resetLastStatement = true;
        } else if (isWhitespaceChar) {
          // If the previous char isn't a dot or opening bracket, and the current computed
          // statement is not a variable/function/class declaration, we track the number
          // of consecutive spaces, so we can re-use them at some point (or drop them).
          if (
            previousNonWhitespaceChar !== "." &&
            previousNonWhitespaceChar !== "[" &&
            !NO_AUTOCOMPLETE_PREFIXES.includes(lastStatement)
          ) {
            pendingWhitespaceChars += c;
            continue;
          }
        } else if (OPEN_BODY.includes(c)) {
          // When opening a bracket or a parens, we store the current statement, in order
          // to be able to retrieve it later.
          bodyStack.push({
            token: c,
            lastStatement,
            index: currentIndex,
          });
          // And we compute a new statement.
          resetLastStatement = true;
        } else if (CLOSE_BODY.includes(c)) {
          const last = bodyStack.pop();
          if (!last || OPEN_CLOSE_BODY[last.token] != c) {
            return {
              err: "syntax error",
            };
          }
          if (c == "}") {
            resetLastStatement = true;
          } else {
            lastStatement = last.lastStatement;
          }
        }
        break;

      // Escaped quote
      case STATE_ESCAPE_QUOTE:
        state = STATE_QUOTE;
        break;
      case STATE_ESCAPE_DQUOTE:
        state = STATE_DQUOTE;
        break;
      case STATE_ESCAPE_TEMPLATE_LITERAL:
        state = STATE_TEMPLATE_LITERAL;
        break;

      // Double quote state > " <
      case STATE_DQUOTE:
        if (c == "\\") {
          state = STATE_ESCAPE_DQUOTE;
        } else if (c == "\n") {
          return {
            err: "unterminated string literal",
          };
        } else if (c == '"') {
          state = STATE_NORMAL;
        }
        break;

      // Template literal state > ` <
      case STATE_TEMPLATE_LITERAL:
        if (c == "\\") {
          state = STATE_ESCAPE_TEMPLATE_LITERAL;
        } else if (c == "`") {
          state = STATE_NORMAL;
        }
        break;

      // Single quote state > ' <
      case STATE_QUOTE:
        if (c == "\\") {
          state = STATE_ESCAPE_QUOTE;
        } else if (c == "\n") {
          return {
            err: "unterminated string literal",
          };
        } else if (c == "'") {
          state = STATE_NORMAL;
        }
        break;
      case STATE_SLASH:
        if (c == "/") {
          state = STATE_INLINE_COMMENT;
        } else if (c == "*") {
          state = STATE_MULTILINE_COMMENT;
        } else {
          lastStatement = "";
          state = STATE_NORMAL;
        }
        break;

      case STATE_INLINE_COMMENT:
        if (c === "\n") {
          state = STATE_NORMAL;
          resetLastStatement = true;
        }
        break;

      case STATE_MULTILINE_COMMENT:
        if (c === "*") {
          state = STATE_MULTILINE_COMMENT_CLOSE;
        }
        break;

      case STATE_MULTILINE_COMMENT_CLOSE:
        if (c === "/") {
          state = STATE_NORMAL;
          resetLastStatement = true;
        } else {
          state = STATE_MULTILINE_COMMENT;
        }
        break;

      case STATE_QUESTION_MARK:
        state = STATE_NORMAL;
        if (c === "?") {
          // If we have a nullish coalescing operator, we start a new statement
          resetLastStatement = true;
        } else if (c !== ".") {
          // If we're not dealing with optional chaining (?.), it means we have a ternary,
          // so we are starting a new statement that includes the current character.
          lastStatement = "";
        } else {
          dotIndex = currentIndex;
        }
        break;
    }

    if (!isWhitespaceChar) {
      previousNonWhitespaceChar = c;
    }
    if (resetLastStatement) {
      lastStatement = "";
    } else {
      lastStatement = lastStatement + c;
    }

    // We update all the open stacks lastStatement so they are up-to-date.
    bodyStack.forEach(stack => {
      if (stack.token !== "}") {
        stack.lastStatement = stack.lastStatement + c;
      }
    });
  }

  let isElementAccess = false;
  let lastOpeningBracketIndex = -1;
  if (bodyStack.length === 1 && bodyStack[0].token === "[") {
    lastStatement = bodyStack[0].lastStatement;
    lastOpeningBracketIndex = bodyStack[0].index;
    isElementAccess = true;

    if (
      state === STATE_DQUOTE ||
      state === STATE_QUOTE ||
      state === STATE_TEMPLATE_LITERAL ||
      state === STATE_ESCAPE_QUOTE ||
      state === STATE_ESCAPE_DQUOTE ||
      state === STATE_ESCAPE_TEMPLATE_LITERAL
    ) {
      state = STATE_NORMAL;
    }
  } else if (pendingWhitespaceChars) {
    lastStatement = "";
  }

  const lastCompletionCharIndex = isElementAccess
    ? lastOpeningBracketIndex
    : dotIndex;

  const stringBeforeLastCompletionChar = str.slice(0, lastCompletionCharIndex);

  const isPropertyAccess =
    lastCompletionCharIndex && lastCompletionCharIndex > 0;

  // Compute `isOptionalAccess`, so that we can use it
  // later for computing `expressionBeforePropertyAccess`.
  //Check `?.` before `[` for element access ( e.g `a?.["b` or `a  ?. ["b` )
  // and `?` before `.` for regular property access ( e.g `a?.b` or `a ?. b` )
  const optionalElementAccessRegex = /\?\.\s*$/;
  const isOptionalAccess = isElementAccess
    ? optionalElementAccessRegex.test(stringBeforeLastCompletionChar)
    : isPropertyAccess &&
      str.slice(lastCompletionCharIndex - 1, lastCompletionCharIndex + 1) ===
        "?.";

  // Get the filtered string for the properties (e.g if `document.qu` then `qu`)
  const matchProp = isPropertyAccess
    ? str.slice(lastCompletionCharIndex + 1).trimLeft()
    : null;

  const expressionBeforePropertyAccess = isPropertyAccess
    ? str.slice(
        0,
        // For optional access, we can take all the chars before the last "?" char.
        isOptionalAccess
          ? stringBeforeLastCompletionChar.lastIndexOf("?")
          : lastCompletionCharIndex
      )
    : str;

  let mainExpression = lastStatement;
  if (isPropertyAccess) {
    if (isOptionalAccess) {
      // Strip anything before the last `?`.
      mainExpression = mainExpression.slice(0, mainExpression.lastIndexOf("?"));
    } else {
      mainExpression = mainExpression.slice(
        0,
        -1 * (str.length - lastCompletionCharIndex)
      );
    }
  }

  mainExpression = mainExpression.trim();

  return {
    state,
    isElementAccess,
    isPropertyAccess,
    expressionBeforePropertyAccess,
    lastStatement,
    mainExpression,
    matchProp,
  };
};

/**
 * Checks whether the analyzed input string is in an appropriate state to autocomplete, e.g. not
 * inside a string, or declaring a variable.
 * @param {object} inputAnalysisState The analyzed string to check
 * @returns {boolean} Whether the input should be autocompleted
 */
exports.shouldInputBeAutocompleted = function (inputAnalysisState) {
  const { err, state, lastStatement } = inputAnalysisState;

  // There was an error analysing the string.
  if (err) {
    return false;
  }

  // If the current state is not STATE_NORMAL, then we are inside string,
  // which means that no completion is possible.
  if (state != STATE_NORMAL) {
    return false;
  }

  // Don't complete on just an empty string.
  if (lastStatement.trim() == "") {
    return false;
  }

  if (
    NO_AUTOCOMPLETE_PREFIXES.some(prefix =>
      lastStatement.startsWith(prefix + " ")
    )
  ) {
    return false;
  }

  return true;
};

/**
 * Checks whether the analyzed input string is in an appropriate state to be eagerly evaluated.
 * @param {object} inputAnalysisState
 * @returns {boolean} Whether the input should be eagerly evaluated
 */
exports.shouldInputBeEagerlyEvaluated = function ({ lastStatement }) {
  const inComputedProperty =
    lastStatement.lastIndexOf("[") !== -1 &&
    lastStatement.lastIndexOf("[") > lastStatement.lastIndexOf("]");

  const hasPropertyAccess =
    lastStatement.includes(".") || lastStatement.includes("[");

  return hasPropertyAccess && !inComputedProperty;
};
