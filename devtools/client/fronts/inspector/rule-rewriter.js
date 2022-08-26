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

const { getCSSLexer } = require("devtools/shared/css/lexer");
const {
  COMMENT_PARSING_HEURISTIC_BYPASS_CHAR,
  escapeCSSComment,
  parseNamedDeclarations,
  unescapeCSSComment,
} = require("devtools/shared/css/parsing-utils");

loader.lazyRequireGetter(
  this,
  ["getIndentationFromPrefs", "getIndentationFromString"],
  "devtools/shared/indentation",
  true
);

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

/**
 * Return an object that can be used to rewrite declarations in some
 * source text.  The source text and parsing are handled in the same
 * way as @see parseNamedDeclarations, with |parseComments| being true.
 * Rewriting is done by calling one of the modification functions like
 * setPropertyEnabled.  The returned object has the same interface
 * as @see RuleModificationList.
 *
 * An example showing how to disable the 3rd property in a rule:
 *
 *    let rewriter = new RuleRewriter(isCssPropertyKnown, ruleActor,
 *                                    ruleActor.authoredText);
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
 * @param {Function} isCssPropertyKnown
 *        A function to check if the CSS property is known. This is either an
 *        internal server function or from the CssPropertiesFront.
 *        that are supported by the server. Note that if Bug 1222047
 *        is completed then isCssPropertyKnown will not need to be passed in.
 *        The CssProperty front will be able to obtained directly from the
 *        RuleRewriter.
 * @param {StyleRuleFront} rule The style rule to use.  Note that this
 *        is only needed by the |apply| and |getDefaultIndentation| methods;
 *        and in particular for testing it can be |null|.
 * @param {String} inputString The CSS source text to parse and modify.
 * @return {Object} an object that can be used to rewrite the input text.
 */
function RuleRewriter(isCssPropertyKnown, rule, inputString) {
  this.rule = rule;
  this.isCssPropertyKnown = isCssPropertyKnown;
  // The RuleRewriter sends CSS rules as text to the server, but with this modifications
  // array, it also sends the list of changes so the server doesn't have to re-parse the
  // rule if it needs to track what changed.
  this.modifications = [];

  // Keep track of which any declarations we had to rewrite while
  // performing the requested action.
  this.changedDeclarations = {};

  // If not null, a promise that must be wait upon before |apply| can
  // do its work.
  this.editPromise = null;

  // If the |defaultIndentation| property is set, then it is used;
  // otherwise the RuleRewriter will try to compute the default
  // indentation based on the style sheet's text.  This override
  // facility is for testing.
  this.defaultIndentation = null;

  this.startInitialization(inputString);
}

RuleRewriter.prototype = {
  /**
   * An internal function to initialize the rewriter with a given
   * input string.
   *
   * @param {String} inputString the input to use
   */
  startInitialization(inputString) {
    this.inputString = inputString;
    // Whether there are any newlines in the input text.
    this.hasNewLine = /[\r\n]/.test(this.inputString);
    // The declarations.
    this.declarations = parseNamedDeclarations(
      this.isCssPropertyKnown,
      this.inputString,
      true
    );
    this.decl = null;
    this.result = null;
  },

  /**
   * An internal function to complete initialization and set some
   * properties for further processing.
   *
   * @param {Number} index The index of the property to modify
   */
  completeInitialization(index) {
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
  getIndentation(string, offset) {
    let originalOffset = offset;
    for (--offset; offset >= 0; --offset) {
      const c = string[offset];
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
  sanitizePropertyValue(text) {
    // Start by stripping any trailing ";".  This is done here to
    // avoid the case where the user types "url(" (which is turned
    // into "url(;" by the rule view before coming here), being turned
    // into "url(;)" by this code -- due to the way "url(...)" is
    // parsed as a single token.
    text = text.replace(/;$/, "");
    const lexer = getCSSLexer(text);

    let result = "";
    let previousOffset = 0;
    const parenStack = [];
    let anySanitized = false;

    // Push a closing paren on the stack.
    const pushParen = (token, closer) => {
      result =
        result +
        text.substring(previousOffset, token.startOffset) +
        text.substring(token.startOffset, token.endOffset);
      // We set the location of the paren in a funny way, to handle
      // the case where we've seen a function token, where the paren
      // appears at the end.
      parenStack.push({ closer, offset: result.length - 1 });
      previousOffset = token.endOffset;
    };

    // Pop a closing paren from the stack.
    const popSomeParens = closer => {
      while (parenStack.length) {
        const paren = parenStack.pop();

        if (paren.closer === closer) {
          return true;
        }

        // Found a non-matching closing paren, so quote it.  Note that
        // these are processed in reverse order.
        result =
          result.substring(0, paren.offset) +
          "\\" +
          result.substring(paren.offset);
        anySanitized = true;
      }
      return false;
    };

    while (true) {
      const token = lexer.nextToken();
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
            pushParen(token, "}");
            break;

          case "(":
            pushParen(token, ")");
            break;

          case "[":
            pushParen(token, "]");
            break;

          case "}":
          case ")":
          case "]":
            // Did we find an unmatched close bracket?
            if (!popSomeParens(token.text)) {
              // Copy out text from |previousOffset|.
              result += text.substring(previousOffset, token.startOffset);
              // Quote the offending symbol.
              result += "\\" + token.text;
              previousOffset = token.endOffset;
              anySanitized = true;
            }
            break;
        }
      } else if (token.tokenType === "function") {
        pushParen(token, ")");
      }
    }

    // Fix up any unmatched parens.
    popSomeParens(null);

    // Copy out any remaining text, then any needed terminators.
    result += text.substring(previousOffset, text.length);
    const eofFixup = lexer.performEOFFixup("", true);
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
  skipWhitespaceBackward(string, index) {
    for (
      --index;
      index >= 0 && (string[index] === " " || string[index] === "\t");
      --index
    ) {
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
  maybeTerminateDecl(index) {
    if (
      index < 0 ||
      index >= this.declarations.length ||
      // No need to rewrite declarations in comments.
      "commentOffsets" in this.declarations[index]
    ) {
      return;
    }

    const termDecl = this.declarations[index];
    let endIndex = termDecl.offsets[1];
    // Due to an oddity of the lexer, we might have gotten a bit of
    // extra whitespace in a trailing bad_url token -- so be sure to
    // skip that as well.
    endIndex = this.skipWhitespaceBackward(this.result, endIndex) + 1;

    const trailingText = this.result.substring(endIndex);
    if (termDecl.terminator) {
      // Insert the terminator just at the end of the declaration,
      // before any trailing whitespace.
      this.result =
        this.result.substring(0, endIndex) + termDecl.terminator + trailingText;
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
  sanitizeText(text, index) {
    const [anySanitized, sanitizedText] = this.sanitizePropertyValue(text);
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
  renameProperty(index, name, newName) {
    this.completeInitialization(index);
    this.result += CSS.escape(newName);
    // We could conceivably compute the name offsets instead so we
    // could preserve white space and comments on the LHS of the ":".
    this.completeCopying(this.decl.colonOffsets[0]);
    this.modifications.push({ type: "set", index, name, newName });
  },

  /**
   * Enable or disable a declaration
   *
   * @param {Number} index index of the property in the rule.
   * @param {String} name current name of the property
   * @param {Boolean} isEnabled true if the property should be enabled;
   *                        false if it should be disabled
   */
  setPropertyEnabled(index, name, isEnabled) {
    this.completeInitialization(index);
    const decl = this.decl;
    const priority = decl.priority;
    let copyOffset = decl.offsets[1];
    if (isEnabled) {
      // Enable it.  First see if the comment start can be deleted.
      const commentStart = decl.commentOffsets[0];
      if (EMPTY_COMMENT_START_RX.test(this.result.substring(commentStart))) {
        this.result = this.result.substring(0, commentStart);
      } else {
        this.result += "*/ ";
      }

      // Insert the name and value separately, so we can report
      // sanitization changes properly.
      const commentNamePart = this.inputString.substring(
        decl.offsets[0],
        decl.colonOffsets[1]
      );
      this.result += unescapeCSSComment(commentNamePart);

      // When uncommenting, we must be sure to sanitize the text, to
      // avoid things like /* decl: }; */, which will be accepted as
      // a property but which would break the entire style sheet.
      let newText = this.inputString.substring(
        decl.colonOffsets[1],
        decl.offsets[1]
      );
      newText = cssTrimRight(unescapeCSSComment(newText));
      this.result += this.sanitizeText(newText, index) + ";";

      // See if the comment end can be deleted.
      const trailingText = this.inputString.substring(decl.offsets[1]);
      if (EMPTY_COMMENT_END_RX.test(trailingText)) {
        copyOffset = decl.commentOffsets[1];
      } else {
        this.result += " /*";
      }
    } else {
      // Disable it.  Note that we use our special comment syntax
      // here.
      const declText = this.inputString.substring(
        decl.offsets[0],
        decl.offsets[1]
      );
      this.result +=
        "/*" +
        COMMENT_PARSING_HEURISTIC_BYPASS_CHAR +
        " " +
        escapeCSSComment(declText) +
        " */";
    }
    this.completeCopying(copyOffset);

    if (isEnabled) {
      this.modifications.push({
        type: "set",
        index,
        name,
        value: decl.value,
        priority,
      });
    } else {
      this.modifications.push({ type: "disable", index, name });
    }
  },

  /**
   * Return a promise that will be resolved to the default indentation
   * of the rule.  This is a helper for internalCreateProperty.
   *
   * @return {Promise} a promise that will be resolved to a string
   *         that holds the default indentation that should be used
   *         for edits to the rule.
   */
  async getDefaultIndentation() {
    if (!this.rule.parentStyleSheet) {
      return null;
    }

    if (this.rule.parentStyleSheet.resourceId) {
      const prefIndent = getIndentationFromPrefs();
      if (prefIndent) {
        const { indentUnit, indentWithTabs } = prefIndent;
        return indentWithTabs ? "\t" : " ".repeat(indentUnit);
      }

      const styleSheetsFront = await this.rule.targetFront.getFront(
        "stylesheets"
      );
      const { str: source } = await styleSheetsFront.getText(
        this.rule.parentStyleSheet.resourceId
      );
      const { indentUnit, indentWithTabs } = getIndentationFromString(source);
      return indentWithTabs ? "\t" : " ".repeat(indentUnit);
    }

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
   * @param {Boolean} enabled True if the new property should be
   *                          enabled, false if disabled
   * @return {Promise} a promise that is resolved when the edit has
   *                   completed
   */
  async internalCreateProperty(index, name, value, priority, enabled) {
    this.completeInitialization(index);
    let newIndentation = "";
    if (this.hasNewLine) {
      if (this.declarations.length) {
        newIndentation = this.getIndentation(
          this.inputString,
          this.declarations[0].offsets[0]
        );
      } else if (this.defaultIndentation) {
        newIndentation = this.defaultIndentation;
      } else {
        newIndentation = await this.getDefaultIndentation();
      }
    }

    this.maybeTerminateDecl(index - 1);

    // If we generally have newlines, and if skipping whitespace
    // backward stops at a newline, then insert our text before that
    // whitespace.  This ensures the indentation we computed is what
    // is actually used.
    let savedWhitespace = "";
    if (this.hasNewLine) {
      const wsOffset = this.skipWhitespaceBackward(
        this.result,
        this.result.length
      );
      if (this.result[wsOffset] === "\r" || this.result[wsOffset] === "\n") {
        savedWhitespace = this.result.substring(wsOffset + 1);
        this.result = this.result.substring(0, wsOffset + 1);
      }
    }

    let newText = CSS.escape(name) + ": " + this.sanitizeText(value, index);
    if (priority === "important") {
      newText += " !important";
    }
    newText += ";";

    if (!enabled) {
      newText =
        "/*" +
        COMMENT_PARSING_HEURISTIC_BYPASS_CHAR +
        " " +
        escapeCSSComment(newText) +
        " */";
    }

    this.result += newIndentation + newText;
    if (this.hasNewLine) {
      this.result += "\n";
    }
    this.result += savedWhitespace;

    if (this.decl) {
      // Still want to copy in the declaration previously at this
      // index.
      this.completeCopying(this.decl.offsets[0]);
    }
  },

  /**
   * Create a new declaration.
   *
   * @param {Number} index index of the property in the rule.
   * @param {String} name name of the new property
   * @param {String} value value of the new property
   * @param {String} priority priority of the new property; either
   *                          the empty string or "important"
   * @param {Boolean} enabled True if the new property should be
   *                          enabled, false if disabled
   */
  createProperty(index, name, value, priority, enabled) {
    this.editPromise = this.internalCreateProperty(
      index,
      name,
      value,
      priority,
      enabled
    );
    // Log the modification only if the created property is enabled.
    if (enabled) {
      this.modifications.push({ type: "set", index, name, value, priority });
    }
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
  setProperty(index, name, value, priority) {
    this.completeInitialization(index);
    // We might see a "set" on a previously non-existent property; in
    // that case, act like "create".
    if (!this.decl) {
      this.createProperty(index, name, value, priority, true);
      return;
    }

    // Note that this assumes that "set" never operates on disabled
    // properties.
    this.result +=
      this.inputString.substring(
        this.decl.offsets[0],
        this.decl.colonOffsets[1]
      ) + this.sanitizeText(value, index);

    if (priority === "important") {
      this.result += " !important";
    }
    this.result += ";";
    this.completeCopying(this.decl.offsets[1]);
    this.modifications.push({ type: "set", index, name, value, priority });
  },

  /**
   * Remove a declaration.
   *
   * @param {Number} index index of the property in the rule.
   * @param {String} name the name of the property to remove
   */
  removeProperty(index, name) {
    this.completeInitialization(index);

    // If asked to remove a property that does not exist, bail out.
    if (!this.decl) {
      return;
    }

    // If the property is disabled, then first enable it, and then
    // delete it.  We take this approach because we want to remove the
    // entire comment if possible; but the logic for dealing with
    // comments is hairy and already implemented in
    // setPropertyEnabled.
    if (this.decl.commentOffsets) {
      this.setPropertyEnabled(index, name, true);
      this.startInitialization(this.result);
      this.completeInitialization(index);
    }

    let copyOffset = this.decl.offsets[1];
    // Maybe removing this rule left us with a completely blank
    // line.  In this case, we'll delete the whole thing.  We only
    // bother with this if we're looking at sources that already
    // have a newline somewhere.
    if (this.hasNewLine) {
      const nlOffset = this.skipWhitespaceBackward(
        this.result,
        this.decl.offsets[0]
      );
      if (
        nlOffset < 0 ||
        this.result[nlOffset] === "\r" ||
        this.result[nlOffset] === "\n"
      ) {
        const trailingText = this.inputString.substring(copyOffset);
        const match = BLANK_LINE_RX.exec(trailingText);
        if (match) {
          this.result = this.result.substring(0, nlOffset + 1);
          copyOffset += match[0].length;
        }
      }
    }
    this.completeCopying(copyOffset);
    this.modifications.push({ type: "remove", index, name });
  },

  /**
   * An internal function to copy any trailing text to the output
   * string.
   *
   * @param {Number} copyOffset Offset into |inputString| of the
   *        final text to copy to the output string.
   */
  completeCopying(copyOffset) {
    // Add the trailing text.
    this.result += this.inputString.substring(copyOffset);
  },

  /**
   * Apply the modifications in this object to the associated rule.
   *
   * @return {Promise} A promise which will be resolved when the modifications
   *         are complete.
   */
  apply() {
    return Promise.resolve(this.editPromise).then(() => {
      return this.rule.setRuleText(this.result, this.modifications);
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
  getResult() {
    return { changed: this.changedDeclarations, text: this.result };
  },
};

/**
 * Like trimRight, but only trims CSS-allowed whitespace.
 */
function cssTrimRight(str) {
  const match = /^(.*?)[ \t\r\n\f]*$/.exec(str);
  if (match) {
    return match[1];
  }
  return str;
}

module.exports = RuleRewriter;
