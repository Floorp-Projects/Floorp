/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the JavaScript 2 Prototype.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <vector>

#include "systemtypes.h"
#include "js2value.h"

#include "numerics.h"
#include "lexer.h"

namespace JS = JavaScript;


// Create a new Lexer for lexing the provided source code.  The Lexer will
// intern identifiers, keywords, and regular expressions in the designated
// world.
JS::Lexer::Lexer(World &world, const String &source, const String &sourceLocation, uint32 initialLineNum):
        world(world), reader(source, sourceLocation, initialLineNum)
{
    nextToken = tokens;
    nTokensFwd = 0;
#ifdef DEBUG
    nTokensBack = 0;
#endif
    lexingUnit = false;
}


// Skip past the next token, which must have been either peeked or read and then unread.
// skip is faster than get but must not be called if the next token has not been seen yet.
void JS::Lexer::skip()
{
    ASSERT(nTokensFwd);
    if (++nextToken == tokens + tokenBufferSize)
        nextToken = tokens;
    --nTokensFwd;
    DEBUG_ONLY(++nTokensBack);
}


// Get and return the next token.  The token remains valid until the next
// call to this Lexer.  If the Reader reached the end of file, return a
// Token whose Kind is end.  The caller may alter the value of this Token
// (in particular, take control over the auto_ptr's data), but if it does so,
// the caller is not allowed to unget this Token.
//
// If preferRegExp is true, a / will be preferentially interpreted as
// starting a regular expression; otherwise, a / will be preferentially
// interpreted as division or /=.
const JS::Token &JS::Lexer::get(bool preferRegExp)
{
    const Token &t = peek(preferRegExp);
    if (++nextToken == tokens + tokenBufferSize)
        nextToken = tokens;
    --nTokensFwd;
    DEBUG_ONLY(++nTokensBack);
    return t;
}


// Peek at the next token using the given preferRegExp setting.  If that
// token's kind matches the given kind, consume that token and return it.
// Otherwise, do not consume that token and return nil.
const JS::Token *JS::Lexer::eat(bool preferRegExp, Token::Kind kind)
{
    const Token &t = peek(preferRegExp);
    if (t.kind != kind)
        return 0;
    if (++nextToken == tokens + tokenBufferSize)
        nextToken = tokens;
    --nTokensFwd;
    DEBUG_ONLY(++nTokensBack);
    return &t;
}


// Return the next token without consuming it.
//
// If preferRegExp is true, a / will be preferentially interpreted as
// starting a regular expression; otherwise, a / will be preferentially
// interpreted as division or /=. A subsequent call to peek or get will
// return the same token; that call must be presented with the same value
// for preferRegExp.
const JS::Token &JS::Lexer::peek(bool preferRegExp)
{
    // Use an already looked-up token if there is one.
    if (nTokensFwd) {
        ASSERT(savedPreferRegExp[nextToken - tokens] == preferRegExp);
    } else {
        lexToken(preferRegExp);
        nTokensFwd = 1;
#ifdef DEBUG
        savedPreferRegExp[nextToken - tokens] = preferRegExp;
        if (nTokensBack == tokenLookahead) {
            nTokensBack = tokenLookahead-1;
            if (tokenGuard)
                (nextToken >= tokens+tokenLookahead ?
                 nextToken-tokenLookahead :
                 nextToken+tokenBufferSize-tokenLookahead)->valid = false;
        }
#endif
    }
    return *nextToken;
}


#ifdef DEBUG
// Change the setting of preferRegExp for an already peeked token.
// The token must not be one for which that setting mattered.
//
// THIS IS A DANGEROUS FUNCTION!
// Use it only if you can be prove that the already peeked token does not
// start with a slash.
void JS::Lexer::redesignate(bool preferRegExp)
{
    if (nTokensFwd) {
        ASSERT(savedPreferRegExp[nextToken - tokens] != preferRegExp);
        ASSERT(!(nextToken->hasKind(Token::regExp) ||
                 nextToken->hasKind(Token::divide) ||
                 nextToken->hasKind(Token::divideEquals)));
        savedPreferRegExp[nextToken - tokens] = preferRegExp;
    }
}
#endif


// Unread the last token.  This call may be called to unread at most
// tokenBufferSize tokens at a time (where a peek also counts as temporarily
// reading and unreading one token). When a token that has been unread is
// peeked or read again, the same value must be passed in preferRegExp as for
// the first time that token was read or peeked.
void JS::Lexer::unget()
{
    ASSERT(nTokensBack--);
    nTokensFwd++;
    if (nextToken == tokens)
        nextToken = tokens + tokenBufferSize;
    --nextToken;
}


// Report a syntax error at the backUp-th last character read by the Reader.
// In other words, if backUp is 0, the error is at the next character to be
// read by the Reader; if backUp is 1, the error is at the last character
// read by the Reader, and so forth.
void JS::Lexer::syntaxError(const char *message, uint backUp)
{
    reader.unget(backUp);
    reader.error(Exception::syntaxError, widenCString(message), reader.getPos());
}


// Get the next character from the reader, skipping any Unicode format-control
// (Cf) characters.
inline char16 JS::Lexer::getChar()
{
    char16 ch = reader.get();
    if (char16Value(ch) >= firstFormatChar)
        ch = internalGetChar(ch);
    return ch;
}


// Helper for getChar()
char16 JS::Lexer::internalGetChar(char16 ch)
{
    while (isFormat(ch))
        ch = reader.get();
    return ch;
}


// Peek the next character from the reader, skipping any Unicode
// format-control (Cf) characters, which are read and discarded.
inline char16 JS::Lexer::peekChar()
{
    char16 ch = reader.peek();
    if (char16Value(ch) >= firstFormatChar)
        ch = internalPeekChar(ch);
    return ch;
}


// Helper for peekChar()
char16 JS::Lexer::internalPeekChar(char16 ch)
{
    while (isFormat(ch)) {
        reader.get();
        ch = reader.peek();
    }
    return ch;
}


// Peek the next character from the reader, skipping any Unicode
// format-control (Cf) characters, which are read and discarded.  If the
// peeked character matches ch, read that character and return true;
// otherwise return false.  ch must not be null.
bool JS::Lexer::testChar(char16 ch)
{
    ASSERT(ch);     // If ch were null, it could match the eof null.
    char16 ch2 = peekChar();
    if (ch == ch2) {
        reader.get();
        return true;
    }
    return false;
}


// A backslash has been read.  Read the rest of the escape code.
// Return the interpreted escaped character.  Throw an exception if the
// escape is not valid. If unicodeOnly is true, allow only \uxxxx escapes.
char16 JS::Lexer::lexEscape(bool unicodeOnly)
{
    char16 ch = getChar();
    int nDigits;

    if (!unicodeOnly || ch == 'u')
        switch (ch) {
          case '0':
            // Make sure that the next character isn't a digit.
            ch = peekChar();
            if (!isASCIIDecimalDigit(ch))
                return 0x00;
/*
            // Point to the next character in the error message
            getChar();
            break;
*/
			/* E3 compatibility, parse the sequence as octal */
            {
                uint32 n = 0;
                while (isASCIIDecimalDigit(ch)) {
                    ch = getChar();
                    n = (n << 3) | (ch - '0');
					ch = peekChar();
                }
                return static_cast<char16>(n);
            }


          case 'b':
            return 0x08;
          case 'f':
            return 0x0C;
          case 'n':
            return 0x0A;
          case 'r':
            return 0x0D;
          case 't':
            return 0x09;
          case 'v':
            return 0x0B;

          case 'x':
            nDigits = 2;
            goto lexHex;
          case 'u':
            nDigits = 4;
          lexHex:
            {
                uint32 n = 0;
                while (nDigits--) {
                    ch = getChar();
                    uint digit;
                    if (!isASCIIHexDigit(ch, digit)) {
						/* E3 compatibility, back off */
						// goto error;
						do {
							reader.unget();
							ch = peekChar();
						} while (ch != '\\');
                        return getChar();
					}
                    n = (n << 4) | digit;
                }
                return static_cast<char16>(n);
            }
          default:
/*
            if (!reader.getEof(ch)) {
                CharInfo chi(ch);
                if (!isAlphanumeric(chi) && !isLineBreak(chi))
                    return ch;
            }
*/
              return ch;
        }
//  error:
    syntaxError("Bad escape code");
    return 0;
}


// Read an identifier into s.  The initial value of s is ignored and cleared.
// Return true if an escape code has been encountered.
// If allowLeadingDigit is true, allow the first character of s to be a digit,
// just like any continuing identifier character.
bool JS::Lexer::lexIdentifier(String &s, bool allowLeadingDigit)
{
    reader.beginRecording(s);
    bool hasEscape = false;

    while (true) {
        char16 ch = getChar();
        char16 ch2 = ch;
        if (ch == '\\') {
            ch2 = lexEscape(true);
            hasEscape = true;
        }
        CharInfo chi2(ch2);

        if (!(allowLeadingDigit ? isIdContinuing(chi2) :
              isIdLeading(chi2))) {
            if (ch == '\\')
                syntaxError("Identifier escape expands into non-identifier character");
            else
                reader.unget();
            break;
        }
        reader.recordChar(ch2);
        allowLeadingDigit = true;
    }
    reader.endRecording();
    return hasEscape;
}


// Read a numeric literal into nextToken->chars and nextToken->value.
// Return true if the numeric literal is followed by a unit, but don't read
// the unit yet.
bool JS::Lexer::lexNumeral()
{
    int hasDecimalPoint = 0;
    bool hexadecimal = false;
    bool octal = false;
    String &s = nextToken->chars;
    uint digit;

    reader.beginRecording(s);
    char16 ch = getChar();
    if (ch == '0') {
        reader.recordChar('0');
        ch = getChar();
        if ((ch&~0x20) == 'X') {
            size_t pos = reader.getPos();
            char16 ch2 = getChar();
            if (isASCIIHexDigit(ch2, digit)) {
                hexadecimal = true;
                reader.recordChar(ch);
                do {
                    reader.recordChar(ch2);
                    ch2 = getChar();
                } while (isASCIIHexDigit(ch2, digit));
                ch = ch2;
            } else
                reader.setPos(pos);
            goto done;
        } else if (isASCIIOctalDigit(ch)) {
            // Backward compatible hack, support octal for SpiderMonkey's sake
            octal = true;
            while (isASCIIOctalDigit(ch)) {
                reader.recordChar(ch);
                ch = getChar();
            }
            goto done;
//          syntaxError("Numeric constant syntax error");
        }
    }
    while (isASCIIDecimalDigit(ch) || ch == '.' && !hasDecimalPoint++) {
        reader.recordChar(ch);
        ch = getChar();
    }
    if ((ch&~0x20) == 'E') {
        size_t pos = reader.getPos();
        char16 ch2 = getChar();
        char16 sign = 0;
        if (ch2 == '+' || ch2 == '-') {
            sign = ch2;
            ch2 = getChar();
        }
        if (isASCIIDecimalDigit(ch2)) {
            reader.recordChar(ch);
            if (sign)
                reader.recordChar(sign);
            do {
                reader.recordChar(ch2);
                ch2 = getChar();
            } while (isASCIIDecimalDigit(ch2));
            ch = ch2;
        } else
            reader.setPos(pos);
    }

  done:
    // At this point the reader is just past the character ch, which
    // is the first non-formatting character that is not part of the
    // number.
    reader.endRecording();
    const char16 *sBegin = s.data();
    const char16 *sEnd = sBegin + s.size();
    const char16 *numEnd;
    nextToken->value = hexadecimal ? 
                            stringToInteger(sBegin, sEnd, numEnd, 16)
                          : octal ? 
                                stringToInteger(sBegin, sEnd, numEnd, 8)
                              : stringToDouble(sBegin, sEnd, numEnd);
    ASSERT(numEnd == sEnd);
    reader.unget();
    ASSERT(ch == reader.peek());
    return isIdContinuing(ch) || ch == '\\';
}


// Read a string literal into s.  The initial value of s is ignored and
// cleared. The opening quote has already been read into separator.
void JS::Lexer::lexString(String &s, char16 separator)
{
    char16 ch;

    reader.beginRecording(s);
    while ((ch = reader.get()) != separator) {
        CharInfo chi(ch);
        if (!isFormat(chi)) {
            if (ch == '\\')
                ch = lexEscape(false);
            else if (reader.getEof(ch) || isLineBreak(chi))
                syntaxError("Unterminated string literal");
            reader.recordChar(ch);
        }
    }
    reader.endRecording();
}


// Read a regular expression literal.  Store the regular expression in
// nextToken->id and the flags in nextToken->chars.
// The opening slash has already been read.
void JS::Lexer::lexRegExp()
{
    String s;
    char16 prevCh = 0;

    reader.beginRecording(s);
    while (true) {
        char16 ch = getChar();
        CharInfo chi(ch);
        if (reader.getEof(ch) || isLineBreak(chi))
            syntaxError("Unterminated regular expression literal");
        if (prevCh == '\\') {
            reader.recordChar(ch);
            // Ignore slashes and backslashes immediately after a backslash
            prevCh = 0;
        } else if (ch != '/') {
            reader.recordChar(ch);
            prevCh = ch;
        } else
            break;
    }
    reader.endRecording();
    nextToken->id = &world.identifiers[s];

    lexIdentifier(nextToken->chars, true);
}


// Read a token from the Reader and store it at *nextToken.
// If the Reader reached the end of file, store a Token whose Kind is end.
void JS::Lexer::lexToken(bool preferRegExp)
{
    Token &t = *nextToken;
    t.lineBreak = false;
    t.id = 0;
    // Don't really need to waste time clearing this string here
    //clear(t.chars);
    Token::Kind kind;

    if (lexingUnit) {
        if (reader.peek() == '_')
            syntaxError("Unit suffix may not begin with an underscore", 0);
        lexIdentifier(t.chars, false);
        ASSERT(t.chars.size());
        kind = Token::unit;   // unit
        lexingUnit = false;
    } else {
      next:
        char16 ch = reader.get();
        if (reader.getEof(ch)) {
          endOfInput:
            t.pos = reader.getPos() - 1;
            kind = Token::end;
        } else {
            char16 ch2;
            CharInfo chi(ch);

            switch (cGroup(chi)) {
              case CharInfo::FormatGroup:
              case CharInfo::WhiteGroup:
                goto next;

              case CharInfo::IdGroup:
                t.pos = reader.getPos() - 1;
              readIdentifier:
                {
                    reader.unget();
                    String s;
                    bool hasEscape = lexIdentifier(s, false);
                    t.id = &world.identifiers[s];
                    kind = hasEscape ? Token::identifier : t.id->tokenKind;
                }
                break;

              case CharInfo::NonIdGroup:
              case CharInfo::IdContinueGroup:
                t.pos = reader.getPos() - 1;
                switch (ch) {
                  case '(':
                    kind = Token::openParenthesis;  // (
                    break;
                  case ')':
                    kind = Token::closeParenthesis; // )
                    break;
                  case '[':
                    kind = Token::openBracket;      // [
                    break;
                  case ']':
                    kind = Token::closeBracket;     // ]
                    break;
                  case '{':
                    kind = Token::openBrace;        // {
                    break;
                  case '}':
                    kind = Token::closeBrace;       // }
                    break;
                  case ',':
                    kind = Token::comma;            // ,
                    break;
                  case ';':
                    kind = Token::semicolon;        // ;
                    break;
                  case '.':
                    kind = Token::dot;              // .
                    ch2 = getChar();
                    if (isASCIIDecimalDigit(ch2)) {
                        reader.setPos(t.pos);
                        goto number;                // decimal point
                    } else if (ch2 == '.') {
                        kind = Token::doubleDot;    // ..
                        if (testChar('.'))
                            kind = Token::tripleDot;// ...
                    } else
                        reader.unget();
                    break;
                  case ':':
                    kind = Token::colon;            // :
                    if (testChar(':'))
                        kind = Token::doubleColon;  // ::
                    break;
                  case '#':
                    kind = Token::pound;            // #
                    break;
                  case '@':
                    kind = Token::at;               // @
                    break;
                  case '?':
                    kind = Token::question;         // ?
                    break;

                  case '~':
                    kind = Token::complement;       // ~
                    break;
                  case '!':
                    kind = Token::logicalNot;       // !
                    if (testChar('=')) {
                        kind = Token::notEqual;     // !=
                        if (testChar('='))
                            kind = Token::notIdentical; // !==
                    }
                    break;

                  case '*':
                    kind = Token::times;            // * *=
                  tryAssignment:
                    if (testChar('='))
                        kind = Token::Kind(kind + Token::timesEquals - Token::times);
                    break;

                  case '/':
                    kind = Token::divide;           // /
                    ch = getChar();
                    if (ch == '/') {                // // comment
                        do {
                            ch = reader.get();
                            if (reader.getEof(ch))
                                goto endOfInput;
                        } while (!isLineBreak(ch));
                        goto endOfLine;
                    } else if (ch == '*') {         // /*comment*/
                        ch = 0;
                        do {
                            ch2 = ch;
                            ch = getChar();
                            if (isLineBreak(ch)) {
                                reader.beginLine();
                                t.lineBreak = true;
                            } else if (reader.getEof(ch))
                                syntaxError("Unterminated /* comment");
                        } while (ch != '/' || ch2 != '*');
                        goto next;
                    } else {
                        reader.unget();
                        if (preferRegExp) {         // Regular expression
                            kind = Token::regExp;
                            lexRegExp();
                        } else
                            goto tryAssignment;     // /=
                    }
                    break;

                  case '%':
                    kind = Token::modulo;           // %
                    goto tryAssignment;             // %=

                  case '+':
                    kind = Token::plus;             // +
                    if (testChar('+'))
                        kind = Token::increment;    // ++
                    else
                        goto tryAssignment;         // +=
                    break;

                  case '-':
                    kind = Token::minus;            // -
                    ch = getChar();
                    if (ch == '-')
                        kind = Token::decrement;    // --
                    else if (ch == '>')
                        kind = Token::arrow;        // ->
                    else {
                        reader.unget();
                        goto tryAssignment;         // -=
                    }
                    break;

                  case '&':
                    kind = Token::bitwiseAnd;       // & && &= &&=
                  logical:
                    if (testChar(ch))
                        kind = Token::Kind(kind - Token::bitwiseAnd + Token::logicalAnd);
                    goto tryAssignment;
                  case '^':
                    kind = Token::bitwiseXor;       // ^ ^^ ^= ^^=
                    goto logical;
                  case '|':
                    kind = Token::bitwiseOr;        // | || |= ||=
                    goto logical;

                  case '=':
                    kind = Token::assignment;       // =
                    if (testChar('=')) {
                        kind = Token::equal;        // ==
                        if (testChar('='))
                            kind = Token::identical; // ===
                    }
                    break;

                  case '<':
                    kind = Token::lessThan;         // <
                    if (testChar('<')) {
                        kind = Token::leftShift;    // <<
                        goto tryAssignment;         // <<=
                    }
                  comparison:
                    if (testChar('='))              // <= >=
                        kind = Token::Kind(kind + Token::lessThanOrEqual - Token::lessThan);
                    break;
                  case '>':
                    kind = Token::greaterThan;      // >
                    if (testChar('>')) {
                        kind = Token::rightShift;   // >>
                        if (testChar('>'))
                            kind = Token::logicalRightShift; // >>>
                        goto tryAssignment;         // >>= >>>=
                    }
                    goto comparison;

                  case '\\':
                    goto readIdentifier;            // An identifier that starts with an escape

                  case '\'':
                  case '"':
                    kind = Token::string;           // 'string' "string"
                    lexString(t.chars, ch);
                    break;

                  case '0':
                  case '1':
                  case '2':
                  case '3':
                  case '4':
                  case '5':
                  case '6':
                  case '7':
                  case '8':
                  case '9':
                    reader.unget();                 // Number
                  number:
                    kind = Token::number;
#ifdef PARSE_UNIT
                    lexingUnit = 
#else
						lexNumeral();
#endif
                    break;

                  default:
                    syntaxError("Bad character");
                }
                break;

              case CharInfo::LineBreakGroup:
              endOfLine:
                reader.beginLine();
                t.lineBreak = true;
                goto next;
            }
        }
    }
    t.kind = kind;
#ifdef DEBUG
    t.valid = true;
#endif
}
