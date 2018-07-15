/* vim: set ts=2 et sw=2 tw=80 filetype=javascript: */

/* Copyright 2017 Mozilla Foundation and others
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


/* fluent@aa95b1f (July 10, 2018) */

/*  eslint no-magic-numbers: [0]  */

const MAX_PLACEABLES = 100;

const entryIdentifierRe = /-?[a-zA-Z][a-zA-Z0-9_-]*/y;
const identifierRe = /[a-zA-Z][a-zA-Z0-9_-]*/y;
const functionIdentifierRe = /^[A-Z][A-Z_?-]*$/;

/**
 * The `Parser` class is responsible for parsing FTL resources.
 *
 * It's only public method is `getResource(source)` which takes an FTL string
 * and returns a two element Array with an Object of entries generated from the
 * source as the first element and an array of SyntaxError objects as the
 * second.
 *
 * This parser is optimized for runtime performance.
 *
 * There is an equivalent of this parser in syntax/parser which is
 * generating full AST which is useful for FTL tools.
 */
class RuntimeParser {
  /**
   * Parse FTL code into entries formattable by the MessageContext.
   *
   * Given a string of FTL syntax, return a map of entries that can be passed
   * to MessageContext.format and a list of errors encountered during parsing.
   *
   * @param {String} string
   * @returns {Array<Object, Array>}
   */
  getResource(string) {
    this._source = string;
    this._index = 0;
    this._length = string.length;
    this.entries = {};

    const errors = [];

    this.skipWS();
    while (this._index < this._length) {
      try {
        this.getEntry();
      } catch (e) {
        if (e instanceof SyntaxError) {
          errors.push(e);

          this.skipToNextEntryStart();
        } else {
          throw e;
        }
      }
      this.skipWS();
    }

    return [this.entries, errors];
  }

  /**
   * Parse the source string from the current index as an FTL entry
   * and add it to object's entries property.
   *
   * @private
   */
  getEntry() {
    // The index here should either be at the beginning of the file
    // or right after new line.
    if (this._index !== 0 &&
        this._source[this._index - 1] !== "\n") {
      throw this.error(`Expected an entry to start
        at the beginning of the file or on a new line.`);
    }

    const ch = this._source[this._index];

    // We don't care about comments or sections at runtime
    if (ch === "/" ||
      (ch === "#" &&
        [" ", "#", "\n"].includes(this._source[this._index + 1]))) {
      this.skipComment();
      return;
    }

    if (ch === "[") {
      this.skipSection();
      return;
    }

    this.getMessage();
  }

  /**
   * Skip the section entry from the current index.
   *
   * @private
   */
  skipSection() {
    this._index += 1;
    if (this._source[this._index] !== "[") {
      throw this.error('Expected "[[" to open a section');
    }

    this._index += 1;

    this.skipInlineWS();
    this.getVariantName();
    this.skipInlineWS();

    if (this._source[this._index] !== "]" ||
        this._source[this._index + 1] !== "]") {
      throw this.error('Expected "]]" to close a section');
    }

    this._index += 2;
  }

  /**
   * Parse the source string from the current index as an FTL message
   * and add it to the entries property on the Parser.
   *
   * @private
   */
  getMessage() {
    const id = this.getEntryIdentifier();

    this.skipInlineWS();

    if (this._source[this._index] === "=") {
      this._index++;
    }

    this.skipInlineWS();

    const val = this.getPattern();

    if (id.startsWith("-") && val === null) {
      throw this.error("Expected term to have a value");
    }

    let attrs = null;

    if (this._source[this._index] === " ") {
      const lineStart = this._index;
      this.skipInlineWS();

      if (this._source[this._index] === ".") {
        this._index = lineStart;
        attrs = this.getAttributes();
      }
    }

    if (attrs === null && typeof val === "string") {
      this.entries[id] = val;
    } else {
      if (val === null && attrs === null) {
        throw this.error("Expected message to have a value or attributes");
      }

      this.entries[id] = {};

      if (val !== null) {
        this.entries[id].val = val;
      }

      if (attrs !== null) {
        this.entries[id].attrs = attrs;
      }
    }
  }

  /**
   * Skip whitespace.
   *
   * @private
   */
  skipWS() {
    let ch = this._source[this._index];
    while (ch === " " || ch === "\n" || ch === "\t" || ch === "\r") {
      ch = this._source[++this._index];
    }
  }

  /**
   * Skip inline whitespace (space and \t).
   *
   * @private
   */
  skipInlineWS() {
    let ch = this._source[this._index];
    while (ch === " " || ch === "\t") {
      ch = this._source[++this._index];
    }
  }

  /**
   * Skip blank lines.
   *
   * @private
   */
  skipBlankLines() {
    while (true) {
      const ptr = this._index;

      this.skipInlineWS();

      if (this._source[this._index] === "\n") {
        this._index += 1;
      } else {
        this._index = ptr;
        break;
      }
    }
  }

  /**
   * Get identifier using the provided regex.
   *
   * By default this will get identifiers of public messages, attributes and
   * external arguments (without the $).
   *
   * @returns {String}
   * @private
   */
  getIdentifier(re = identifierRe) {
    re.lastIndex = this._index;
    const result = re.exec(this._source);

    if (result === null) {
      this._index += 1;
      throw this.error(`Expected an identifier [${re.toString()}]`);
    }

    this._index = re.lastIndex;
    return result[0];
  }

  /**
   * Get identifier of a Message or a Term (staring with a dash).
   *
   * @returns {String}
   * @private
   */
  getEntryIdentifier() {
    return this.getIdentifier(entryIdentifierRe);
  }

  /**
   * Get Variant name.
   *
   * @returns {Object}
   * @private
   */
  getVariantName() {
    let name = "";

    const start = this._index;
    let cc = this._source.charCodeAt(this._index);

    if ((cc >= 97 && cc <= 122) || // a-z
        (cc >= 65 && cc <= 90) || // A-Z
        cc === 95 || cc === 32) { // _ <space>
      cc = this._source.charCodeAt(++this._index);
    } else {
      throw this.error("Expected a keyword (starting with [a-zA-Z_])");
    }

    while ((cc >= 97 && cc <= 122) || // a-z
           (cc >= 65 && cc <= 90) || // A-Z
           (cc >= 48 && cc <= 57) || // 0-9
           cc === 95 || cc === 45 || cc === 32) { // _- <space>
      cc = this._source.charCodeAt(++this._index);
    }

    // If we encountered the end of name, we want to test if the last
    // collected character is a space.
    // If it is, we will backtrack to the last non-space character because
    // the keyword cannot end with a space character.
    while (this._source.charCodeAt(this._index - 1) === 32) {
      this._index--;
    }

    name += this._source.slice(start, this._index);

    return { type: "varname", name };
  }

  /**
   * Get simple string argument enclosed in `"`.
   *
   * @returns {String}
   * @private
   */
  getString() {
    const start = this._index + 1;

    while (++this._index < this._length) {
      const ch = this._source[this._index];

      if (ch === '"') {
        break;
      }

      if (ch === "\n") {
        throw this.error("Unterminated string expression");
      }
    }

    return this._source.substring(start, this._index++);
  }

  /**
   * Parses a Message pattern.
   * Message Pattern may be a simple string or an array of strings
   * and placeable expressions.
   *
   * @returns {String|Array}
   * @private
   */
  getPattern() {
    // We're going to first try to see if the pattern is simple.
    // If it is we can just look for the end of the line and read the string.
    //
    // Then, if either the line contains a placeable opening `{` or the
    // next line starts an indentation, we switch to complex pattern.
    const start = this._index;
    let eol = this._source.indexOf("\n", this._index);

    if (eol === -1) {
      eol = this._length;
    }

    const firstLineContent = start !== eol ?
      this._source.slice(start, eol) : null;

    if (firstLineContent && firstLineContent.includes("{")) {
      return this.getComplexPattern();
    }

    this._index = eol + 1;

    this.skipBlankLines();

    if (this._source[this._index] !== " ") {
      // No indentation means we're done with this message. Callers should check
      // if the return value here is null. It may be OK for messages, but not OK
      // for terms, attributes and variants.
      return firstLineContent;
    }

    const lineStart = this._index;

    this.skipInlineWS();

    if (this._source[this._index] === ".") {
      // The pattern is followed by an attribute. Rewind _index to the first
      // column of the current line as expected by getAttributes.
      this._index = lineStart;
      return firstLineContent;
    }

    if (firstLineContent) {
      // It's a multiline pattern which started on the same line as the
      // identifier. Reparse the whole pattern to make sure we get all of it.
      this._index = start;
    }

    return this.getComplexPattern();
  }

  /**
   * Parses a complex Message pattern.
   * This function is called by getPattern when the message is multiline,
   * or contains escape chars or placeables.
   * It does full parsing of complex patterns.
   *
   * @returns {Array}
   * @private
   */
  /* eslint-disable complexity */
  getComplexPattern() {
    let buffer = "";
    const content = [];
    let placeables = 0;

    let ch = this._source[this._index];

    while (this._index < this._length) {
      // This block handles multi-line strings combining strings separated
      // by new line.
      if (ch === "\n") {
        this._index++;

        // We want to capture the start and end pointers
        // around blank lines and add them to the buffer
        // but only if the blank lines are in the middle
        // of the string.
        const blankLinesStart = this._index;
        this.skipBlankLines();
        const blankLinesEnd = this._index;


        if (this._source[this._index] !== " ") {
          break;
        }
        this.skipInlineWS();

        if (this._source[this._index] === "}" ||
            this._source[this._index] === "[" ||
            this._source[this._index] === "*" ||
            this._source[this._index] === ".") {
          this._index = blankLinesEnd;
          break;
        }

        buffer += this._source.substring(blankLinesStart, blankLinesEnd);

        if (buffer.length || content.length) {
          buffer += "\n";
        }
        ch = this._source[this._index];
        continue;
      } else if (ch === "\\") {
        const ch2 = this._source[this._index + 1];
        if (ch2 === '"' || ch2 === "{" || ch2 === "\\") {
          ch = ch2;
          this._index++;
        }
      } else if (ch === "{") {
        // Push the buffer to content array right before placeable
        if (buffer.length) {
          content.push(buffer);
        }
        if (placeables > MAX_PLACEABLES - 1) {
          throw this.error(
            `Too many placeables, maximum allowed is ${MAX_PLACEABLES}`);
        }
        buffer = "";
        content.push(this.getPlaceable());

        this._index++;

        ch = this._source[this._index];
        placeables++;
        continue;
      }

      if (ch) {
        buffer += ch;
      }
      this._index++;
      ch = this._source[this._index];
    }

    if (content.length === 0) {
      return buffer.length ? buffer : null;
    }

    if (buffer.length) {
      content.push(buffer);
    }

    return content;
  }
  /* eslint-enable complexity */

  /**
   * Parses a single placeable in a Message pattern and returns its
   * expression.
   *
   * @returns {Object}
   * @private
   */
  getPlaceable() {
    const start = ++this._index;

    this.skipWS();

    if (this._source[this._index] === "*" ||
       (this._source[this._index] === "[" &&
        this._source[this._index + 1] !== "]")) {
      const variants = this.getVariants();

      return {
        type: "sel",
        exp: null,
        vars: variants[0],
        def: variants[1]
      };
    }

    // Rewind the index and only support in-line white-space now.
    this._index = start;
    this.skipInlineWS();

    const selector = this.getSelectorExpression();

    this.skipWS();

    const ch = this._source[this._index];

    if (ch === "}") {
      if (selector.type === "attr" && selector.id.name.startsWith("-")) {
        throw this.error(
          "Attributes of private messages cannot be interpolated."
        );
      }

      return selector;
    }

    if (ch !== "-" || this._source[this._index + 1] !== ">") {
      throw this.error('Expected "}" or "->"');
    }

    if (selector.type === "ref") {
      throw this.error("Message references cannot be used as selectors.");
    }

    if (selector.type === "var") {
      throw this.error("Variants cannot be used as selectors.");
    }

    if (selector.type === "attr" && !selector.id.name.startsWith("-")) {
      throw this.error(
        "Attributes of public messages cannot be used as selectors."
      );
    }


    this._index += 2; // ->

    this.skipInlineWS();

    if (this._source[this._index] !== "\n") {
      throw this.error("Variants should be listed in a new line");
    }

    this.skipWS();

    const variants = this.getVariants();

    if (variants[0].length === 0) {
      throw this.error("Expected members for the select expression");
    }

    return {
      type: "sel",
      exp: selector,
      vars: variants[0],
      def: variants[1]
    };
  }

  /**
   * Parses a selector expression.
   *
   * @returns {Object}
   * @private
   */
  getSelectorExpression() {
    const literal = this.getLiteral();

    if (literal.type !== "ref") {
      return literal;
    }

    if (this._source[this._index] === ".") {
      this._index++;

      const name = this.getIdentifier();
      this._index++;
      return {
        type: "attr",
        id: literal,
        name
      };
    }

    if (this._source[this._index] === "[") {
      this._index++;

      const key = this.getVariantKey();
      this._index++;
      return {
        type: "var",
        id: literal,
        key
      };
    }

    if (this._source[this._index] === "(") {
      this._index++;
      const args = this.getCallArgs();

      if (!functionIdentifierRe.test(literal.name)) {
        throw this.error("Function names must be all upper-case");
      }

      this._index++;

      literal.type = "fun";

      return {
        type: "call",
        fun: literal,
        args
      };
    }

    return literal;
  }

  /**
   * Parses call arguments for a CallExpression.
   *
   * @returns {Array}
   * @private
   */
  getCallArgs() {
    const args = [];

    while (this._index < this._length) {
      this.skipInlineWS();

      if (this._source[this._index] === ")") {
        return args;
      }

      const exp = this.getSelectorExpression();

      // MessageReference in this place may be an entity reference, like:
      // `call(foo)`, or, if it's followed by `:` it will be a key-value pair.
      if (exp.type !== "ref") {
        args.push(exp);
      } else {
        this.skipInlineWS();

        if (this._source[this._index] === ":") {
          this._index++;
          this.skipInlineWS();

          const val = this.getSelectorExpression();

          // If the expression returned as a value of the argument
          // is not a quote delimited string or number, throw.
          //
          // We don't have to check here if the pattern is quote delimited
          // because that's the only type of string allowed in expressions.
          if (typeof val === "string" ||
              Array.isArray(val) ||
              val.type === "num") {
            args.push({
              type: "narg",
              name: exp.name,
              val
            });
          } else {
            this._index = this._source.lastIndexOf(":", this._index) + 1;
            throw this.error(
              "Expected string in quotes, number.");
          }

        } else {
          args.push(exp);
        }
      }

      this.skipInlineWS();

      if (this._source[this._index] === ")") {
        break;
      } else if (this._source[this._index] === ",") {
        this._index++;
      } else {
        throw this.error('Expected "," or ")"');
      }
    }

    return args;
  }

  /**
   * Parses an FTL Number.
   *
   * @returns {Object}
   * @private
   */
  getNumber() {
    let num = "";
    let cc = this._source.charCodeAt(this._index);

    // The number literal may start with negative sign `-`.
    if (cc === 45) {
      num += "-";
      cc = this._source.charCodeAt(++this._index);
    }

    // next, we expect at least one digit
    if (cc < 48 || cc > 57) {
      throw this.error(`Unknown literal "${num}"`);
    }

    // followed by potentially more digits
    while (cc >= 48 && cc <= 57) {
      num += this._source[this._index++];
      cc = this._source.charCodeAt(this._index);
    }

    // followed by an optional decimal separator `.`
    if (cc === 46) {
      num += this._source[this._index++];
      cc = this._source.charCodeAt(this._index);

      // followed by at least one digit
      if (cc < 48 || cc > 57) {
        throw this.error(`Unknown literal "${num}"`);
      }

      // and optionally more digits
      while (cc >= 48 && cc <= 57) {
        num += this._source[this._index++];
        cc = this._source.charCodeAt(this._index);
      }
    }

    return {
      type: "num",
      val: num
    };
  }

  /**
   * Parses a list of Message attributes.
   *
   * @returns {Object}
   * @private
   */
  getAttributes() {
    const attrs = {};

    while (this._index < this._length) {
      if (this._source[this._index] !== " ") {
        break;
      }
      this.skipInlineWS();

      if (this._source[this._index] !== ".") {
        break;
      }
      this._index++;

      const key = this.getIdentifier();

      this.skipInlineWS();

      if (this._source[this._index] !== "=") {
        throw this.error('Expected "="');
      }
      this._index++;

      this.skipInlineWS();

      const val = this.getPattern();

      if (val === null) {
        throw this.error("Expected attribute to have a value");
      }

      if (typeof val === "string") {
        attrs[key] = val;
      } else {
        attrs[key] = {
          val
        };
      }

      this.skipBlankLines();
    }

    return attrs;
  }

  /**
   * Parses a list of Selector variants.
   *
   * @returns {Array}
   * @private
   */
  getVariants() {
    const variants = [];
    let index = 0;
    let defaultIndex;

    while (this._index < this._length) {
      const ch = this._source[this._index];

      if ((ch !== "[" || this._source[this._index + 1] === "[") &&
          ch !== "*") {
        break;
      }
      if (ch === "*") {
        this._index++;
        defaultIndex = index;
      }

      if (this._source[this._index] !== "[") {
        throw this.error('Expected "["');
      }

      this._index++;

      const key = this.getVariantKey();

      this.skipInlineWS();

      const val = this.getPattern();

      if (val === null) {
        throw this.error("Expected variant to have a value");
      }

      variants[index++] = {key, val};

      this.skipWS();
    }

    return [variants, defaultIndex];
  }

  /**
   * Parses a Variant key.
   *
   * @returns {String}
   * @private
   */
  getVariantKey() {
    // VariantKey may be a Keyword or Number

    const cc = this._source.charCodeAt(this._index);
    let literal;

    if ((cc >= 48 && cc <= 57) || cc === 45) {
      literal = this.getNumber();
    } else {
      literal = this.getVariantName();
    }

    if (this._source[this._index] !== "]") {
      throw this.error('Expected "]"');
    }

    this._index++;
    return literal;
  }

  /**
   * Parses an FTL literal.
   *
   * @returns {Object}
   * @private
   */
  getLiteral() {
    const cc0 = this._source.charCodeAt(this._index);

    if (cc0 === 36) { // $
      this._index++;
      return {
        type: "ext",
        name: this.getIdentifier()
      };
    }

    const cc1 = cc0 === 45 // -
      // Peek at the next character after the dash.
      ? this._source.charCodeAt(this._index + 1)
      // Or keep using the character at the current index.
      : cc0;

    if ((cc1 >= 97 && cc1 <= 122) || // a-z
        (cc1 >= 65 && cc1 <= 90)) { // A-Z
      return {
        type: "ref",
        name: this.getEntryIdentifier()
      };
    }

    if ((cc1 >= 48 && cc1 <= 57)) { // 0-9
      return this.getNumber();
    }

    if (cc0 === 34) { // "
      return this.getString();
    }

    throw this.error("Expected literal");
  }

  /**
   * Skips an FTL comment.
   *
   * @private
   */
  skipComment() {
    // At runtime, we don't care about comments so we just have
    // to parse them properly and skip their content.
    let eol = this._source.indexOf("\n", this._index);

    while (eol !== -1 &&
      ((this._source[eol + 1] === "/" && this._source[eol + 2] === "/") ||
       (this._source[eol + 1] === "#" &&
         [" ", "#"].includes(this._source[eol + 2])))) {
      this._index = eol + 3;

      eol = this._source.indexOf("\n", this._index);

      if (eol === -1) {
        break;
      }
    }

    if (eol === -1) {
      this._index = this._length;
    } else {
      this._index = eol + 1;
    }
  }

  /**
   * Creates a new SyntaxError object with a given message.
   *
   * @param {String} message
   * @returns {Object}
   * @private
   */
  error(message) {
    return new SyntaxError(message);
  }

  /**
   * Skips to the beginning of a next entry after the current position.
   * This is used to mark the boundary of junk entry in case of error,
   * and recover from the returned position.
   *
   * @private
   */
  skipToNextEntryStart() {
    let start = this._index;

    while (true) {
      if (start === 0 || this._source[start - 1] === "\n") {
        const cc = this._source.charCodeAt(start);

        if ((cc >= 97 && cc <= 122) || // a-z
            (cc >= 65 && cc <= 90) || // A-Z
             cc === 47 || cc === 91) { // /[
          this._index = start;
          return;
        }
      }

      start = this._source.indexOf("\n", start);

      if (start === -1) {
        this._index = this._length;
        return;
      }
      start++;
    }
  }
}

/**
 * Parses an FTL string using RuntimeParser and returns the generated
 * object with entries and a list of errors.
 *
 * @param {String} string
 * @returns {Array<Object, Array>}
 */
function parse(string) {
  const parser = new RuntimeParser();
  return parser.getResource(string);
}

/* global Intl */

/**
 * The `FluentType` class is the base of Fluent's type system.
 *
 * Fluent types wrap JavaScript values and store additional configuration for
 * them, which can then be used in the `toString` method together with a proper
 * `Intl` formatter.
 */
class FluentType {

  /**
   * Create an `FluentType` instance.
   *
   * @param   {Any}    value - JavaScript value to wrap.
   * @param   {Object} opts  - Configuration.
   * @returns {FluentType}
   */
  constructor(value, opts) {
    this.value = value;
    this.opts = opts;
  }

  /**
   * Unwrap the raw value stored by this `FluentType`.
   *
   * @returns {Any}
   */
  valueOf() {
    return this.value;
  }

  /**
   * Format this instance of `FluentType` to a string.
   *
   * Formatted values are suitable for use outside of the `MessageContext`.
   * This method can use `Intl` formatters memoized by the `MessageContext`
   * instance passed as an argument.
   *
   * @param   {MessageContext} [ctx]
   * @returns {string}
   */
  toString() {
    throw new Error("Subclasses of FluentType must implement toString.");
  }
}

class FluentNone extends FluentType {
  toString() {
    return this.value || "???";
  }
}

class FluentNumber extends FluentType {
  constructor(value, opts) {
    super(parseFloat(value), opts);
  }

  toString(ctx) {
    try {
      const nf = ctx._memoizeIntlObject(
        Intl.NumberFormat, this.opts
      );
      return nf.format(this.value);
    } catch (e) {
      // XXX Report the error.
      return this.value;
    }
  }

  /**
   * Compare the object with another instance of a FluentType.
   *
   * @param   {MessageContext} ctx
   * @param   {FluentType}     other
   * @returns {bool}
   */
  match(ctx, other) {
    if (other instanceof FluentNumber) {
      return this.value === other.value;
    }
    return false;
  }
}

class FluentDateTime extends FluentType {
  constructor(value, opts) {
    super(new Date(value), opts);
  }

  toString(ctx) {
    try {
      const dtf = ctx._memoizeIntlObject(
        Intl.DateTimeFormat, this.opts
      );
      return dtf.format(this.value);
    } catch (e) {
      // XXX Report the error.
      return this.value;
    }
  }
}

class FluentSymbol extends FluentType {
  toString() {
    return this.value;
  }

  /**
   * Compare the object with another instance of a FluentType.
   *
   * @param   {MessageContext} ctx
   * @param   {FluentType}     other
   * @returns {bool}
   */
  match(ctx, other) {
    if (other instanceof FluentSymbol) {
      return this.value === other.value;
    } else if (typeof other === "string") {
      return this.value === other;
    } else if (other instanceof FluentNumber) {
      const pr = ctx._memoizeIntlObject(
        Intl.PluralRules, other.opts
      );
      return this.value === pr.select(other.value);
    }
    return false;
  }
}

/**
 * @overview
 *
 * The FTL resolver ships with a number of functions built-in.
 *
 * Each function take two arguments:
 *   - args - an array of positional args
 *   - opts - an object of key-value args
 *
 * Arguments to functions are guaranteed to already be instances of
 * `FluentType`.  Functions must return `FluentType` objects as well.
 */

const builtins = {
  "NUMBER": ([arg], opts) =>
    new FluentNumber(arg.valueOf(), merge(arg.opts, opts)),
  "DATETIME": ([arg], opts) =>
    new FluentDateTime(arg.valueOf(), merge(arg.opts, opts)),
};

function merge(argopts, opts) {
  return Object.assign({}, argopts, values(opts));
}

function values(opts) {
  const unwrapped = {};
  for (const [name, opt] of Object.entries(opts)) {
    unwrapped[name] = opt.valueOf();
  }
  return unwrapped;
}

/**
 * @overview
 *
 * The role of the Fluent resolver is to format a translation object to an
 * instance of `FluentType` or an array of instances.
 *
 * Translations can contain references to other messages or external arguments,
 * conditional logic in form of select expressions, traits which describe their
 * grammatical features, and can use Fluent builtins which make use of the
 * `Intl` formatters to format numbers, dates, lists and more into the
 * context's language.  See the documentation of the Fluent syntax for more
 * information.
 *
 * In case of errors the resolver will try to salvage as much of the
 * translation as possible.  In rare situations where the resolver didn't know
 * how to recover from an error it will return an instance of `FluentNone`.
 *
 * `MessageReference`, `VariantExpression`, `AttributeExpression` and
 * `SelectExpression` resolve to raw Runtime Entries objects and the result of
 * the resolution needs to be passed into `Type` to get their real value.
 * This is useful for composing expressions.  Consider:
 *
 *     brand-name[nominative]
 *
 * which is a `VariantExpression` with properties `id: MessageReference` and
 * `key: Keyword`.  If `MessageReference` was resolved eagerly, it would
 * instantly resolve to the value of the `brand-name` message.  Instead, we
 * want to get the message object and look for its `nominative` variant.
 *
 * All other expressions (except for `FunctionReference` which is only used in
 * `CallExpression`) resolve to an instance of `FluentType`.  The caller should
 * use the `toString` method to convert the instance to a native value.
 *
 *
 * All functions in this file pass around a special object called `env`.
 * This object stores a set of elements used by all resolve functions:
 *
 *  * {MessageContext} ctx
 *      context for which the given resolution is happening
 *  * {Object} args
 *      list of developer provided arguments that can be used
 *  * {Array} errors
 *      list of errors collected while resolving
 *  * {WeakSet} dirty
 *      Set of patterns already encountered during this resolution.
 *      This is used to prevent cyclic resolutions.
 */

// Prevent expansion of too long placeables.
const MAX_PLACEABLE_LENGTH = 2500;

// Unicode bidi isolation characters.
const FSI = "\u2068";
const PDI = "\u2069";


/**
 * Helper for choosing the default value from a set of members.
 *
 * Used in SelectExpressions and Type.
 *
 * @param   {Object} env
 *    Resolver environment object.
 * @param   {Object} members
 *    Hash map of variants from which the default value is to be selected.
 * @param   {Number} def
 *    The index of the default variant.
 * @returns {FluentType}
 * @private
 */
function DefaultMember(env, members, def) {
  if (members[def]) {
    return members[def];
  }

  const { errors } = env;
  errors.push(new RangeError("No default"));
  return new FluentNone();
}


/**
 * Resolve a reference to another message.
 *
 * @param   {Object} env
 *    Resolver environment object.
 * @param   {Object} id
 *    The identifier of the message to be resolved.
 * @param   {String} id.name
 *    The name of the identifier.
 * @returns {FluentType}
 * @private
 */
function MessageReference(env, {name}) {
  const { ctx, errors } = env;
  const message = name.startsWith("-")
    ? ctx._terms.get(name)
    : ctx._messages.get(name);

  if (!message) {
    const err = name.startsWith("-")
      ? new ReferenceError(`Unknown term: ${name}`)
      : new ReferenceError(`Unknown message: ${name}`);
    errors.push(err);
    return new FluentNone(name);
  }

  return message;
}

/**
 * Resolve a variant expression to the variant object.
 *
 * @param   {Object} env
 *    Resolver environment object.
 * @param   {Object} expr
 *    An expression to be resolved.
 * @param   {Object} expr.id
 *    An Identifier of a message for which the variant is resolved.
 * @param   {Object} expr.id.name
 *    Name a message for which the variant is resolved.
 * @param   {Object} expr.key
 *    Variant key to be resolved.
 * @returns {FluentType}
 * @private
 */
function VariantExpression(env, {id, key}) {
  const message = MessageReference(env, id);
  if (message instanceof FluentNone) {
    return message;
  }

  const { ctx, errors } = env;
  const keyword = Type(env, key);

  function isVariantList(node) {
    return Array.isArray(node) &&
      node[0].type === "sel" &&
      node[0].exp === null;
  }

  if (isVariantList(message.val)) {
    // Match the specified key against keys of each variant, in order.
    for (const variant of message.val[0].vars) {
      const variantKey = Type(env, variant.key);
      if (keyword.match(ctx, variantKey)) {
        return variant;
      }
    }
  }

  errors.push(new ReferenceError(`Unknown variant: ${keyword.toString(ctx)}`));
  return Type(env, message);
}


/**
 * Resolve an attribute expression to the attribute object.
 *
 * @param   {Object} env
 *    Resolver environment object.
 * @param   {Object} expr
 *    An expression to be resolved.
 * @param   {String} expr.id
 *    An ID of a message for which the attribute is resolved.
 * @param   {String} expr.name
 *    Name of the attribute to be resolved.
 * @returns {FluentType}
 * @private
 */
function AttributeExpression(env, {id, name}) {
  const message = MessageReference(env, id);
  if (message instanceof FluentNone) {
    return message;
  }

  if (message.attrs) {
    // Match the specified name against keys of each attribute.
    for (const attrName in message.attrs) {
      if (name === attrName) {
        return message.attrs[name];
      }
    }
  }

  const { errors } = env;
  errors.push(new ReferenceError(`Unknown attribute: ${name}`));
  return Type(env, message);
}

/**
 * Resolve a select expression to the member object.
 *
 * @param   {Object} env
 *    Resolver environment object.
 * @param   {Object} expr
 *    An expression to be resolved.
 * @param   {String} expr.exp
 *    Selector expression
 * @param   {Array} expr.vars
 *    List of variants for the select expression.
 * @param   {Number} expr.def
 *    Index of the default variant.
 * @returns {FluentType}
 * @private
 */
function SelectExpression(env, {exp, vars, def}) {
  if (exp === null) {
    return DefaultMember(env, vars, def);
  }

  const selector = Type(env, exp);
  if (selector instanceof FluentNone) {
    return DefaultMember(env, vars, def);
  }

  // Match the selector against keys of each variant, in order.
  for (const variant of vars) {
    const key = Type(env, variant.key);
    const keyCanMatch =
      key instanceof FluentNumber || key instanceof FluentSymbol;

    if (!keyCanMatch) {
      continue;
    }

    const { ctx } = env;

    if (key.match(ctx, selector)) {
      return variant;
    }
  }

  return DefaultMember(env, vars, def);
}


/**
 * Resolve expression to a Fluent type.
 *
 * JavaScript strings are a special case.  Since they natively have the
 * `toString` method they can be used as if they were a Fluent type without
 * paying the cost of creating a instance of one.
 *
 * @param   {Object} env
 *    Resolver environment object.
 * @param   {Object} expr
 *    An expression object to be resolved into a Fluent type.
 * @returns {FluentType}
 * @private
 */
function Type(env, expr) {
  // A fast-path for strings which are the most common case, and for
  // `FluentNone` which doesn't require any additional logic.
  if (typeof expr === "string") {
    return env.ctx._transform(expr);
  }
  if (expr instanceof FluentNone) {
    return expr;
  }

  // The Runtime AST (Entries) encodes patterns (complex strings with
  // placeables) as Arrays.
  if (Array.isArray(expr)) {
    return Pattern(env, expr);
  }


  switch (expr.type) {
    case "varname":
      return new FluentSymbol(expr.name);
    case "num":
      return new FluentNumber(expr.val);
    case "ext":
      return ExternalArgument(env, expr);
    case "fun":
      return FunctionReference(env, expr);
    case "call":
      return CallExpression(env, expr);
    case "ref": {
      const message = MessageReference(env, expr);
      return Type(env, message);
    }
    case "attr": {
      const attr = AttributeExpression(env, expr);
      return Type(env, attr);
    }
    case "var": {
      const variant = VariantExpression(env, expr);
      return Type(env, variant);
    }
    case "sel": {
      const member = SelectExpression(env, expr);
      return Type(env, member);
    }
    case undefined: {
      // If it's a node with a value, resolve the value.
      if (expr.val !== null && expr.val !== undefined) {
        return Type(env, expr.val);
      }

      const { errors } = env;
      errors.push(new RangeError("No value"));
      return new FluentNone();
    }
    default:
      return new FluentNone();
  }
}

/**
 * Resolve a reference to an external argument.
 *
 * @param   {Object} env
 *    Resolver environment object.
 * @param   {Object} expr
 *    An expression to be resolved.
 * @param   {String} expr.name
 *    Name of an argument to be returned.
 * @returns {FluentType}
 * @private
 */
function ExternalArgument(env, {name}) {
  const { args, errors } = env;

  if (!args || !args.hasOwnProperty(name)) {
    errors.push(new ReferenceError(`Unknown external: ${name}`));
    return new FluentNone(name);
  }

  const arg = args[name];

  // Return early if the argument already is an instance of FluentType.
  if (arg instanceof FluentType) {
    return arg;
  }

  // Convert the argument to a Fluent type.
  switch (typeof arg) {
    case "string":
      return arg;
    case "number":
      return new FluentNumber(arg);
    case "object":
      if (arg instanceof Date) {
        return new FluentDateTime(arg);
      }
    default:
      errors.push(
        new TypeError(`Unsupported external type: ${name}, ${typeof arg}`)
      );
      return new FluentNone(name);
  }
}

/**
 * Resolve a reference to a function.
 *
 * @param   {Object}  env
 *    Resolver environment object.
 * @param   {Object} expr
 *    An expression to be resolved.
 * @param   {String} expr.name
 *    Name of the function to be returned.
 * @returns {Function}
 * @private
 */
function FunctionReference(env, {name}) {
  // Some functions are built-in.  Others may be provided by the runtime via
  // the `MessageContext` constructor.
  const { ctx: { _functions }, errors } = env;
  const func = _functions[name] || builtins[name];

  if (!func) {
    errors.push(new ReferenceError(`Unknown function: ${name}()`));
    return new FluentNone(`${name}()`);
  }

  if (typeof func !== "function") {
    errors.push(new TypeError(`Function ${name}() is not callable`));
    return new FluentNone(`${name}()`);
  }

  return func;
}

/**
 * Resolve a call to a Function with positional and key-value arguments.
 *
 * @param   {Object} env
 *    Resolver environment object.
 * @param   {Object} expr
 *    An expression to be resolved.
 * @param   {Object} expr.fun
 *    FTL Function object.
 * @param   {Array} expr.args
 *    FTL Function argument list.
 * @returns {FluentType}
 * @private
 */
function CallExpression(env, {fun, args}) {
  const callee = FunctionReference(env, fun);

  if (callee instanceof FluentNone) {
    return callee;
  }

  const posargs = [];
  const keyargs = {};

  for (const arg of args) {
    if (arg.type === "narg") {
      keyargs[arg.name] = Type(env, arg.val);
    } else {
      posargs.push(Type(env, arg));
    }
  }

  try {
    return callee(posargs, keyargs);
  } catch (e) {
    // XXX Report errors.
    return new FluentNone();
  }
}

/**
 * Resolve a pattern (a complex string with placeables).
 *
 * @param   {Object} env
 *    Resolver environment object.
 * @param   {Array} ptn
 *    Array of pattern elements.
 * @returns {Array}
 * @private
 */
function Pattern(env, ptn) {
  const { ctx, dirty, errors } = env;

  if (dirty.has(ptn)) {
    errors.push(new RangeError("Cyclic reference"));
    return new FluentNone();
  }

  // Tag the pattern as dirty for the purpose of the current resolution.
  dirty.add(ptn);
  const result = [];

  // Wrap interpolations with Directional Isolate Formatting characters
  // only when the pattern has more than one element.
  const useIsolating = ctx._useIsolating && ptn.length > 1;

  for (const elem of ptn) {
    if (typeof elem === "string") {
      result.push(ctx._transform(elem));
      continue;
    }

    const part = Type(env, elem).toString(ctx);

    if (useIsolating) {
      result.push(FSI);
    }

    if (part.length > MAX_PLACEABLE_LENGTH) {
      errors.push(
        new RangeError(
          "Too many characters in placeable " +
          `(${part.length}, max allowed is ${MAX_PLACEABLE_LENGTH})`
        )
      );
      result.push(part.slice(MAX_PLACEABLE_LENGTH));
    } else {
      result.push(part);
    }

    if (useIsolating) {
      result.push(PDI);
    }
  }

  dirty.delete(ptn);
  return result.join("");
}

/**
 * Format a translation into a string.
 *
 * @param   {MessageContext} ctx
 *    A MessageContext instance which will be used to resolve the
 *    contextual information of the message.
 * @param   {Object}         args
 *    List of arguments provided by the developer which can be accessed
 *    from the message.
 * @param   {Object}         message
 *    An object with the Message to be resolved.
 * @param   {Array}          errors
 *    An error array that any encountered errors will be appended to.
 * @returns {FluentType}
 */
function resolve(ctx, args, message, errors = []) {
  const env = {
    ctx, args, errors, dirty: new WeakSet()
  };
  return Type(env, message).toString(ctx);
}

/**
 * Fluent Resource is a structure storing a map
 * of localization entries.
 */
class FluentResource extends Map {
  constructor(entries, errors = []) {
    super(entries);
    this.errors = errors;
  }

  static fromString(source) {
    const [entries, errors] = parse(source);
    return new FluentResource(Object.entries(entries), errors);
  }
}

/**
 * Message contexts are single-language stores of translations.  They are
 * responsible for parsing translation resources in the Fluent syntax and can
 * format translation units (entities) to strings.
 *
 * Always use `MessageContext.format` to retrieve translation units from
 * a context.  Translations can contain references to other entities or
 * external arguments, conditional logic in form of select expressions, traits
 * which describe their grammatical features, and can use Fluent builtins which
 * make use of the `Intl` formatters to format numbers, dates, lists and more
 * into the context's language.  See the documentation of the Fluent syntax for
 * more information.
 */
class MessageContext {

  /**
   * Create an instance of `MessageContext`.
   *
   * The `locales` argument is used to instantiate `Intl` formatters used by
   * translations.  The `options` object can be used to configure the context.
   *
   * Examples:
   *
   *     const ctx = new MessageContext(locales);
   *
   *     const ctx = new MessageContext(locales, { useIsolating: false });
   *
   *     const ctx = new MessageContext(locales, {
   *       useIsolating: true,
   *       functions: {
   *         NODE_ENV: () => process.env.NODE_ENV
   *       }
   *     });
   *
   * Available options:
   *
   *   - `functions` - an object of additional functions available to
   *                   translations as builtins.
   *
   *   - `useIsolating` - boolean specifying whether to use Unicode isolation
   *                    marks (FSI, PDI) for bidi interpolations.
   *
   *   - `transform` - a function used to transform string parts of patterns.
   *
   * @param   {string|Array<string>} locales - Locale or locales of the context
   * @param   {Object} [options]
   * @returns {MessageContext}
   */
  constructor(locales, {
    functions = {},
    useIsolating = true,
    transform = v => v
  } = {}) {
    this.locales = Array.isArray(locales) ? locales : [locales];

    this._terms = new Map();
    this._messages = new Map();
    this._functions = functions;
    this._useIsolating = useIsolating;
    this._transform = transform;
    this._intls = new WeakMap();
  }

  /*
   * Return an iterator over public `[id, message]` pairs.
   *
   * @returns {Iterator}
   */
  get messages() {
    return this._messages[Symbol.iterator]();
  }

  /*
   * Check if a message is present in the context.
   *
   * @param {string} id - The identifier of the message to check.
   * @returns {bool}
   */
  hasMessage(id) {
    return this._messages.has(id);
  }

  /*
   * Return the internal representation of a message.
   *
   * The internal representation should only be used as an argument to
   * `MessageContext.format`.
   *
   * @param {string} id - The identifier of the message to check.
   * @returns {Any}
   */
  getMessage(id) {
    return this._messages.get(id);
  }

  /**
   * Add a translation resource to the context.
   *
   * The translation resource must use the Fluent syntax.  It will be parsed by
   * the context and each translation unit (message) will be available in the
   * context by its identifier.
   *
   *     ctx.addMessages('foo = Foo');
   *     ctx.getMessage('foo');
   *
   *     // Returns a raw representation of the 'foo' message.
   *
   * Parsed entities should be formatted with the `format` method in case they
   * contain logic (references, select expressions etc.).
   *
   * @param   {string} source - Text resource with translations.
   * @returns {Array<Error>}
   */
  addMessages(source) {
    const res = FluentResource.fromString(source);
    return this.addResource(res);
  }

  /**
   * Add a translation resource to the context.
   *
   * The translation resource must be a proper FluentResource
   * parsed by `MessageContext.parseResource`.
   *
   *     let res = MessageContext.parseResource("foo = Foo");
   *     ctx.addResource(res);
   *     ctx.getMessage('foo');
   *
   *     // Returns a raw representation of the 'foo' message.
   *
   * Parsed entities should be formatted with the `format` method in case they
   * contain logic (references, select expressions etc.).
   *
   * @param   {FluentResource} res - FluentResource object.
   * @returns {Array<Error>}
   */
  addResource(res) {
    const errors = res.errors.slice();
    for (const [id, value] of res) {
      if (id.startsWith("-")) {
        // Identifiers starting with a dash (-) define terms. Terms are private
        // and cannot be retrieved from MessageContext.
        if (this._terms.has(id)) {
          errors.push(`Attempt to override an existing term: "${id}"`);
          continue;
        }
        this._terms.set(id, value);
      } else {
        if (this._messages.has(id)) {
          errors.push(`Attempt to override an existing message: "${id}"`);
          continue;
        }
        this._messages.set(id, value);
      }
    }

    return errors;
  }

  /**
   * Format a message to a string or null.
   *
   * Format a raw `message` from the context into a string (or a null if it has
   * a null value).  `args` will be used to resolve references to external
   * arguments inside of the translation.
   *
   * In case of errors `format` will try to salvage as much of the translation
   * as possible and will still return a string.  For performance reasons, the
   * encountered errors are not returned but instead are appended to the
   * `errors` array passed as the third argument.
   *
   *     const errors = [];
   *     ctx.addMessages('hello = Hello, { $name }!');
   *     const hello = ctx.getMessage('hello');
   *     ctx.format(hello, { name: 'Jane' }, errors);
   *
   *     // Returns 'Hello, Jane!' and `errors` is empty.
   *
   *     ctx.format(hello, undefined, errors);
   *
   *     // Returns 'Hello, name!' and `errors` is now:
   *
   *     [<ReferenceError: Unknown external: name>]
   *
   * @param   {Object | string}    message
   * @param   {Object | undefined} args
   * @param   {Array}              errors
   * @returns {?string}
   */
  format(message, args, errors) {
    // optimize entities which are simple strings with no attributes
    if (typeof message === "string") {
      return this._transform(message);
    }

    // optimize simple-string entities with attributes
    if (typeof message.val === "string") {
      return this._transform(message.val);
    }

    // optimize entities with null values
    if (message.val === undefined) {
      return null;
    }

    return resolve(this, args, message, errors);
  }

  _memoizeIntlObject(ctor, opts) {
    const cache = this._intls.get(ctor) || {};
    const id = JSON.stringify(opts);

    if (!cache[id]) {
      cache[id] = new ctor(this.locales, opts);
      this._intls.set(ctor, cache);
    }

    return cache[id];
  }
}

this.MessageContext = MessageContext;
this.FluentResource = FluentResource;
var EXPORTED_SYMBOLS = ["MessageContext", "FluentResource"];
