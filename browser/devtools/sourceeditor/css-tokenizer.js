/**
 * This file is taken from the below mentioned url and is under CC0 license.
 * https://github.com/tabatkins/css-parser/blob/master/tokenizer.js
 * Please retain this comment while updating this file from upstream.
 */

(function (root, factory) {
    // Universal Module Definition (UMD) to support AMD, CommonJS/Node.js,
    // Rhino, and plain browser loading.
    if (typeof define === 'function' && define.amd) {
        define(['exports'], factory);
    } else if (typeof exports !== 'undefined') {
        factory(exports);
    } else {
        factory(root);
    }
}(this, function (exports) {

var between = function (num, first, last) { return num >= first && num <= last; }
function digit(code) { return between(code, 0x30,0x39); }
function hexdigit(code) { return digit(code) || between(code, 0x41,0x46) || between(code, 0x61,0x66); }
function uppercaseletter(code) { return between(code, 0x41,0x5a); }
function lowercaseletter(code) { return between(code, 0x61,0x7a); }
function letter(code) { return uppercaseletter(code) || lowercaseletter(code); }
function nonascii(code) { return code >= 0xa0; }
function namestartchar(code) { return letter(code) || nonascii(code) || code == 0x5f; }
function namechar(code) { return namestartchar(code) || digit(code) || code == 0x2d; }
function nonprintable(code) { return between(code, 0,8) || between(code, 0xe,0x1f) || between(code, 0x7f,0x9f); }
function newline(code) { return code == 0xa || code == 0xc; }
function whitespace(code) { return newline(code) || code == 9 || code == 0x20; }
function badescape(code) { return newline(code) || isNaN(code); }

// Note: I'm not yet acting smart enough to actually handle astral characters.
var maximumallowedcodepoint = 0x10ffff;

function tokenize(str, options) {
  if(options == undefined) options = {transformFunctionWhitespace:false, scientificNotation:false};
  var i = -1;
  var tokens = [];
  var state = "data";
  var code;
  var currtoken;

  // Line number information.
  var line = 0;
  var column = 0;
  // The only use of lastLineLength is in reconsume().
  var lastLineLength = 0;
  var incrLineno = function() {
    line += 1;
    lastLineLength = column;
    column = 0;
  };
  var locStart = {line:line, column:column};

  var next = function(num) { if(num === undefined) num = 1; return str.charCodeAt(i+num); };
  var consume = function(num) {
    if(num === undefined)
      num = 1;
    i += num;
    code = str.charCodeAt(i);
    if (newline(code)) incrLineno();
    else column += num;
    //console.log('Consume '+i+' '+String.fromCharCode(code) + ' 0x' + code.toString(16));
    return true;
  };
  var reconsume = function() {
    i -= 1;
    if (newline(code)) {
      line -= 1;
      column = lastLineLength;
    } else {
      column -= 1;
    }
    locStart.line = line;
    locStart.column = column;
    return true;
  };
  var eof = function() { return i >= str.length; };
  var donothing = function() {};
  var emit = function(token) {
    if(token) {
      token.finish();
    } else {
      token = currtoken.finish();
    }
    if (options.loc === true) {
      token.loc = {};
      token.loc.start = {line:locStart.line, column:locStart.column};
      locStart = {line: line, column: column};
      token.loc.end = locStart;
    }
    tokens.push(token);
    //console.log('Emitting ' + token);
    currtoken = undefined;
    return true;
  };
  var create = function(token) { currtoken = token; return true; };
  var parseerror = function() { console.log("Parse error at index " + i + ", processing codepoint 0x" + code.toString(16) + " in state " + state + ".");return true; };
  var switchto = function(newstate) {
    state = newstate;
    //console.log('Switching to ' + state);
    return true;
  };
  var consumeEscape = function() {
    // Assume the the current character is the \
    consume();
    if(hexdigit(code)) {
      // Consume 1-6 hex digits
      var digits = [];
      for(var total = 0; total < 6; total++) {
        if(hexdigit(code)) {
          digits.push(code);
          consume();
        } else { break; }
      }
      var value = parseInt(digits.map(String.fromCharCode).join(''), 16);
      if( value > maximumallowedcodepoint ) value = 0xfffd;
      // If the current char is whitespace, cool, we'll just eat it.
      // Otherwise, put it back.
      if(!whitespace(code)) reconsume();
      return value;
    } else {
      return code;
    }
  };

  for(;;) {
    if(i > str.length*2) return "I'm infinite-looping!";
    consume();
    switch(state) {
    case "data":
      if(whitespace(code)) {
        emit(new WhitespaceToken);
        while(whitespace(next())) consume();
      }
      else if(code == 0x22) switchto("double-quote-string");
      else if(code == 0x23) switchto("hash");
      else if(code == 0x27) switchto("single-quote-string");
      else if(code == 0x28) emit(new OpenParenToken);
      else if(code == 0x29) emit(new CloseParenToken);
      else if(code == 0x2b) {
        if(digit(next()) || (next() == 0x2e && digit(next(2)))) switchto("number") && reconsume();
        else emit(new DelimToken(code));
      }
      else if(code == 0x2d) {
        if(next(1) == 0x2d && next(2) == 0x3e) consume(2) && emit(new CDCToken);
        else if(digit(next()) || (next(1) == 0x2e && digit(next(2)))) switchto("number") && reconsume();
        else if(namestartchar(next())) switchto("identifier") && reconsume();
        else emit(new DelimToken(code));
      }
      else if(code == 0x2e) {
        if(digit(next())) switchto("number") && reconsume();
        else emit(new DelimToken(code));
      }
      else if(code == 0x2f) {
        if(next() == 0x2a) switchto("comment");
        else emit(new DelimToken(code));
      }
      else if(code == 0x3a) emit(new ColonToken);
      else if(code == 0x3b) emit(new SemicolonToken);
      else if(code == 0x3c) {
        if(next(1) == 0x21 && next(2) == 0x2d && next(3) == 0x2d) consume(3) && emit(new CDOToken);
        else emit(new DelimToken(code));
      }
      else if(code == 0x40) switchto("at-keyword");
      else if(code == 0x5b) emit(new OpenSquareToken);
      else if(code == 0x5c) {
        if(badescape(next())) parseerror() && emit(new DelimToken(code));
        else switchto("identifier") && reconsume();
      }
      else if(code == 0x5d) emit(new CloseSquareToken);
      else if(code == 0x7b) emit(new OpenCurlyToken);
      else if(code == 0x7d) emit(new CloseCurlyToken);
      else if(digit(code)) switchto("number") && reconsume();
      else if(code == 0x55 || code == 0x75) {
        if(next(1) == 0x2b && hexdigit(next(2))) consume() && switchto("unicode-range");
        else if((next(1) == 0x52 || next(1) == 0x72) && (next(2) == 0x4c || next(2) == 0x6c) && (next(3) == 0x28)) consume(3) && switchto("url");
        else switchto("identifier") && reconsume();
      }
      else if(namestartchar(code)) switchto("identifier") && reconsume();
      else if(eof()) { emit(new EOFToken); return tokens; }
      else emit(new DelimToken(code));
      break;

    case "double-quote-string":
      if(currtoken == undefined) create(new StringToken);

      if(code == 0x22) emit() && switchto("data");
      else if(eof()) parseerror() && emit() && switchto("data");
      else if(newline(code)) parseerror() && emit(new BadStringToken) && switchto("data") && reconsume();
      else if(code == 0x5c) {
        if(badescape(next())) parseerror() && emit(new BadStringToken) && switchto("data");
        else if(newline(next())) consume();
        else currtoken.append(consumeEscape());
      }
      else currtoken.append(code);
      break;

    case "single-quote-string":
      if(currtoken == undefined) create(new StringToken);

      if(code == 0x27) emit() && switchto("data");
      else if(eof()) parseerror() && emit() && switchto("data");
      else if(newline(code)) parseerror() && emit(new BadStringToken) && switchto("data") && reconsume();
      else if(code == 0x5c) {
        if(badescape(next())) parseerror() && emit(new BadStringToken) && switchto("data");
        else if(newline(next())) consume();
        else currtoken.append(consumeEscape());
      }
      else currtoken.append(code);
      break;

    case "hash":
      if(namechar(code)) create(new HashToken(code)) && switchto("hash-rest");
      else if(code == 0x5c) {
        if(badescape(next())) parseerror() && emit(new DelimToken(0x23)) && switchto("data") && reconsume();
        else create(new HashToken(consumeEscape())) && switchto('hash-rest');
      }
      else emit(new DelimToken(0x23)) && switchto('data') && reconsume();
      break;

    case "hash-rest":
      if(namechar(code)) currtoken.append(code);
      else if(code == 0x5c) {
        if(badescape(next())) parseerror() && emit(new DelimToken(0x23)) && switchto("data") && reconsume();
        else currtoken.append(consumeEscape());
      }
      else emit() && switchto('data') && reconsume();
      break;

    case "comment":
      if(code == 0x2a) {
        if(next() == 0x2f) consume() && switchto('data');
        else donothing();
      }
      else if(eof()) parseerror() && switchto('data') && reconsume();
      else donothing();
      break;

    case "at-keyword":
      if(code == 0x2d) {
        if(namestartchar(next())) consume() && create(new AtKeywordToken([0x40,code])) && switchto('at-keyword-rest');
        else emit(new DelimToken(0x40)) && switchto('data') && reconsume();
      }
      else if(namestartchar(code)) create(new AtKeywordToken(code)) && switchto('at-keyword-rest');
      else if(code == 0x5c) {
        if(badescape(next())) parseerror() && emit(new DelimToken(0x23)) && switchto("data") && reconsume();
        else create(new AtKeywordToken(consumeEscape())) && switchto('at-keyword-rest');
      }
      else emit(new DelimToken(0x40)) && switchto('data') && reconsume();
      break;

    case "at-keyword-rest":
      if(namechar(code)) currtoken.append(code);
      else if(code == 0x5c) {
        if(badescape(next())) parseerror() && emit() && switchto("data") && reconsume();
        else currtoken.append(consumeEscape());
      }
      else emit() && switchto('data') && reconsume();
      break;

    case "identifier":
      if(code == 0x2d) {
        if(namestartchar(next())) create(new IdentifierToken(code)) && switchto('identifier-rest');
        else switchto('data') && reconsume();
      }
      else if(namestartchar(code)) create(new IdentifierToken(code)) && switchto('identifier-rest');
      else if(code == 0x5c) {
        if(badescape(next())) parseerror() && switchto("data") && reconsume();
        else create(new IdentifierToken(consumeEscape())) && switchto('identifier-rest');
      }
      else switchto('data') && reconsume();
      break;

    case "identifier-rest":
      if(namechar(code)) currtoken.append(code);
      else if(code == 0x5c) {
        if(badescape(next())) parseerror() && emit() && switchto("data") && reconsume();
        else currtoken.append(consumeEscape());
      }
      else if(code == 0x28) emit(new FunctionToken(currtoken)) && switchto('data');
      else if(whitespace(code) && options.transformFunctionWhitespace) switchto('transform-function-whitespace');
      else emit() && switchto('data') && reconsume();
      break;

    case "transform-function-whitespace":
      if(whitespace(code)) donothing();
      else if(code == 0x28) emit(new FunctionToken(currtoken)) && switchto('data');
      else emit() && switchto('data') && reconsume();
      break;

    case "number":
      create(new NumberToken());

      if(code == 0x2d) {
        if(digit(next())) consume() && currtoken.append([0x2d,code]) && switchto('number-rest');
        else if(next(1) == 0x2e && digit(next(2))) consume(2) && currtoken.append([0x2d,0x2e,code]) && switchto('number-fraction');
        else switchto('data') && reconsume();
      }
      else if(code == 0x2b) {
        if(digit(next())) consume() && currtoken.append([0x2b,code]) && switchto('number-rest');
        else if(next(1) == 0x2e && digit(next(2))) consume(2) && currtoken.append([0x2b,0x2e,code]) && switchto('number-fraction');
        else switchto('data') && reconsume();
      }
      else if(digit(code)) currtoken.append(code) && switchto('number-rest');
      else if(code == 0x2e) {
        if(digit(next())) consume() && currtoken.append([0x2e,code]) && switchto('number-fraction');
        else switchto('data') && reconsume();
      }
      else switchto('data') && reconsume();
      break;

    case "number-rest":
      if(digit(code)) currtoken.append(code);
      else if(code == 0x2e) {
        if(digit(next())) consume() && currtoken.append([0x2e,code]) && switchto('number-fraction');
        else emit() && switchto('data') && reconsume();
      }
      else if(code == 0x25) emit(new PercentageToken(currtoken)) && switchto('data') && reconsume();
      else if(code == 0x45 || code == 0x65) {
        if(!options.scientificNotation) create(new DimensionToken(currtoken,code)) && switchto('dimension');
        else if(digit(next())) consume() && currtoken.append([0x25,code]) && switchto('sci-notation');
        else if((next(1) == 0x2b || next(1) == 0x2d) && digit(next(2))) currtoken.append([0x25,next(1),next(2)]) && consume(2) && switchto('sci-notation');
        else create(new DimensionToken(currtoken,code)) && switchto('dimension');
      }
      else if(code == 0x2d) {
        if(namestartchar(next())) consume() && create(new DimensionToken(currtoken,[0x2d,code])) && switchto('dimension');
        else if(next(1) == 0x5c && badescape(next(2))) parseerror() && emit() && switchto('data') && reconsume();
        else if(next(1) == 0x5c) consume() && create(new DimensionToken(currtoken, [0x2d,consumeEscape()])) && switchto('dimension');
        else emit() && switchto('data') && reconsume();
      }
      else if(namestartchar(code)) create(new DimensionToken(currtoken, code)) && switchto('dimension');
      else if(code == 0x5c) {
        if(badescape(next)) emit() && switchto('data') && reconsume();
        else create(new DimensionToken(currtoken,consumeEscape)) && switchto('dimension');
      }
      else emit() && switchto('data') && reconsume();
      break;

    case "number-fraction":
      currtoken.type = "number";

      if(digit(code)) currtoken.append(code);
      else if(code == 0x2e) emit() && switchto('data') && reconsume();
      else if(code == 0x25) emit(new PercentageToken(currtoken)) && switchto('data') && reconsume();
      else if(code == 0x45 || code == 0x65) {
        if(!options.scientificNotation) create(new DimensionToken(currtoken,code)) && switchto('dimension');
        else if(digit(next())) consume() && currtoken.append([0x25,code]) && switchto('sci-notation');
        else if((next(1) == 0x2b || next(1) == 0x2d) && digit(next(2))) currtoken.append([0x25,next(1),next(2)]) && consume(2) && switchto('sci-notation');
        else create(new DimensionToken(currtoken,code)) && switchto('dimension');
      }
      else if(code == 0x2d) {
        if(namestartchar(next())) consume() && create(new DimensionToken(currtoken,[0x2d,code])) && switchto('dimension');
        else if(next(1) == 0x5c && badescape(next(2))) parseerror() && emit() && switchto('data') && reconsume();
        else if(next(1) == 0x5c) consume() && create(new DimensionToken(currtoken, [0x2d,consumeEscape()])) && switchto('dimension');
        else emit() && switchto('data') && reconsume();
      }
      else if(namestartchar(code)) create(new DimensionToken(currtoken, code)) && switchto('dimension');
      else if(code == 0x5c) {
        if(badescape(next)) emit() && switchto('data') && reconsume();
        else create(new DimensionToken(currtoken,consumeEscape)) && switchto('dimension');
      }
      else emit() && switchto('data') && reconsume();
      break;

    case "dimension":
      if(namechar(code)) currtoken.append(code);
      else if(code == 0x5c) {
        if(badescape(next())) parseerror() && emit() && switchto('data') && reconsume();
        else currtoken.append(consumeEscape());
      }
      else emit() && switchto('data') && reconsume();
      break;

    case "sci-notation":
      if(digit(code)) currtoken.append(code);
      else emit() && switchto('data') && reconsume();
      break;

    case "url":
      if(code == 0x22) switchto('url-double-quote');
      else if(code == 0x27) switchto('url-single-quote');
      else if(code == 0x29) emit(new URLToken) && switchto('data');
      else if(whitespace(code)) donothing();
      else switchto('url-unquoted') && reconsume();
      break;

    case "url-double-quote":
      if(currtoken == undefined) create(new URLToken);

      if(code == 0x22) switchto('url-end');
      else if(newline(code)) parseerror() && switchto('bad-url');
      else if(code == 0x5c) {
        if(newline(next())) consume();
        else if(badescape(next())) parseerror() && emit(new BadURLToken) && switchto('data') && reconsume();
        else currtoken.append(consumeEscape());
      }
      else currtoken.append(code);
      break;

    case "url-single-quote":
      if(currtoken == undefined) create(new URLToken);

      if(code == 0x27) switchto('url-end');
      else if(newline(code)) parseerror() && switchto('bad-url');
      else if(code == 0x5c) {
        if(newline(next())) consume();
        else if(badescape(next())) parseerror() && emit(new BadURLToken) && switchto('data') && reconsume();
        else currtoken.append(consumeEscape());
      }
      else currtoken.append(code);
      break;

    case "url-end":
      if(whitespace(code)) donothing();
      else if(code == 0x29) emit() && switchto('data');
      else parseerror() && switchto('bad-url') && reconsume();
      break;

    case "url-unquoted":
      if(currtoken == undefined) create(new URLToken);

      if(whitespace(code)) switchto('url-end');
      else if(code == 0x29) emit() && switchto('data');
      else if(code == 0x22 || code == 0x27 || code == 0x28 || nonprintable(code)) parseerror() && switchto('bad-url');
      else if(code == 0x5c) {
        if(badescape(next())) parseerror() && switchto('bad-url');
        else currtoken.append(consumeEscape());
      }
      else currtoken.append(code);
      break;

    case "bad-url":
      if(code == 0x29) emit(new BadURLToken) && switchto('data');
      else if(code == 0x5c) {
        if(badescape(next())) donothing();
        else consumeEscape()
      }
      else donothing();
      break;

    case "unicode-range":
      // We already know that the current code is a hexdigit.

      var start = [code], end = [code];

      for(var total = 1; total < 6; total++) {
        if(hexdigit(next())) {
          consume();
          start.push(code);
          end.push(code);
        }
        else break;
      }

      if(next() == 0x3f) {
        for(;total < 6; total++) {
          if(next() == 0x3f) {
            consume();
            start.push("0".charCodeAt(0));
            end.push("f".charCodeAt(0));
          }
          else break;
        }
        emit(new UnicodeRangeToken(start,end)) && switchto('data');
      }
      else if(next(1) == 0x2d && hexdigit(next(2))) {
        consume();
        consume();
        end = [code];
        for(var total = 1; total < 6; total++) {
          if(hexdigit(next())) {
            consume();
            end.push(code);
          }
          else break;
        }
        emit(new UnicodeRangeToken(start,end)) && switchto('data');
      }
      else emit(new UnicodeRangeToken(start)) && switchto('data');
      break;

    default:
      console.log("Unknown state '" + state + "'");
    }
  }
}

function stringFromCodeArray(arr) {
  return String.fromCharCode.apply(null,arr.filter(function(e){return e;}));
}

function CSSParserToken(options) { return this; }
CSSParserToken.prototype.finish = function() { return this; }
CSSParserToken.prototype.toString = function() { return this.tokenType; }
CSSParserToken.prototype.toJSON = function() { return this.toString(); }

function BadStringToken() { return this; }
BadStringToken.prototype = new CSSParserToken;
BadStringToken.prototype.tokenType = "BADSTRING";

function BadURLToken() { return this; }
BadURLToken.prototype = new CSSParserToken;
BadURLToken.prototype.tokenType = "BADURL";

function WhitespaceToken() { return this; }
WhitespaceToken.prototype = new CSSParserToken;
WhitespaceToken.prototype.tokenType = "WHITESPACE";
WhitespaceToken.prototype.toString = function() { return "WS"; }

function CDOToken() { return this; }
CDOToken.prototype = new CSSParserToken;
CDOToken.prototype.tokenType = "CDO";

function CDCToken() { return this; }
CDCToken.prototype = new CSSParserToken;
CDCToken.prototype.tokenType = "CDC";

function ColonToken() { return this; }
ColonToken.prototype = new CSSParserToken;
ColonToken.prototype.tokenType = ":";

function SemicolonToken() { return this; }
SemicolonToken.prototype = new CSSParserToken;
SemicolonToken.prototype.tokenType = ";";

function OpenCurlyToken() { return this; }
OpenCurlyToken.prototype = new CSSParserToken;
OpenCurlyToken.prototype.tokenType = "{";

function CloseCurlyToken() { return this; }
CloseCurlyToken.prototype = new CSSParserToken;
CloseCurlyToken.prototype.tokenType = "}";

function OpenSquareToken() { return this; }
OpenSquareToken.prototype = new CSSParserToken;
OpenSquareToken.prototype.tokenType = "[";

function CloseSquareToken() { return this; }
CloseSquareToken.prototype = new CSSParserToken;
CloseSquareToken.prototype.tokenType = "]";

function OpenParenToken() { return this; }
OpenParenToken.prototype = new CSSParserToken;
OpenParenToken.prototype.tokenType = "(";

function CloseParenToken() { return this; }
CloseParenToken.prototype = new CSSParserToken;
CloseParenToken.prototype.tokenType = ")";

function EOFToken() { return this; }
EOFToken.prototype = new CSSParserToken;
EOFToken.prototype.tokenType = "EOF";

function DelimToken(code) {
  this.value = String.fromCharCode(code);
  return this;
}
DelimToken.prototype = new CSSParserToken;
DelimToken.prototype.tokenType = "DELIM";
DelimToken.prototype.toString = function() { return "DELIM("+this.value+")"; }

function StringValuedToken() { return this; }
StringValuedToken.prototype = new CSSParserToken;
StringValuedToken.prototype.append = function(val) {
  if(val instanceof Array) {
    for(var i = 0; i < val.length; i++) {
      this.value.push(val[i]);
    }
  } else {
    this.value.push(val);
  }
  return true;
}
StringValuedToken.prototype.finish = function() {
  this.value = stringFromCodeArray(this.value);
  return this;
}

function IdentifierToken(val) {
  this.value = [];
  this.append(val);
}
IdentifierToken.prototype = new StringValuedToken;
IdentifierToken.prototype.tokenType = "IDENT";
IdentifierToken.prototype.toString = function() { return "IDENT("+this.value+")"; }

function FunctionToken(val) {
  // These are always constructed by passing an IdentifierToken
  this.value = val.finish().value;
}
FunctionToken.prototype = new CSSParserToken;
FunctionToken.prototype.tokenType = "FUNCTION";
FunctionToken.prototype.toString = function() { return "FUNCTION("+this.value+")"; }

function AtKeywordToken(val) {
  this.value = [];
  this.append(val);
}
AtKeywordToken.prototype = new StringValuedToken;
AtKeywordToken.prototype.tokenType = "AT-KEYWORD";
AtKeywordToken.prototype.toString = function() { return "AT("+this.value+")"; }

function HashToken(val) {
  this.value = [];
  this.append(val);
}
HashToken.prototype = new StringValuedToken;
HashToken.prototype.tokenType = "HASH";
HashToken.prototype.toString = function() { return "HASH("+this.value+")"; }

function StringToken(val) {
  this.value = [];
  this.append(val);
}
StringToken.prototype = new StringValuedToken;
StringToken.prototype.tokenType = "STRING";
StringToken.prototype.toString = function() { return "\""+this.value+"\""; }

function URLToken(val) {
  this.value = [];
  this.append(val);
}
URLToken.prototype = new StringValuedToken;
URLToken.prototype.tokenType = "URL";
URLToken.prototype.toString = function() { return "URL("+this.value+")"; }

function NumberToken(val) {
  this.value = [];
  this.append(val);
  this.type = "integer";
}
NumberToken.prototype = new StringValuedToken;
NumberToken.prototype.tokenType = "NUMBER";
NumberToken.prototype.toString = function() {
  if(this.type == "integer")
    return "INT("+this.value+")";
  return "NUMBER("+this.value+")";
}
NumberToken.prototype.finish = function() {
  this.repr = stringFromCodeArray(this.value);
  this.value = this.repr * 1;
  if(Math.abs(this.value) % 1 != 0) this.type = "number";
  return this;
}

function PercentageToken(val) {
  // These are always created by passing a NumberToken as val
  val.finish();
  this.value = val.value;
  this.repr = val.repr;
}
PercentageToken.prototype = new CSSParserToken;
PercentageToken.prototype.tokenType = "PERCENTAGE";
PercentageToken.prototype.toString = function() { return "PERCENTAGE("+this.value+")"; }

function DimensionToken(val,unit) {
  // These are always created by passing a NumberToken as the val
  val.finish();
  this.num = val.value;
  this.unit = [];
  this.repr = val.repr;
  this.append(unit);
}
DimensionToken.prototype = new CSSParserToken;
DimensionToken.prototype.tokenType = "DIMENSION";
DimensionToken.prototype.toString = function() { return "DIM("+this.num+","+this.unit+")"; }
DimensionToken.prototype.append = function(val) {
  if(val instanceof Array) {
    for(var i = 0; i < val.length; i++) {
      this.unit.push(val[i]);
    }
  } else {
    this.unit.push(val);
  }
  return true;
}
DimensionToken.prototype.finish = function() {
  this.unit = stringFromCodeArray(this.unit);
  this.repr += this.unit;
  return this;
}

function UnicodeRangeToken(start,end) {
  // start and end are array of char codes, completely finished
  start = parseInt(stringFromCodeArray(start),16);
  if(end === undefined) end = start + 1;
  else end = parseInt(stringFromCodeArray(end),16);

  if(start > maximumallowedcodepoint) end = start;
  if(end < start) end = start;
  if(end > maximumallowedcodepoint) end = maximumallowedcodepoint;

  this.start = start;
  this.end = end;
  return this;
}
UnicodeRangeToken.prototype = new CSSParserToken;
UnicodeRangeToken.prototype.tokenType = "UNICODE-RANGE";
UnicodeRangeToken.prototype.toString = function() {
  if(this.start+1 == this.end)
    return "UNICODE-RANGE("+this.start.toString(16).toUpperCase()+")";
  if(this.start < this.end)
    return "UNICODE-RANGE("+this.start.toString(16).toUpperCase()+"-"+this.end.toString(16).toUpperCase()+")";
  return "UNICODE-RANGE()";
}
UnicodeRangeToken.prototype.contains = function(code) {
  return code >= this.start && code < this.end;
}


// Exportation.
// TODO: also export the various tokens objects?
module.exports = tokenize;

}));
