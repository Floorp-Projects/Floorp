/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file holds various CSS parsing and rewriting utilities.
// Some entry points of note are:
// parseDeclarations - parse a CSS rule into declarations
// RuleRewriter - rewrite CSS rule text
// parsePseudoClassesAndAttributes - parse selector and extract
//     pseudo-classes
// parseSingleValue - parse a single CSS property value

"use strict";

const {Cc, Ci, Cu} = require("chrome");
loader.lazyRequireGetter(this, "CSS", "CSS");
const promise = require("promise");
Cu.import("resource://gre/modules/Task.jsm", this);
loader.lazyGetter(this, "DOMUtils", () => {
  return Cc["@mozilla.org/inspector/dom-utils;1"].getService(Ci.inIDOMUtils);
});

const SELECTOR_ATTRIBUTE = exports.SELECTOR_ATTRIBUTE = 1;
const SELECTOR_ELEMENT = exports.SELECTOR_ELEMENT = 2;
const SELECTOR_PSEUDO_CLASS = exports.SELECTOR_PSEUDO_CLASS = 3;

// Used to test whether a newline appears anywhere in some text.
const NEWLINE_RX = /[\r\n]/;
// Used to test whether a bit of text starts an empty comment, either
// an "ordinary" /* ... */ comment, or a "heuristic bypass" comment
// like /*! ... */.
const EMPTY_COMMENT_START_RX = /^\/\*!?[ \r\n\t\f]*$/;
// Used to test whether a bit of text ends an empty comment.
const EMPTY_COMMENT_END_RX = /^[ \r\n\t\f]*\*\//;
// Used to test whether a string starts with a blank line.
const BLANK_LINE_RX = /^[ \t]*(?:\r\n|\n|\r|\f|$)/;

// When commenting out a declaration, we put this character into the
// comment opener so that future parses of the commented text know to
// bypass the property name validity heuristic.
const COMMENT_PARSING_HEURISTIC_BYPASS_CHAR = "!";

/**
 * A generator function that lexes a CSS source string, yielding the
 * CSS tokens.  Comment tokens are dropped.
 *
 * @param {String} CSS source string
 * @yield {CSSToken} The next CSSToken that is lexed
 * @see CSSToken for details about the returned tokens
 */
function* cssTokenizer(string) {
  let lexer = DOMUtils.getCSSLexer(string);
  while (true) {
    let token = lexer.nextToken();
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
 * |cssTokenizer| or |DOMUtils.getCSSLexer| instead.
 *
 * @param{String} string The input string.
 * @return {Array} An array of tokens (@see CSSToken) that have
 *        line and column information.
 */
function cssTokenizerWithLineColumn(string) {
  let lexer = DOMUtils.getCSSLexer(string);
  let result = [];
  let prevToken = undefined;
  while (true) {
    let token = lexer.nextToken();
    let lineNumber = lexer.lineNumber;
    let columnNumber = lexer.columnNumber;

    if (prevToken) {
      prevToken.loc.end = {
        line: lineNumber,
        column: columnNumber
      };
    }

    if (!token) {
      break;
    }

    if (token.tokenType === "comment") {
      // We've already dealt with the previous token's location.
      prevToken = undefined;
    } else {
      let startLoc = {
        line: lineNumber,
        column: columnNumber
      };
      token.loc = {start: startLoc};

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
  let result = inputString.replace(/\/(\\*)\*/g, "/\\$1*");
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
  let result = inputString.replace(/\/\\(\\*)\*/g, "/$1*");
  return result.replace(/\*\\(\\*)\//g, "*$1/");
}

/**
 * A helper function for parseDeclarations that implements a heuristic
 * to decide whether a given bit of comment text should be parsed as a
 * declaration.
 *
 * @param {String} name the property name that has been parsed
 * @return {Boolean} true if the property should be parsed, false if
 *                        the remainder of the comment should be skipped
 */
function shouldParsePropertyInComment(name) {
  try {
    // If the property name is invalid, the cssPropertyIsShorthand
    // will throw an exception.  But if it is valid, no exception will
    // be thrown; so we just ignore the return value.
    DOMUtils.cssPropertyIsShorthand(name);
    return true;
  } catch (e) {
    return false;
  }
}

/**
 * A helper function for @see parseDeclarations that handles parsing
 * of comment text.  This wraps a recursive call to parseDeclarations
 * with the processing needed to ensure that offsets in the result
 * refer back to the original, unescaped, input string.
 *
 * @param {String} commentText The text of the comment, without the
 *                             delimiters.
 * @param {Number} startOffset The offset of the comment opener
 *                             in the original text.
 * @param {Number} endOffset The offset of the comment closer
 *                           in the original text.
 * @return {array} Array of declarations of the same form as returned
 *                 by parseDeclarations.
 */
function parseCommentDeclarations(commentText, startOffset, endOffset) {
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

  let rewrittenText = unescapeCSSComment(commentText);

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
  let rewrites = new Array(rewrittenText.length + 1).fill(0);

  let commentRe = /\/\\*\*|\*\\*\//g;
  while (true) {
    let matchData = commentRe.exec(rewrittenText);
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
  let newDecls = parseDeclarationsInternal(rewrittenText, false,
                                           true, commentOverride);
  for (let decl of newDecls) {
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
  return {name: "", value: "", priority: "",
          terminator: "",
          offsets: [undefined, undefined],
          colonOffsets: false};
}

/**
 * A helper function that does all the parsing work for
 * parseDeclarations.  This is separate because it has some arguments
 * that don't make sense in isolation.
 *
 * The return value and arguments are like parseDeclarations, with
 * these additional arguments.
 *
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
function parseDeclarationsInternal(inputString, parseComments,
                                   inComment, commentOverride) {
  if (inputString === null || inputString === undefined) {
    throw new Error("empty input string");
  }

  let lexer = DOMUtils.getCSSLexer(inputString);

  let declarations = [getEmptyDeclaration()];
  let lastProp = declarations[0];

  let current = "", hasBang = false;
  while (true) {
    let token = lexer.nextToken();
    if (!token) {
      break;
    }

    // Ignore HTML comment tokens (but parse anything they might
    // happen to surround).
    if (token.tokenType === "htmlcomment") {
      continue;
    }

    // Update the start and end offsets of the declaration, but only
    // when we see a significant token.
    if (token.tokenType !== "whitespace" && token.tokenType !== "comment") {
      if (lastProp.offsets[0] === undefined) {
        lastProp.offsets[0] = token.startOffset;
      }
      lastProp.offsets[1] = token.endOffset;
    } else if (lastProp.name && !current && !hasBang &&
               !lastProp.priority && lastProp.colonOffsets[1]) {
      // Whitespace appearing after the ":" is attributed to it.
      lastProp.colonOffsets[1] = token.endOffset;
    }

    if (token.tokenType === "symbol" && token.text === ":") {
      if (!lastProp.name) {
        // Set the current declaration name if there's no name yet
        lastProp.name = current.trim();
        lastProp.colonOffsets = [token.startOffset, token.endOffset];
        current = "";
        hasBang = false;

        // When parsing a comment body, if the left-hand-side is not a
        // valid property name, then drop it and stop parsing.
        if (inComment && !commentOverride &&
            !shouldParsePropertyInComment(lastProp.name)) {
          lastProp.name = null;
          break;
        }
      } else {
        // Otherwise, just append ':' to the current value (declaration value
        // with colons)
        current += ":";
      }
    } else if (token.tokenType === "symbol" && token.text === ";") {
      lastProp.terminator = "";
      // When parsing a comment, if the name hasn't been set, then we
      // have probably just seen an ordinary semicolon used in text,
      // so drop this and stop parsing.
      if (inComment && !lastProp.name) {
        current = "";
        break;
      }
      lastProp.value = current.trim();
      current = "";
      hasBang = false;
      declarations.push(getEmptyDeclaration());
      lastProp = declarations[declarations.length - 1];
    } else if (token.tokenType === "ident") {
      if (token.text === "important" && hasBang) {
        lastProp.priority = "important";
        hasBang = false;
      } else {
        if (hasBang) {
          current += "!";
        }
        current += token.text;
      }
    } else if (token.tokenType === "symbol" && token.text === "!") {
      hasBang = true;
    } else if (token.tokenType === "whitespace") {
      if (current !== "") {
        current += " ";
      }
    } else if (token.tokenType === "comment") {
      if (parseComments && !lastProp.name && !lastProp.value) {
        let commentText = inputString.substring(token.startOffset + 2,
                                                token.endOffset - 2);
        let newDecls = parseCommentDeclarations(commentText, token.startOffset,
                                                token.endOffset);

        // Insert the new declarations just before the final element.
        let lastDecl = declarations.pop();
        declarations = [...declarations, ...newDecls, lastDecl];
      }
    } else {
      current += inputString.substring(token.startOffset, token.endOffset);
    }
  }

  // Handle whatever trailing properties or values might still be there
  if (current) {
    if (!lastProp.name) {
      // Ignore this case in comments.
      if (!inComment) {
        // Trailing property found, e.g. p1:v1;p2:v2;p3
        lastProp.name = current.trim();
      }
    } else {
      // Trailing value found, i.e. value without an ending ;
      lastProp.value = current.trim();
      let terminator = lexer.performEOFFixup("", true);
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
 * For example, parseDeclarations("width: 1px; height: 1px") would return
 * [{name:"width", value: "1px"}, {name: "height", "value": "1px"}]
 *
 * The input string is assumed to only contain declarations so { and }
 * characters will be treated as part of either the property or value,
 * depending where it's found.
 *
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
function parseDeclarations(inputString, parseComments = false) {
  return parseDeclarationsInternal(inputString, parseComments, false, false);
}

/**
 * Return an object that can be used to rewrite declarations in some
 * source text.  The source text and parsing are handled in the same
 * way as @see parseDeclarations, with |parseComments| being true.
 * Rewriting is done by calling one of the modification functions like
 * setPropertyEnabled.  The returned object has the same interface
 * as @see RuleModificationList.
 *
 * An example showing how to disable the 3rd property in a rule:
 *
 *    let rewriter = new RuleRewriter(ruleActor, ruleActor.authoredText);
 *    rewriter.setPropertyEnabled(3, "color", false);
 *    rewriter.apply().then(() => { ... the change is made ... });
 *
 * The exported rewriting methods are |renameProperty|, |setPropertyEnabled|,
 * |createProperty|, |setProperty|, and |removeProperty|.  The |apply|
 * method can be used to send the edited text to the StyleRuleActor;
 * |getDefaultIndentation| is useful for the methods requiring a
 * default indentation value; and |getResult| is useful for testing.
 *
 * Additionally, editing will set the |changedDeclarations| property
 * on this object.  This property has the same form as the |changed|
 * property of the object returned by |getResult|.
 *
 * @param {StyleRuleFront} rule The style rule to use.  Note that this
 *        is only needed by the |apply| and |getDefaultIndentation| methods;
 *        and in particular for testing it can be |null|.
 * @param {String} inputString The CSS source text to parse and modify.
 * @return {Object} an object that can be used to rewrite the input text.
 */
function RuleRewriter(rule, inputString) {
  this.rule = rule;
  this.inputString = inputString;
  // Whether there are any newlines in the input text.
  this.hasNewLine = /[\r\n]/.test(this.inputString);
  // Keep track of which any declarations we had to rewrite while
  // performing the requested action.
  this.changedDeclarations = {};
  // The declarations.
  this.declarations = parseDeclarations(this.inputString, true);

  this.decl = null;
  this.result = null;
  // If not null, a promise that must be wait upon before |apply| can
  // do its work.
  this.editPromise = null;

  // If the |defaultIndentation| property is set, then it is used;
  // otherwise the RuleRewriter will try to compute the default
  // indentation based on the style sheet's text.  This override
  // facility is for testing.
  this.defaultIndentation = null;
}

RuleRewriter.prototype = {
  /**
   * An internal function to complete initialization and set some
   * properties for further processing.
   *
   * @param {Number} index The index of the property to modify
   */
  completeInitialization: function (index) {
    if (index < 0) {
      throw new Error("Invalid index " + index + ". Expected positive integer");
    }
    // |decl| is the declaration to be rewritten, or null if there is no
    // declaration corresponding to |index|.
    // |result| is used to accumulate the result text.
    if (index < this.declarations.length) {
      this.decl = this.declarations[index];
      this.result = this.inputString.substring(0, this.decl.offsets[0]);
    } else {
      this.decl = null;
      this.result = this.inputString;
    }
  },

  /**
   * A helper function to compute the indentation of some text.  This
   * examines the rule's existing text to guess the indentation to use;
   * unlike |getDefaultIndentation|, which examines the entire style
   * sheet.
   *
   * @param {String} string the input text
   * @param {Number} offset the offset at which to compute the indentation
   * @return {String} the indentation at the indicated position
   */
  getIndentation: function (string, offset) {
    let originalOffset = offset;
    for (--offset; offset >= 0; --offset) {
      let c = string[offset];
      if (c === "\r" || c === "\n" || c === "\f") {
        return string.substring(offset + 1, originalOffset);
      }
      if (c !== " " && c !== "\t") {
        // Found some non-whitespace character before we found a newline
        // -- let's reset the starting point and keep going, as we saw
        // something on the line before the declaration.
        originalOffset = offset;
      }
    }
    // Ran off the end.
    return "";
  },

  /**
   * Modify a property value to ensure it is "lexically safe" for
   * insertion into a style sheet.  This function doesn't attempt to
   * ensure that the resulting text is a valid value for the given
   * property; but rather just that inserting the text into the style
   * sheet will not cause unwanted changes to other rules or
   * declarations.
   *
   * @param {String} text The input text.  This should include the trailing ";".
   * @return {Array} An array of the form [anySanitized, text], where
   *                 |anySanitized| is a boolean that indicates
   *                  whether anything substantive has changed; and
   *                  where |text| is the text that has been rewritten
   *                  to be "lexically safe".
   */
  sanitizePropertyValue: function (text) {
    let lexer = DOMUtils.getCSSLexer(text);

    let result = "";
    let previousOffset = 0;
    let braceDepth = 0;
    let anySanitized = false;
    while (true) {
      let token = lexer.nextToken();
      if (!token) {
        break;
      }

      if (token.tokenType === "symbol") {
        switch (token.text) {
          case ";":
            // We simply drop the ";" here.  This lets us cope with
            // declarations that don't have a ";" and also other
            // termination.  The caller handles adding the ";" again.
            result += text.substring(previousOffset, token.startOffset);
            previousOffset = token.endOffset;
            break;

          case "{":
            ++braceDepth;
            break;

          case "}":
            --braceDepth;
            if (braceDepth < 0) {
              // Found an unmatched close bracket.
              braceDepth = 0;
              // Copy out text from |previousOffset|.
              result += text.substring(previousOffset, token.startOffset);
              // Quote the offending symbol.
              result += "\\" + token.text;
              previousOffset = token.endOffset;
              anySanitized = true;
            }
            break;
        }
      }
    }

    // Copy out any remaining text, then any needed terminators.
    result += text.substring(previousOffset, text.length);
    let eofFixup = lexer.performEOFFixup("", true);
    if (eofFixup) {
      anySanitized = true;
      result += eofFixup;
    }
    return [anySanitized, result];
  },

  /**
   * Start at |index| and skip whitespace
   * backward in |string|.  Return the index of the first
   * non-whitespace character, or -1 if the entire string was
   * whitespace.
   * @param {String} string the input string
   * @param {Number} index the index at which to start
   * @return {Number} index of the first non-whitespace character, or -1
   */
  skipWhitespaceBackward: function (string, index) {
    for (--index;
         index >= 0 && (string[index] === " " || string[index] === "\t");
         --index) {
      // Nothing.
    }
    return index;
  },

  /**
   * Terminate a given declaration, if needed.
   *
   * @param {Number} index The index of the rule to possibly
   *                       terminate.  It might be invalid, so this
   *                       function must check for that.
   */
  maybeTerminateDecl: function (index) {
    if (index < 0 || index >= this.declarations.length
        // No need to rewrite declarations in comments.
        || ("commentOffsets" in this.declarations[index])) {
      return;
    }

    let termDecl = this.declarations[index];
    let endIndex = termDecl.offsets[1];
    // Due to an oddity of the lexer, we might have gotten a bit of
    // extra whitespace in a trailing bad_url token -- so be sure to
    // skip that as well.
    endIndex = this.skipWhitespaceBackward(this.result, endIndex) + 1;

    let trailingText = this.result.substring(endIndex);
    if (termDecl.terminator) {
      // Insert the terminator just at the end of the declaration,
      // before any trailing whitespace.
      this.result = this.result.substring(0, endIndex) + termDecl.terminator +
        trailingText;
      // In a couple of cases, we may have had to add something to
      // terminate the declaration, but the termination did not
      // actually affect the property's value -- and at this spot, we
      // only care about reporting value changes.  In particular, we
      // might have added a plain ";", or we might have terminated a
      // comment with "*/;".  Neither of these affect the value.
      if (termDecl.terminator !== ";" && termDecl.terminator !== "*/;") {
        this.changedDeclarations[index] =
          termDecl.value + termDecl.terminator.slice(0, -1);
      }
    }
    // If the rule generally has newlines, but this particular
    // declaration doesn't have a trailing newline, insert one now.
    // Maybe this style is too weird to bother with.
    if (this.hasNewLine && !NEWLINE_RX.test(trailingText)) {
      this.result += "\n";
    }
  },

  /**
   * Sanitize the given property value and return the sanitized form.
   * If the property is rewritten during sanitization, make a note in
   * |changedDeclarations|.
   *
   * @param {String} text The property text.
   * @param {Number} index The index of the property.
   * @return {String} The sanitized text.
   */
  sanitizeText: function (text, index) {
    let [anySanitized, sanitizedText] = this.sanitizePropertyValue(text);
    if (anySanitized) {
      this.changedDeclarations[index] = sanitizedText;
    }
    return sanitizedText;
  },

  /**
   * Rename a declaration.
   *
   * @param {Number} index index of the property in the rule.
   * @param {String} name current name of the property
   * @param {String} newName new name of the property
   */
  renameProperty: function (index, name, newName) {
    this.completeInitialization(index);
    this.result += CSS.escape(newName);
    // We could conceivably compute the name offsets instead so we
    // could preserve white space and comments on the LHS of the ":".
    this.completeCopying(this.decl.colonOffsets[0]);
  },

  /**
   * Enable or disable a declaration
   *
   * @param {Number} index index of the property in the rule.
   * @param {String} name current name of the property
   * @param {Boolean} isEnabled true if the property should be enabled;
   *                        false if it should be disabled
   */
  setPropertyEnabled: function (index, name, isEnabled) {
    this.completeInitialization(index);
    const decl = this.decl;
    let copyOffset = decl.offsets[1];
    if (isEnabled) {
      // Enable it.  First see if the comment start can be deleted.
      let commentStart = decl.commentOffsets[0];
      if (EMPTY_COMMENT_START_RX.test(this.result.substring(commentStart))) {
        this.result = this.result.substring(0, commentStart);
      } else {
        this.result += "*/ ";
      }

      // Insert the name and value separately, so we can report
      // sanitization changes properly.
      let commentNamePart =
          this.inputString.substring(decl.offsets[0],
                                     decl.colonOffsets[1]);
      this.result += unescapeCSSComment(commentNamePart);

      // When uncommenting, we must be sure to sanitize the text, to
      // avoid things like /* decl: }; */, which will be accepted as
      // a property but which would break the entire style sheet.
      let newText = this.inputString.substring(decl.colonOffsets[1],
                                               decl.offsets[1]);
      newText = unescapeCSSComment(newText).trimRight();
      this.result += this.sanitizeText(newText, index) + ";";

      // See if the comment end can be deleted.
      let trailingText = this.inputString.substring(decl.offsets[1]);
      if (EMPTY_COMMENT_END_RX.test(trailingText)) {
        copyOffset = decl.commentOffsets[1];
      } else {
        this.result += " /*";
      }
    } else {
      // Disable it.  Note that we use our special comment syntax
      // here.
      let declText = this.inputString.substring(decl.offsets[0],
                                                decl.offsets[1]);
      this.result += "/*" + COMMENT_PARSING_HEURISTIC_BYPASS_CHAR +
        " " + escapeCSSComment(declText) + " */";
    }
    this.completeCopying(copyOffset);
  },

  /**
   * Return a promise that will be resolved to the default indentation
   * of the rule.  This is a helper for internalCreateProperty.
   *
   * @return {Promise} a promise that will be resolved to a string
   *         that holds the default indentation that should be used
   *         for edits to the rule.
   */
  getDefaultIndentation: function () {
    return this.rule.parentStyleSheet.guessIndentation();
  },

  /**
   * An internal function to create a new declaration.  This does all
   * the work of |createProperty|.
   *
   * @param {Number} index index of the property in the rule.
   * @param {String} name name of the new property
   * @param {String} value value of the new property
   * @param {String} priority priority of the new property; either
   *                          the empty string or "important"
   * @return {Promise} a promise that is resolved when the edit has
   *                   completed
   */
  internalCreateProperty: Task.async(function* (index, name, value, priority) {
    this.completeInitialization(index);
    let newIndentation = "";
    if (this.hasNewLine) {
      if (this.declarations.length > 0) {
        newIndentation = this.getIndentation(this.inputString,
                                             this.declarations[0].offsets[0]);
      } else if (this.defaultIndentation) {
        newIndentation = this.defaultIndentation;
      } else {
        newIndentation = yield this.getDefaultIndentation();
      }
    }

    this.maybeTerminateDecl(index - 1);

    // If we generally have newlines, and if skipping whitespace
    // backward stops at a newline, then insert our text before that
    // whitespace.  This ensures the indentation we computed is what
    // is actually used.
    let savedWhitespace = "";
    if (this.hasNewLine) {
      let wsOffset = this.skipWhitespaceBackward(this.result,
                                                 this.result.length);
      if (this.result[wsOffset] === "\r" || this.result[wsOffset] === "\n") {
        savedWhitespace = this.result.substring(wsOffset + 1);
        this.result = this.result.substring(0, wsOffset + 1);
      }
    }

    this.result += newIndentation + CSS.escape(name) + ": " +
      this.sanitizeText(value, index);

    if (priority === "important") {
      this.result += " !important";
    }
    this.result += ";";
    if (this.hasNewLine) {
      this.result += "\n";
    }
    this.result += savedWhitespace;

    if (this.decl) {
      // Still want to copy in the declaration previously at this
      // index.
      this.completeCopying(this.decl.offsets[0]);
    }
  }),

  /**
   * Create a new declaration.
   *
   * @param {Number} index index of the property in the rule.
   * @param {String} name name of the new property
   * @param {String} value value of the new property
   * @param {String} priority priority of the new property; either
   *                          the empty string or "important"
   */
  createProperty: function (index, name, value, priority) {
    this.editPromise = this.internalCreateProperty(index, name, value,
                                                   priority);
  },

  /**
   * Set a declaration's value.
   *
   * @param {Number} index index of the property in the rule.
   *                       This can be -1 in the case where
   *                       the rule does not support setRuleText;
   *                       generally for setting properties
   *                       on an element's style.
   * @param {String} name the property's name
   * @param {String} value the property's value
   * @param {String} priority the property's priority, either the empty
   *                          string or "important"
   */
  setProperty: function (index, name, value, priority) {
    this.completeInitialization(index);
    // We might see a "set" on a previously non-existent property; in
    // that case, act like "create".
    if (!this.decl) {
      this.createProperty(index, name, value, priority);
      return;
    }

    // Note that this assumes that "set" never operates on disabled
    // properties.
    this.result += this.inputString.substring(this.decl.offsets[0],
                                              this.decl.colonOffsets[1]) +
      this.sanitizeText(value, index);

    if (priority === "important") {
      this.result += " !important";
    }
    this.result += ";";
    this.completeCopying(this.decl.offsets[1]);
  },

  /**
   * Remove a declaration.
   *
   * @param {Number} index index of the property in the rule.
   * @param {String} name the name of the property to remove
   */
  removeProperty: function (index, name) {
    this.completeInitialization(index);
    let copyOffset = this.decl.offsets[1];
    // Maybe removing this rule left us with a completely blank
    // line.  In this case, we'll delete the whole thing.  We only
    // bother with this if we're looking at sources that already
    // have a newline somewhere.
    if (this.hasNewLine) {
      let nlOffset = this.skipWhitespaceBackward(this.result,
                                                 this.decl.offsets[0]);
      if (nlOffset < 0 || this.result[nlOffset] === "\r" ||
          this.result[nlOffset] === "\n") {
        let trailingText = this.inputString.substring(copyOffset);
        let match = BLANK_LINE_RX.exec(trailingText);
        if (match) {
          this.result = this.result.substring(0, nlOffset + 1);
          copyOffset += match[0].length;
        }
      }
    }
    this.completeCopying(copyOffset);
  },

  /**
   * An internal function to copy any trailing text to the output
   * string.
   *
   * @param {Number} copyOffset Offset into |inputString| of the
   *        final text to copy to the output string.
   */
  completeCopying: function (copyOffset) {
    // Add the trailing text.
    this.result += this.inputString.substring(copyOffset);
  },

  /**
   * Apply the modifications in this object to the associated rule.
   *
   * @return {Promise} A promise which will be resolved when the modifications
   *         are complete.
   */
  apply: function () {
    return promise.resolve(this.editPromise).then(() => {
      return this.rule.setRuleText(this.result);
    });
  },

  /**
   * Get the result of the rewriting.  This is used for testing.
   *
   * @return {object} an object of the form {changed: object, text: string}
   *                  |changed| is an object where each key is
   *                  the index of a property whose value had to be
   *                  rewritten during the sanitization process, and
   *                  whose value is the new text of the property.
   *                  |text| is the rewritten text of the rule.
   */
  getResult: function () {
    return {changed: this.changedDeclarations, text: this.result};
  },
};

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
function parsePseudoClassesAndAttributes(value) {
  if (!value) {
    throw new Error("empty input string");
  }

  let tokens = cssTokenizer(value);
  let result = [];
  let current = "";
  let functionCount = 0;
  let hasAttribute = false;
  let hasColon = false;

  for (let token of tokens) {
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
 * @param {String} value
 *        The value from the text editor.
 * @return {Object} an object with 'value' and 'priority' properties.
 */
function parseSingleValue(value) {
  let declaration = parseDeclarations("a: " + value + ";")[0];
  return {
    value: declaration ? declaration.value : "",
    priority: declaration ? declaration.priority : ""
  };
}

exports.cssTokenizer = cssTokenizer;
exports.cssTokenizerWithLineColumn = cssTokenizerWithLineColumn;
exports.escapeCSSComment = escapeCSSComment;
// unescapeCSSComment is exported for testing.
exports._unescapeCSSComment = unescapeCSSComment;
exports.parseDeclarations = parseDeclarations;
// parseCommentDeclarations is exported for testing.
exports._parseCommentDeclarations = parseCommentDeclarations;
exports.RuleRewriter = RuleRewriter;
exports.parsePseudoClassesAndAttributes = parsePseudoClassesAndAttributes;
exports.parseSingleValue = parseSingleValue;
