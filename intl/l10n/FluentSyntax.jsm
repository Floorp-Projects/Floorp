/* vim: set ts=2 et sw=2 tw=80 filetype=javascript: */

/* Copyright 2019 Mozilla Foundation and others
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


/* fluent-syntax@0.10.0 */

/*
 * Base class for all Fluent AST nodes.
 *
 * All productions described in the ASDL subclass BaseNode, including Span and
 * Annotation.
 *
 */
class BaseNode {
  constructor() {}
}

/*
 * Base class for AST nodes which can have Spans.
 */
class SyntaxNode extends BaseNode {
  addSpan(start, end) {
    this.span = new Span(start, end);
  }
}

class Resource extends SyntaxNode {
  constructor(body = []) {
    super();
    this.type = "Resource";
    this.body = body;
  }
}

/*
 * An abstract base class for useful elements of Resource.body.
 */
class Entry extends SyntaxNode {}

class Message extends Entry {
  constructor(id, value = null, attributes = [], comment = null) {
    super();
    this.type = "Message";
    this.id = id;
    this.value = value;
    this.attributes = attributes;
    this.comment = comment;
  }
}

class Term extends Entry {
  constructor(id, value, attributes = [], comment = null) {
    super();
    this.type = "Term";
    this.id = id;
    this.value = value;
    this.attributes = attributes;
    this.comment = comment;
  }
}

class VariantList extends SyntaxNode {
  constructor(variants) {
    super();
    this.type = "VariantList";
    this.variants = variants;
  }
}

class Pattern extends SyntaxNode {
  constructor(elements) {
    super();
    this.type = "Pattern";
    this.elements = elements;
  }
}

/*
 * An abstract base class for elements of Patterns.
 */
class PatternElement extends SyntaxNode {}

class TextElement extends PatternElement {
  constructor(value) {
    super();
    this.type = "TextElement";
    this.value = value;
  }
}

class Placeable extends PatternElement {
  constructor(expression) {
    super();
    this.type = "Placeable";
    this.expression = expression;
  }
}

/*
 * An abstract base class for expressions.
 */
class Expression extends SyntaxNode {}

class StringLiteral extends Expression {
  constructor(raw, value) {
    super();
    this.type = "StringLiteral";
    this.raw = raw;
    this.value = value;
  }
}

class NumberLiteral extends Expression {
  constructor(value) {
    super();
    this.type = "NumberLiteral";
    this.value = value;
  }
}

class MessageReference extends Expression {
  constructor(id) {
    super();
    this.type = "MessageReference";
    this.id = id;
  }
}

class TermReference extends Expression {
  constructor(id) {
    super();
    this.type = "TermReference";
    this.id = id;
  }
}

class VariableReference extends Expression {
  constructor(id) {
    super();
    this.type = "VariableReference";
    this.id = id;
  }
}

class FunctionReference extends Expression {
  constructor(id) {
    super();
    this.type = "FunctionReference";
    this.id = id;
  }
}

class SelectExpression extends Expression {
  constructor(selector, variants) {
    super();
    this.type = "SelectExpression";
    this.selector = selector;
    this.variants = variants;
  }
}

class AttributeExpression extends Expression {
  constructor(ref, name) {
    super();
    this.type = "AttributeExpression";
    this.ref = ref;
    this.name = name;
  }
}

class VariantExpression extends Expression {
  constructor(ref, key) {
    super();
    this.type = "VariantExpression";
    this.ref = ref;
    this.key = key;
  }
}

class CallExpression extends Expression {
  constructor(callee, positional = [], named = []) {
    super();
    this.type = "CallExpression";
    this.callee = callee;
    this.positional = positional;
    this.named = named;
  }
}

class Attribute extends SyntaxNode {
  constructor(id, value) {
    super();
    this.type = "Attribute";
    this.id = id;
    this.value = value;
  }
}

class Variant extends SyntaxNode {
  constructor(key, value, def = false) {
    super();
    this.type = "Variant";
    this.key = key;
    this.value = value;
    this.default = def;
  }
}

class NamedArgument extends SyntaxNode {
  constructor(name, value) {
    super();
    this.type = "NamedArgument";
    this.name = name;
    this.value = value;
  }
}

class Identifier extends SyntaxNode {
  constructor(name) {
    super();
    this.type = "Identifier";
    this.name = name;
  }
}

class BaseComment extends Entry {
  constructor(content) {
    super();
    this.type = "BaseComment";
    this.content = content;
  }
}

class Comment extends BaseComment {
  constructor(content) {
    super(content);
    this.type = "Comment";
  }
}

class GroupComment extends BaseComment {
  constructor(content) {
    super(content);
    this.type = "GroupComment";
  }
}
class ResourceComment extends BaseComment {
  constructor(content) {
    super(content);
    this.type = "ResourceComment";
  }
}

class Junk extends SyntaxNode {
  constructor(content) {
    super();
    this.type = "Junk";
    this.annotations = [];
    this.content = content;
  }

  addAnnotation(annot) {
    this.annotations.push(annot);
  }
}

class Span extends BaseNode {
  constructor(start, end) {
    super();
    this.type = "Span";
    this.start = start;
    this.end = end;
  }
}

class Annotation extends SyntaxNode {
  constructor(code, args = [], message) {
    super();
    this.type = "Annotation";
    this.code = code;
    this.args = args;
    this.message = message;
  }
}

const ast = ({
  Resource: Resource,
  Entry: Entry,
  Message: Message,
  Term: Term,
  VariantList: VariantList,
  Pattern: Pattern,
  PatternElement: PatternElement,
  TextElement: TextElement,
  Placeable: Placeable,
  Expression: Expression,
  StringLiteral: StringLiteral,
  NumberLiteral: NumberLiteral,
  MessageReference: MessageReference,
  TermReference: TermReference,
  VariableReference: VariableReference,
  FunctionReference: FunctionReference,
  SelectExpression: SelectExpression,
  AttributeExpression: AttributeExpression,
  VariantExpression: VariantExpression,
  CallExpression: CallExpression,
  Attribute: Attribute,
  Variant: Variant,
  NamedArgument: NamedArgument,
  Identifier: Identifier,
  BaseComment: BaseComment,
  Comment: Comment,
  GroupComment: GroupComment,
  ResourceComment: ResourceComment,
  Junk: Junk,
  Span: Span,
  Annotation: Annotation
});

class ParseError extends Error {
  constructor(code, ...args) {
    super();
    this.code = code;
    this.args = args;
    this.message = getErrorMessage(code, args);
  }
}

/* eslint-disable complexity */
function getErrorMessage(code, args) {
  switch (code) {
    case "E0001":
      return "Generic error";
    case "E0002":
      return "Expected an entry start";
    case "E0003": {
      const [token] = args;
      return `Expected token: "${token}"`;
    }
    case "E0004": {
      const [range] = args;
      return `Expected a character from range: "${range}"`;
    }
    case "E0005": {
      const [id] = args;
      return `Expected message "${id}" to have a value or attributes`;
    }
    case "E0006": {
      const [id] = args;
      return `Expected term "-${id}" to have a value`;
    }
    case "E0007":
      return "Keyword cannot end with a whitespace";
    case "E0008":
      return "The callee has to be an upper-case identifier or a term";
    case "E0009":
      return "The key has to be a simple identifier";
    case "E0010":
      return "Expected one of the variants to be marked as default (*)";
    case "E0011":
      return 'Expected at least one variant after "->"';
    case "E0012":
      return "Expected value";
    case "E0013":
      return "Expected variant key";
    case "E0014":
      return "Expected literal";
    case "E0015":
      return "Only one variant can be marked as default (*)";
    case "E0016":
      return "Message references cannot be used as selectors";
    case "E0017":
      return "Terms cannot be used as selectors";
    case "E0018":
      return "Attributes of messages cannot be used as selectors";
    case "E0019":
      return "Attributes of terms cannot be used as placeables";
    case "E0020":
      return "Unterminated string expression";
    case "E0021":
      return "Positional arguments must not follow named arguments";
    case "E0022":
      return "Named arguments must be unique";
    case "E0024":
      return "Cannot access variants of a message.";
    case "E0025": {
      const [char] = args;
      return `Unknown escape sequence: \\${char}.`;
    }
    case "E0026": {
      const [sequence] = args;
      return `Invalid Unicode escape sequence: ${sequence}.`;
    }
    case "E0027":
      return "Unbalanced closing brace in TextElement.";
    case "E0028":
      return "Expected an inline expression";
    default:
      return code;
  }
}

function includes(arr, elem) {
  return arr.indexOf(elem) > -1;
}

/* eslint no-magic-numbers: "off" */

class ParserStream {
  constructor(string) {
    this.string = string;
    this.index = 0;
    this.peekOffset = 0;
  }

  charAt(offset) {
    // When the cursor is at CRLF, return LF but don't move the cursor.
    // The cursor still points to the EOL position, which in this case is the
    // beginning of the compound CRLF sequence. This ensures slices of
    // [inclusive, exclusive) continue to work properly.
    if (this.string[offset] === "\r"
        && this.string[offset + 1] === "\n") {
      return "\n";
    }

    return this.string[offset];
  }

  get currentChar() {
    return this.charAt(this.index);
  }

  get currentPeek() {
    return this.charAt(this.index + this.peekOffset);
  }

  next() {
    this.peekOffset = 0;
    // Skip over the CRLF as if it was a single character.
    if (this.string[this.index] === "\r"
        && this.string[this.index + 1] === "\n") {
      this.index++;
    }
    this.index++;
    return this.string[this.index];
  }

  peek() {
    // Skip over the CRLF as if it was a single character.
    if (this.string[this.index + this.peekOffset] === "\r"
        && this.string[this.index + this.peekOffset + 1] === "\n") {
      this.peekOffset++;
    }
    this.peekOffset++;
    return this.string[this.index + this.peekOffset];
  }

  resetPeek(offset = 0) {
    this.peekOffset = offset;
  }

  skipToPeek() {
    this.index += this.peekOffset;
    this.peekOffset = 0;
  }
}

const EOL = "\n";
const EOF = undefined;
const SPECIAL_LINE_START_CHARS = ["}", ".", "[", "*"];

class FluentParserStream extends ParserStream {
  peekBlankInline() {
    const start = this.index + this.peekOffset;
    while (this.currentPeek === " ") {
      this.peek();
    }
    return this.string.slice(start, this.index + this.peekOffset);
  }

  skipBlankInline() {
    const blank = this.peekBlankInline();
    this.skipToPeek();
    return blank;
  }

  peekBlankBlock() {
    let blank = "";
    while (true) {
      const lineStart = this.peekOffset;
      this.peekBlankInline();
      if (this.currentPeek === EOL) {
        blank += EOL;
        this.peek();
        continue;
      }
      if (this.currentPeek === EOF) {
        // Treat the blank line at EOF as a blank block.
        return blank;
      }
      // Any other char; reset to column 1 on this line.
      this.resetPeek(lineStart);
      return blank;
    }
  }

  skipBlankBlock() {
    const blank = this.peekBlankBlock();
    this.skipToPeek();
    return blank;
  }

  peekBlank() {
    while (this.currentPeek === " " || this.currentPeek === EOL) {
      this.peek();
    }
  }

  skipBlank() {
    this.peekBlank();
    this.skipToPeek();
  }

  expectChar(ch) {
    if (this.currentChar === ch) {
      this.next();
      return true;
    }

    throw new ParseError("E0003", ch);
  }

  expectLineEnd() {
    if (this.currentChar === EOF) {
      // EOF is a valid line end in Fluent.
      return true;
    }

    if (this.currentChar === EOL) {
      this.next();
      return true;
    }

    // Unicode Character 'SYMBOL FOR NEWLINE' (U+2424)
    throw new ParseError("E0003", "\u2424");
  }

  takeChar(f) {
    const ch = this.currentChar;
    if (ch === EOF) {
      return EOF;
    }
    if (f(ch)) {
      this.next();
      return ch;
    }
    return null;
  }

  isCharIdStart(ch) {
    if (ch === EOF) {
      return false;
    }

    const cc = ch.charCodeAt(0);
    return (cc >= 97 && cc <= 122) || // a-z
           (cc >= 65 && cc <= 90); // A-Z
  }

  isIdentifierStart() {
    return this.isCharIdStart(this.currentPeek);
  }

  isNumberStart() {
    const ch = this.currentChar === "-"
      ? this.peek()
      : this.currentChar;

    if (ch === EOF) {
      this.resetPeek();
      return false;
    }

    const cc = ch.charCodeAt(0);
    const isDigit = cc >= 48 && cc <= 57; // 0-9
    this.resetPeek();
    return isDigit;
  }

  isCharPatternContinuation(ch) {
    if (ch === EOF) {
      return false;
    }

    return !includes(SPECIAL_LINE_START_CHARS, ch);
  }

  isValueStart() {
    // Inline Patterns may start with any char.
    const ch = this.currentPeek;
    return ch !== EOL && ch !== EOF;
  }

  isValueContinuation() {
    const column1 = this.peekOffset;
    this.peekBlankInline();

    if (this.currentPeek === "{") {
      this.resetPeek(column1);
      return true;
    }

    if (this.peekOffset - column1 === 0) {
      return false;
    }

    if (this.isCharPatternContinuation(this.currentPeek)) {
      this.resetPeek(column1);
      return true;
    }

    return false;
  }

  // -1 - any
  //  0 - comment
  //  1 - group comment
  //  2 - resource comment
  isNextLineComment(level = -1) {
    if (this.currentChar !== EOL) {
      return false;
    }

    let i = 0;

    while (i <= level || (level === -1 && i < 3)) {
      if (this.peek() !== "#") {
        if (i <= level && level !== -1) {
          this.resetPeek();
          return false;
        }
        break;
      }
      i++;
    }

    // The first char after #, ## or ###.
    const ch = this.peek();
    if (ch === " " || ch === EOL) {
      this.resetPeek();
      return true;
    }

    this.resetPeek();
    return false;
  }

  isVariantStart() {
    const currentPeekOffset = this.peekOffset;
    if (this.currentPeek === "*") {
      this.peek();
    }
    if (this.currentPeek === "[") {
      this.resetPeek(currentPeekOffset);
      return true;
    }
    this.resetPeek(currentPeekOffset);
    return false;
  }

  isAttributeStart() {
    return this.currentPeek === ".";
  }

  skipToNextEntryStart(junkStart) {
    let lastNewline = this.string.lastIndexOf(EOL, this.index);
    if (junkStart < lastNewline) {
      // Last seen newline is _after_ the junk start. It's safe to rewind
      // without the risk of resuming at the same broken entry.
      this.index = lastNewline;
    }
    while (this.currentChar) {
      // We're only interested in beginnings of line.
      if (this.currentChar !== EOL) {
        this.next();
        continue;
      }

      // Break if the first char in this line looks like an entry start.
      const first = this.next();
      if (this.isCharIdStart(first) || first === "-" || first === "#") {
        break;
      }
    }
  }

  takeIDStart() {
    if (this.isCharIdStart(this.currentChar)) {
      const ret = this.currentChar;
      this.next();
      return ret;
    }

    throw new ParseError("E0004", "a-zA-Z");
  }

  takeIDChar() {
    const closure = ch => {
      const cc = ch.charCodeAt(0);
      return ((cc >= 97 && cc <= 122) || // a-z
              (cc >= 65 && cc <= 90) || // A-Z
              (cc >= 48 && cc <= 57) || // 0-9
               cc === 95 || cc === 45); // _-
    };

    return this.takeChar(closure);
  }

  takeDigit() {
    const closure = ch => {
      const cc = ch.charCodeAt(0);
      return (cc >= 48 && cc <= 57); // 0-9
    };

    return this.takeChar(closure);
  }

  takeHexDigit() {
    const closure = ch => {
      const cc = ch.charCodeAt(0);
      return (cc >= 48 && cc <= 57) // 0-9
        || (cc >= 65 && cc <= 70) // A-F
        || (cc >= 97 && cc <= 102); // a-f
    };

    return this.takeChar(closure);
  }
}

/*  eslint no-magic-numbers: [0]  */


const trailingWSRe = /[ \t\n\r]+$/;


function withSpan(fn) {
  return function(ps, ...args) {
    if (!this.withSpans) {
      return fn.call(this, ps, ...args);
    }

    const start = ps.index;
    const node = fn.call(this, ps, ...args);

    // Don't re-add the span if the node already has it. This may happen when
    // one decorated function calls another decorated function.
    if (node.span) {
      return node;
    }

    const end = ps.index;
    node.addSpan(start, end);
    return node;
  };
}


class FluentParser {
  constructor({
    withSpans = true,
  } = {}) {
    this.withSpans = withSpans;

    // Poor man's decorators.
    const methodNames = [
      "getComment", "getMessage", "getTerm", "getAttribute", "getIdentifier",
      "getVariant", "getNumber", "getPattern", "getVariantList",
      "getTextElement", "getPlaceable", "getExpression",
      "getInlineExpression", "getCallArgument", "getString",
      "getSimpleExpression", "getLiteral",
    ];
    for (const name of methodNames) {
      this[name] = withSpan(this[name]);
    }
  }

  parse(source) {
    const ps = new FluentParserStream(source);
    ps.skipBlankBlock();

    const entries = [];
    let lastComment = null;

    while (ps.currentChar) {
      const entry = this.getEntryOrJunk(ps);
      const blankLines = ps.skipBlankBlock();

      // Regular Comments require special logic. Comments may be attached to
      // Messages or Terms if they are followed immediately by them. However
      // they should parse as standalone when they're followed by Junk.
      // Consequently, we only attach Comments once we know that the Message
      // or the Term parsed successfully.
      if (entry.type === "Comment"
          && blankLines.length === 0
          && ps.currentChar) {
        // Stash the comment and decide what to do with it in the next pass.
        lastComment = entry;
        continue;
      }

      if (lastComment) {
        if (entry.type === "Message" || entry.type === "Term") {
          entry.comment = lastComment;
          if (this.withSpans) {
            entry.span.start = entry.comment.span.start;
          }
        } else {
          entries.push(lastComment);
        }
        // In either case, the stashed comment has been dealt with; clear it.
        lastComment = null;
      }

      // No special logic for other types of entries.
      entries.push(entry);
    }

    const res = new Resource(entries);

    if (this.withSpans) {
      res.addSpan(0, ps.index);
    }

    return res;
  }

  /*
   * Parse the first Message or Term in `source`.
   *
   * Skip all encountered comments and start parsing at the first Message or
   * Term start. Return Junk if the parsing is not successful.
   *
   * Preceding comments are ignored unless they contain syntax errors
   * themselves, in which case Junk for the invalid comment is returned.
   */
  parseEntry(source) {
    const ps = new FluentParserStream(source);
    ps.skipBlankBlock();

    while (ps.currentChar === "#") {
      const skipped = this.getEntryOrJunk(ps);
      if (skipped.type === "Junk") {
        // Don't skip Junk comments.
        return skipped;
      }
      ps.skipBlankBlock();
    }

    return this.getEntryOrJunk(ps);
  }

  getEntryOrJunk(ps) {
    const entryStartPos = ps.index;

    try {
      const entry = this.getEntry(ps);
      ps.expectLineEnd();
      return entry;
    } catch (err) {
      if (!(err instanceof ParseError)) {
        throw err;
      }

      let errorIndex = ps.index;
      ps.skipToNextEntryStart(entryStartPos);
      const nextEntryStart = ps.index;
      if (nextEntryStart < errorIndex) {
        // The position of the error must be inside of the Junk's span.
        errorIndex = nextEntryStart;
      }

      // Create a Junk instance
      const slice = ps.string.substring(entryStartPos, nextEntryStart);
      const junk = new Junk(slice);
      if (this.withSpans) {
        junk.addSpan(entryStartPos, nextEntryStart);
      }
      const annot = new Annotation(err.code, err.args, err.message);
      annot.addSpan(errorIndex, errorIndex);
      junk.addAnnotation(annot);
      return junk;
    }
  }

  getEntry(ps) {
    if (ps.currentChar === "#") {
      return this.getComment(ps);
    }

    if (ps.currentChar === "-") {
      return this.getTerm(ps);
    }

    if (ps.isIdentifierStart()) {
      return this.getMessage(ps);
    }

    throw new ParseError("E0002");
  }

  getComment(ps) {
    // 0 - comment
    // 1 - group comment
    // 2 - resource comment
    let level = -1;
    let content = "";

    while (true) {
      let i = -1;
      while (ps.currentChar === "#" && (i < (level === -1 ? 2 : level))) {
        ps.next();
        i++;
      }

      if (level === -1) {
        level = i;
      }

      if (ps.currentChar !== EOL) {
        ps.expectChar(" ");
        let ch;
        while ((ch = ps.takeChar(x => x !== EOL))) {
          content += ch;
        }
      }

      if (ps.isNextLineComment(level)) {
        content += ps.currentChar;
        ps.next();
      } else {
        break;
      }
    }

    let Comment$$1;
    switch (level) {
      case 0:
        Comment$$1 = Comment;
        break;
      case 1:
        Comment$$1 = GroupComment;
        break;
      case 2:
        Comment$$1 = ResourceComment;
        break;
    }
    return new Comment$$1(content);
  }

  getMessage(ps) {
    const id = this.getIdentifier(ps);

    ps.skipBlankInline();
    ps.expectChar("=");

    const value = this.maybeGetPattern(ps);
    const attrs = this.getAttributes(ps);

    if (value === null && attrs.length === 0) {
      throw new ParseError("E0005", id.name);
    }

    return new Message(id, value, attrs);
  }

  getTerm(ps) {
    ps.expectChar("-");
    const id = this.getIdentifier(ps);

    ps.skipBlankInline();
    ps.expectChar("=");

    // Syntax 0.8 compat: VariantLists are supported but deprecated. They can
    // only be found as values of Terms. Nested VariantLists are not allowed.
    const value = this.maybeGetVariantList(ps) || this.maybeGetPattern(ps);
    if (value === null) {
      throw new ParseError("E0006", id.name);
    }

    const attrs = this.getAttributes(ps);
    return new Term(id, value, attrs);
  }

  getAttribute(ps) {
    ps.expectChar(".");

    const key = this.getIdentifier(ps);

    ps.skipBlankInline();
    ps.expectChar("=");

    const value = this.maybeGetPattern(ps);
    if (value === null) {
      throw new ParseError("E0012");
    }

    return new Attribute(key, value);
  }

  getAttributes(ps) {
    const attrs = [];
    ps.peekBlank();
    while (ps.isAttributeStart()) {
      ps.skipToPeek();
      const attr = this.getAttribute(ps);
      attrs.push(attr);
      ps.peekBlank();
    }
    return attrs;
  }

  getIdentifier(ps) {
    let name = ps.takeIDStart();

    let ch;
    while ((ch = ps.takeIDChar())) {
      name += ch;
    }

    return new Identifier(name);
  }

  getVariantKey(ps) {
    const ch = ps.currentChar;

    if (ch === EOF) {
      throw new ParseError("E0013");
    }

    const cc = ch.charCodeAt(0);

    if ((cc >= 48 && cc <= 57) || cc === 45) { // 0-9, -
      return this.getNumber(ps);
    }

    return this.getIdentifier(ps);
  }

  getVariant(ps, {hasDefault}) {
    let defaultIndex = false;

    if (ps.currentChar === "*") {
      if (hasDefault) {
        throw new ParseError("E0015");
      }
      ps.next();
      defaultIndex = true;
    }

    ps.expectChar("[");

    ps.skipBlank();

    const key = this.getVariantKey(ps);

    ps.skipBlank();
    ps.expectChar("]");

    const value = this.maybeGetPattern(ps);
    if (value === null) {
      throw new ParseError("E0012");
    }

    return new Variant(key, value, defaultIndex);
  }

  getVariants(ps) {
    const variants = [];
    let hasDefault = false;

    ps.skipBlank();
    while (ps.isVariantStart()) {
      const variant = this.getVariant(ps, {hasDefault});

      if (variant.default) {
        hasDefault = true;
      }

      variants.push(variant);
      ps.expectLineEnd();
      ps.skipBlank();
    }

    if (variants.length === 0) {
      throw new ParseError("E0011");
    }

    if (!hasDefault) {
      throw new ParseError("E0010");
    }

    return variants;
  }

  getDigits(ps) {
    let num = "";

    let ch;
    while ((ch = ps.takeDigit())) {
      num += ch;
    }

    if (num.length === 0) {
      throw new ParseError("E0004", "0-9");
    }

    return num;
  }

  getNumber(ps) {
    let num = "";

    if (ps.currentChar === "-") {
      num += "-";
      ps.next();
    }

    num = `${num}${this.getDigits(ps)}`;

    if (ps.currentChar === ".") {
      num += ".";
      ps.next();
      num = `${num}${this.getDigits(ps)}`;
    }

    return new NumberLiteral(num);
  }

  // maybeGetPattern distinguishes between patterns which start on the same line
  // as the identifier (a.k.a. inline signleline patterns and inline multiline
  // patterns) and patterns which start on a new line (a.k.a. block multiline
  // patterns). The distinction is important for the dedentation logic: the
  // indent of the first line of a block pattern must be taken into account when
  // calculating the maximum common indent.
  maybeGetPattern(ps) {
    ps.peekBlankInline();
    if (ps.isValueStart()) {
      ps.skipToPeek();
      return this.getPattern(ps, {isBlock: false});
    }

    ps.peekBlankBlock();
    if (ps.isValueContinuation()) {
      ps.skipToPeek();
      return this.getPattern(ps, {isBlock: true});
    }

    return null;
  }

  // Deprecated in Syntax 0.8. VariantLists are only allowed as values of Terms.
  // Values of Messages, Attributes and Variants must be Patterns. This method
  // is only used in getTerm.
  maybeGetVariantList(ps) {
    ps.peekBlank();
    if (ps.currentPeek === "{") {
      const start = ps.peekOffset;
      ps.peek();
      ps.peekBlankInline();
      if (ps.currentPeek === EOL) {
        ps.peekBlank();
        if (ps.isVariantStart()) {
          ps.resetPeek(start);
          ps.skipToPeek();
          return this.getVariantList(ps);
        }
      }
    }

    ps.resetPeek();
    return null;
  }

  getVariantList(ps) {
    ps.expectChar("{");
    var variants = this.getVariants(ps);
    ps.expectChar("}");
    return new VariantList(variants);
  }

  getPattern(ps, {isBlock}) {
    const elements = [];
    if (isBlock) {
      // A block pattern is a pattern which starts on a new line. Store and
      // measure the indent of this first line for the dedentation logic.
      const blankStart = ps.index;
      const firstIndent = ps.skipBlankInline();
      elements.push(this.getIndent(ps, firstIndent, blankStart));
      var commonIndentLength = firstIndent.length;
    } else {
      var commonIndentLength = Infinity;
    }

    let ch;
    elements: while ((ch = ps.currentChar)) {
      switch (ch) {
        case EOL: {
          const blankStart = ps.index;
          const blankLines = ps.peekBlankBlock();
          if (ps.isValueContinuation()) {
            ps.skipToPeek();
            const indent = ps.skipBlankInline();
            commonIndentLength = Math.min(commonIndentLength, indent.length);
            elements.push(this.getIndent(ps, blankLines + indent, blankStart));
            continue elements;
          }

          // The end condition for getPattern's while loop is a newline
          // which is not followed by a valid pattern continuation.
          ps.resetPeek();
          break elements;
        }
        case "{":
          elements.push(this.getPlaceable(ps));
          continue elements;
        case "}":
          throw new ParseError("E0027");
        default:
          const element = this.getTextElement(ps);
          elements.push(element);
      }
    }

    const dedented = this.dedent(elements, commonIndentLength);
    return new Pattern(dedented);
  }

  // Create a token representing an indent. It's not part of the AST and it will
  // be trimmed and merged into adjacent TextElements, or turned into a new
  // TextElement, if it's surrounded by two Placeables.
  getIndent(ps, value, start) {
    return {
      type: "Indent",
      span: {start, end: ps.index},
      value,
    };
  }

  // Dedent a list of elements by removing the maximum common indent from the
  // beginning of text lines. The common indent is calculated in getPattern.
  dedent(elements, commonIndent) {
    const trimmed = [];

    for (let element of elements) {
      if (element.type === "Placeable") {
        trimmed.push(element);
        continue;
      }

      if (element.type === "Indent") {
        // Strip common indent.
        element.value = element.value.slice(
          0, element.value.length - commonIndent);
        if (element.value.length === 0) {
          continue;
        }
      }

      let prev = trimmed[trimmed.length - 1];
      if (prev && prev.type === "TextElement") {
        // Join adjacent TextElements by replacing them with their sum.
        const sum = new TextElement(prev.value + element.value);
        if (this.withSpans) {
          sum.addSpan(prev.span.start, element.span.end);
        }
        trimmed[trimmed.length - 1] = sum;
        continue;
      }

      if (element.type === "Indent") {
        // If the indent hasn't been merged into a preceding TextElement,
        // convert it into a new TextElement.
        const textElement = new TextElement(element.value);
        if (this.withSpans) {
          textElement.addSpan(element.span.start, element.span.end);
        }
        element = textElement;
      }

      trimmed.push(element);
    }

    // Trim trailing whitespace from the Pattern.
    const lastElement = trimmed[trimmed.length - 1];
    if (lastElement.type === "TextElement") {
      lastElement.value = lastElement.value.replace(trailingWSRe, "");
      if (lastElement.value.length === 0) {
        trimmed.pop();
      }
    }

    return trimmed;
  }

  getTextElement(ps) {
    let buffer = "";

    let ch;
    while ((ch = ps.currentChar)) {
      if (ch === "{" || ch === "}") {
        return new TextElement(buffer);
      }

      if (ch === EOL) {
        return new TextElement(buffer);
      }

      buffer += ch;
      ps.next();
    }

    return new TextElement(buffer);
  }

  getEscapeSequence(ps) {
    const next = ps.currentChar;

    switch (next) {
      case "\\":
      case "\"":
        ps.next();
        return [`\\${next}`, next];
      case "u":
        return this.getUnicodeEscapeSequence(ps, next, 4);
      case "U":
        return this.getUnicodeEscapeSequence(ps, next, 6);
      default:
        throw new ParseError("E0025", next);
    }
  }

  getUnicodeEscapeSequence(ps, u, digits) {
    ps.expectChar(u);

    let sequence = "";
    for (let i = 0; i < digits; i++) {
      const ch = ps.takeHexDigit();

      if (!ch) {
        throw new ParseError(
          "E0026", `\\${u}${sequence}${ps.currentChar}`);
      }

      sequence += ch;
    }

    const codepoint = parseInt(sequence, 16);
    const unescaped = codepoint <= 0xD7FF || 0xE000 <= codepoint
      // It's a Unicode scalar value.
      ? String.fromCodePoint(codepoint)
      // Escape sequences reresenting surrogate code points are well-formed
      // but invalid in Fluent. Replace them with U+FFFD REPLACEMENT
      // CHARACTER.
      : "�";
    return [`\\${u}${sequence}`, unescaped];
  }

  getPlaceable(ps) {
    ps.expectChar("{");
    ps.skipBlank();
    const expression = this.getExpression(ps);
    ps.expectChar("}");
    return new Placeable(expression);
  }

  getExpression(ps) {
    const selector = this.getInlineExpression(ps);
    ps.skipBlank();

    if (ps.currentChar === "-") {
      if (ps.peek() !== ">") {
        ps.resetPeek();
        return selector;
      }

      if (selector.type === "MessageReference") {
        throw new ParseError("E0016");
      }

      if (selector.type === "AttributeExpression"
          && selector.ref.type === "MessageReference") {
        throw new ParseError("E0018");
      }

      if (selector.type === "TermReference"
          || selector.type === "VariantExpression") {
        throw new ParseError("E0017");
      }

      if (selector.type === "CallExpression"
          && selector.callee.type === "TermReference") {
        throw new ParseError("E0017");
      }

      ps.next();
      ps.next();

      ps.skipBlankInline();
      ps.expectLineEnd();

      const variants = this.getVariants(ps);
      return new SelectExpression(selector, variants);
    }

    if (selector.type === "AttributeExpression"
        && selector.ref.type === "TermReference") {
      throw new ParseError("E0019");
    }

    if (selector.type === "CallExpression"
        && selector.callee.type === "AttributeExpression") {
      throw new ParseError("E0019");
    }

    return selector;
  }

  getInlineExpression(ps) {
    if (ps.currentChar === "{") {
      return this.getPlaceable(ps);
    }

    let expr = this.getSimpleExpression(ps);
    switch (expr.type) {
      case "NumberLiteral":
      case "StringLiteral":
      case "VariableReference":
        return expr;
      case "MessageReference": {
        if (ps.currentChar === ".") {
          ps.next();
          const attr = this.getIdentifier(ps);
          return new AttributeExpression(expr, attr);
        }

        if (ps.currentChar === "(") {
          // It's a Function. Ensure it's all upper-case.
          if (!/^[A-Z][A-Z_?-]*$/.test(expr.id.name)) {
            throw new ParseError("E0008");
          }

          const func = new FunctionReference(expr.id);
          if (this.withSpans) {
            func.addSpan(expr.span.start, expr.span.end);
          }
          return new CallExpression(func, ...this.getCallArguments(ps));
        }

        return expr;
      }
      case "TermReference": {
        if (ps.currentChar === "[") {
          ps.next();
          const key = this.getVariantKey(ps);
          ps.expectChar("]");
          return new VariantExpression(expr, key);
        }

        if (ps.currentChar === ".") {
          ps.next();
          const attr = this.getIdentifier(ps);
          expr = new AttributeExpression(expr, attr);
        }

        if (ps.currentChar === "(") {
          return new CallExpression(expr, ...this.getCallArguments(ps));
        }

        return expr;
      }
      default:
        throw new ParseError("E0028");
    }
  }

  getSimpleExpression(ps) {
    if (ps.isNumberStart()) {
      return this.getNumber(ps);
    }

    if (ps.currentChar === '"') {
      return this.getString(ps);
    }

    if (ps.currentChar === "$") {
      ps.next();
      const id = this.getIdentifier(ps);
      return new VariableReference(id);
    }

    if (ps.currentChar === "-") {
      ps.next();
      const id = this.getIdentifier(ps);
      return new TermReference(id);
    }

    if (ps.isIdentifierStart()) {
      const id = this.getIdentifier(ps);
      return new MessageReference(id);
    }

    throw new ParseError("E0028");
  }

  getCallArgument(ps) {
    const exp = this.getInlineExpression(ps);

    ps.skipBlank();

    if (ps.currentChar !== ":") {
      return exp;
    }

    if (exp.type !== "MessageReference") {
      throw new ParseError("E0009");
    }

    ps.next();
    ps.skipBlank();

    const value = this.getLiteral(ps);
    return new NamedArgument(exp.id, value);
  }

  getCallArguments(ps) {
    const positional = [];
    const named = [];
    const argumentNames = new Set();

    ps.expectChar("(");
    ps.skipBlank();

    while (true) {
      if (ps.currentChar === ")") {
        break;
      }

      const arg = this.getCallArgument(ps);
      if (arg.type === "NamedArgument") {
        if (argumentNames.has(arg.name.name)) {
          throw new ParseError("E0022");
        }
        named.push(arg);
        argumentNames.add(arg.name.name);
      } else if (argumentNames.size > 0) {
        throw new ParseError("E0021");
      } else {
        positional.push(arg);
      }

      ps.skipBlank();

      if (ps.currentChar === ",") {
        ps.next();
        ps.skipBlank();
        continue;
      }

      break;
    }

    ps.expectChar(")");
    return [positional, named];
  }

  getString(ps) {
    let raw = "";
    let value = "";

    ps.expectChar("\"");

    let ch;
    while ((ch = ps.takeChar(x => x !== '"' && x !== EOL))) {
      if (ch === "\\") {
        const [sequence, unescaped] = this.getEscapeSequence(ps);
        raw += sequence;
        value += unescaped;
      } else {
        raw += ch;
        value += ch;
      }
    }

    if (ps.currentChar === EOL) {
      throw new ParseError("E0020");
    }

    ps.expectChar("\"");

    return new StringLiteral(raw, value);
  }

  getLiteral(ps) {
    if (ps.isNumberStart()) {
      return this.getNumber(ps);
    }

    if (ps.currentChar === '"') {
      return this.getString(ps);
    }

    throw new ParseError("E0014");
  }
}

function indent(content) {
  return content.split("\n").join("\n    ");
}

function includesNewLine(elem) {
  return elem.type === "TextElement" && includes(elem.value, "\n");
}

function isSelectExpr(elem) {
  return elem.type === "Placeable"
    && elem.expression.type === "SelectExpression";
}

// Bit masks representing the state of the serializer.
const HAS_ENTRIES = 1;

class FluentSerializer {
  constructor({ withJunk = false } = {}) {
    this.withJunk = withJunk;
  }

  serialize(resource) {
    if (resource.type !== "Resource") {
      throw new Error(`Unknown resource type: ${resource.type}`);
    }

    let state = 0;
    const parts = [];

    for (const entry of resource.body) {
      if (entry.type !== "Junk" || this.withJunk) {
        parts.push(this.serializeEntry(entry, state));
        if (!(state & HAS_ENTRIES)) {
          state |= HAS_ENTRIES;
        }
      }
    }

    return parts.join("");
  }

  serializeEntry(entry, state = 0) {
    switch (entry.type) {
      case "Message":
        return serializeMessage(entry);
      case "Term":
        return serializeTerm(entry);
      case "Comment":
        if (state & HAS_ENTRIES) {
          return `\n${serializeComment(entry, "#")}\n`;
        }
        return `${serializeComment(entry, "#")}\n`;
      case "GroupComment":
        if (state & HAS_ENTRIES) {
          return `\n${serializeComment(entry, "##")}\n`;
        }
        return `${serializeComment(entry, "##")}\n`;
      case "ResourceComment":
        if (state & HAS_ENTRIES) {
          return `\n${serializeComment(entry, "###")}\n`;
        }
        return `${serializeComment(entry, "###")}\n`;
      case "Junk":
        return serializeJunk(entry);
      default :
        throw new Error(`Unknown entry type: ${entry.type}`);
    }
  }

  serializeExpression(expr) {
    return serializeExpression(expr);
  }
}


function serializeComment(comment, prefix = "#") {
  const prefixed = comment.content.split("\n").map(
    line => line.length ? `${prefix} ${line}` : prefix
  ).join("\n");
  // Add the trailing newline.
  return `${prefixed}\n`;
}


function serializeJunk(junk) {
  return junk.content;
}


function serializeMessage(message) {
  const parts = [];

  if (message.comment) {
    parts.push(serializeComment(message.comment));
  }

  parts.push(`${message.id.name} =`);

  if (message.value) {
    parts.push(serializeValue(message.value));
  }

  for (const attribute of message.attributes) {
    parts.push(serializeAttribute(attribute));
  }

  parts.push("\n");
  return parts.join("");
}


function serializeTerm(term) {
  const parts = [];

  if (term.comment) {
    parts.push(serializeComment(term.comment));
  }

  parts.push(`-${term.id.name} =`);
  parts.push(serializeValue(term.value));

  for (const attribute of term.attributes) {
    parts.push(serializeAttribute(attribute));
  }

  parts.push("\n");
  return parts.join("");
}


function serializeAttribute(attribute) {
  const value = indent(serializeValue(attribute.value));
  return `\n    .${attribute.id.name} =${value}`;
}


function serializeValue(value) {
  switch (value.type) {
    case "Pattern":
      return serializePattern(value);
    case "VariantList":
      return serializeVariantList(value);
    default:
      throw new Error(`Unknown value type: ${value.type}`);
  }
}


function serializePattern(pattern) {
  const content = pattern.elements.map(serializeElement).join("");
  const startOnNewLine =
    pattern.elements.some(isSelectExpr) ||
    pattern.elements.some(includesNewLine);

  if (startOnNewLine) {
    return `\n    ${indent(content)}`;
  }

  return ` ${content}`;
}


function serializeVariantList(varlist) {
  const content = varlist.variants.map(serializeVariant).join("");
  return `\n    {${indent(content)}\n    }`;
}


function serializeVariant(variant) {
  const key = serializeVariantKey(variant.key);
  const value = indent(serializeValue(variant.value));

  if (variant.default) {
    return `\n   *[${key}]${value}`;
  }

  return `\n    [${key}]${value}`;
}


function serializeElement(element) {
  switch (element.type) {
    case "TextElement":
      return element.value;
    case "Placeable":
      return serializePlaceable(element);
    default:
      throw new Error(`Unknown element type: ${element.type}`);
  }
}


function serializePlaceable(placeable) {
  const expr = placeable.expression;

  switch (expr.type) {
    case "Placeable":
      return `{${serializePlaceable(expr)}}`;
    case "SelectExpression":
      // Special-case select expression to control the whitespace around the
      // opening and the closing brace.
      return `{ ${serializeSelectExpression(expr)}}`;
    default:
      return `{ ${serializeExpression(expr)} }`;
  }
}


function serializeExpression(expr) {
  switch (expr.type) {
    case "StringLiteral":
      return `"${expr.raw}"`;
    case "NumberLiteral":
      return expr.value;
    case "MessageReference":
    case "FunctionReference":
      return expr.id.name;
    case "TermReference":
      return `-${expr.id.name}`;
    case "VariableReference":
      return `$${expr.id.name}`;
    case "AttributeExpression":
      return serializeAttributeExpression(expr);
    case "VariantExpression":
      return serializeVariantExpression(expr);
    case "CallExpression":
      return serializeCallExpression(expr);
    case "SelectExpression":
      return serializeSelectExpression(expr);
    case "Placeable":
      return serializePlaceable(expr);
    default:
      throw new Error(`Unknown expression type: ${expr.type}`);
  }
}


function serializeSelectExpression(expr) {
  const parts = [];
  const selector = `${serializeExpression(expr.selector)} ->`;
  parts.push(selector);

  for (const variant of expr.variants) {
    parts.push(serializeVariant(variant));
  }

  parts.push("\n");
  return parts.join("");
}


function serializeAttributeExpression(expr) {
  const ref = serializeExpression(expr.ref);
  return `${ref}.${expr.name.name}`;
}


function serializeVariantExpression(expr) {
  const ref = serializeExpression(expr.ref);
  const key = serializeVariantKey(expr.key);
  return `${ref}[${key}]`;
}


function serializeCallExpression(expr) {
  const callee = serializeExpression(expr.callee);
  const positional = expr.positional.map(serializeExpression).join(", ");
  const named = expr.named.map(serializeNamedArgument).join(", ");
  if (expr.positional.length > 0 && expr.named.length > 0) {
    return `${callee}(${positional}, ${named})`;
  }
  return `${callee}(${positional || named})`;
}


function serializeNamedArgument(arg) {
  const value = serializeExpression(arg.value);
  return `${arg.name.name}: ${value}`;
}


function serializeVariantKey(key) {
  switch (key.type) {
    case "Identifier":
      return key.name;
    default:
      return serializeExpression(key);
  }
}

/* eslint object-shorthand: "off",
          no-unused-vars: "off",
          no-redeclare: "off",
          comma-dangle: "off",
          no-labels: "off" */

this.EXPORTED_SYMBOLS = [
  "FluentParser",
  "FluentSerializer",
  ...Object.keys(ast),
];
