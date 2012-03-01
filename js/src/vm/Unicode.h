/*
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
 * The Original Code is SpiderMonkey JavaScript engine.
 *
 * The Initial Developer of the Original Code is
 * SpiderMonkey Unicode support code.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Tom Schuster <evilpies@gmail.com>
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

#ifndef Unicode_h__
#define Unicode_h__

#include "mozilla/StandardInteger.h"

#include "jspubtd.h"

#ifdef DEBUG
#include <stdio.h> /* For EOF */
#endif

extern const bool js_isidstart[];
extern const bool js_isident[];
extern const bool js_isspace[];

namespace js {
namespace unicode {

/*
 * This enum contains the all the knowledge required to handle
 * Unicode in JavaScript.
 *
 * SPACE
 *   Every character that is either in the ECMA-262 5th Edition
 *   class WhiteSpace or LineTerminator.
 *
 *   WhiteSpace
 *    \u0009, \u000B, \u000C, \u0020, \u00A0 and \uFEFF
 *    and every other Unicode character with the General Category "Zs".
 *    In pratice this is every character with the value "Zs" as the third
 *    field (after the char code in hex, and the name) called General_Category
 *    (see http://www.unicode.org/reports/tr44/#UnicodeData.txt)
 *     in the file UnicodeData.txt.
 *
 *   LineTerminator
 *    \u000A, \u000D, \u2028, \u2029
 *
 * LETTER
 *   This are all characters included UnicodeLetter from ECMA-262.
 *   This includes the category 'Lu', 'Ll', 'Lt', 'Lm', 'Lo', 'Nl'
 *
 * IDENTIFIER_PART
 *   This is UnicodeCombiningMark, UnicodeDigit, UnicodeConnectorPunctuation.
 *   Aka categories Mn/Mc, Md, Nd, Pc
 *   And <ZWNJ> and <ZWJ>.
 *   Attention: FLAG_LETTER is _not_ IdentifierStart, but you could build
 *   a matcher for the real IdentifierPart like this:
 *
 *   if isEscapeSequence():
 *      handleEscapeSequence()
 *      return True
 *   if char in ['$', '_']:
 *      return True
 *   if GetFlag(char) & (FLAG_IDENTIFIER_PART | FLAG_LETTER):
 *      return True
 *
 * NO_DELTA
 *   See comment in CharacterInfo
 *
 * ENCLOSING_MARK / COMBINING_SPACING_MARK
 *   Something for E4X....
 */

struct CharFlag {
    enum temp {
        SPACE  = 1 << 0,
        LETTER = 1 << 1,
        IDENTIFIER_PART = 1 << 2,
        NO_DELTA = 1 << 3,
        ENCLOSING_MARK = 1 << 4,
        COMBINING_SPACING_MARK = 1 << 5
    };
};

const jschar BYTE_ORDER_MARK2 = 0xFFFE;
const jschar NO_BREAK_SPACE  = 0x00A0;

class CharacterInfo {
    /*
     * upperCase and loweCase normally store the delta between two
     * letters. For example the lower case alpha (a) has the char code
     * 97, and the upper case alpha (A) has 65. So for "a" we would
     * store -32 in upperCase (97 + (-32) = 65) and 0 in lowerCase,
     * because this char is already in lower case.
     * Well, not -32 exactly, but (2**16 - 32) to induce
     * unsigned overflow with identical mathematical behavior.
     * For upper case alpha, we would store 0 in upperCase and 32 in
     * lowerCase (65 + 32 = 97).
     *
     * If the delta between the chars wouldn't fit in a T, the flag
     * FLAG_NO_DELTA is set, and you can just use upperCase and lowerCase
     * without adding them the base char. See CharInfo.toUpperCase().
     *
     * We use deltas to reuse information for multiple characters. For
     * example the whole lower case latin alphabet fits into one entry,
     * because it's always a UnicodeLetter and upperCase contains
     * -32.
     */
  public:
    uint16_t upperCase;
    uint16_t lowerCase;
    uint8_t flags;

    inline bool isSpace() const {
        return flags & CharFlag::SPACE;
    }

    inline bool isLetter() const {
        return flags & CharFlag::LETTER;
    }

    inline bool isIdentifierPart() const {
        return flags & (CharFlag::IDENTIFIER_PART | CharFlag::LETTER);
    }

    inline bool isEnclosingMark() const {
        return flags & CharFlag::ENCLOSING_MARK;
    }

    inline bool isCombiningSpacingMark() const {
        return flags & CharFlag::COMBINING_SPACING_MARK;
    }
};

extern const uint8_t index1[];
extern const uint8_t index2[];
extern const CharacterInfo js_charinfo[];

inline const CharacterInfo&
CharInfo(jschar code)
{
    size_t index = index1[code >> 6];
    index = index2[(index << 6) + (code & 0x3f)];

    return js_charinfo[index];
}

inline bool
IsIdentifierStart(jschar ch)
{
    /*
     * ES5 7.6 IdentifierStart
     *  $ (dollar sign)
     *  _ (underscore)
     *  or any UnicodeLetter.
     *
     * We use a lookup table for small and thus common characters for speed.
     */

    if (ch < 128)
        return js_isidstart[ch];

    return CharInfo(ch).isLetter();
}

inline bool
IsIdentifierPart(jschar ch)
{
    /* Matches ES5 7.6 IdentifierPart. */

    if (ch < 128)
        return js_isident[ch];

    return CharInfo(ch).isIdentifierPart();
}

inline bool
IsLetter(jschar ch)
{
    return CharInfo(ch).isLetter();
}

inline bool
IsSpace(jschar ch)
{
    /*
     * IsSpace checks if some character is included in the merged set
     * of WhiteSpace and LineTerminator, specified by ES5 7.2 and 7.3.
     * We combined them, because in practice nearly every
     * calling function wants this, except some code in the tokenizer.
     *
     * We use a lookup table for ASCII-7 characters, because they are
     * very common and must be handled quickly in the tokenizer.
     * NO-BREAK SPACE is supposed to be the most common character not in
     * this range, so we inline this case, too.
     */

    if (ch < 128)
        return js_isspace[ch];

    if (ch == NO_BREAK_SPACE)
        return true;

    return CharInfo(ch).isSpace();
}

inline bool
IsSpaceOrBOM2(jschar ch)
{
    if (ch < 128)
        return js_isspace[ch];

    /* We accept BOM2 (0xFFFE) for compatibility reasons in the parser. */
    if (ch == NO_BREAK_SPACE || ch == BYTE_ORDER_MARK2)
        return true;

    return CharInfo(ch).isSpace();
}

inline jschar
ToUpperCase(jschar ch)
{
    const CharacterInfo &info = CharInfo(ch);

    /*
     * The delta didn't fit into T, so we had to store the
     * actual char code.
     */
    if (info.flags & CharFlag::NO_DELTA)
        return info.upperCase;

    return uint16_t(ch) + info.upperCase;
}

inline jschar
ToLowerCase(jschar ch)
{
    const CharacterInfo &info = CharInfo(ch);

    if (info.flags & CharFlag::NO_DELTA)
        return info.lowerCase;

    return uint16_t(ch) + info.lowerCase;
}

/* XML support functions */

inline bool
IsXMLSpace(jschar ch)
{
    return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
}

inline bool
IsXMLNamespaceStart(jschar ch)
{
    if (ch == '_')
        return true;

    return CharInfo(ch).isCombiningSpacingMark() || IsIdentifierStart(ch);
}

inline bool
IsXMLNamespacePart(jschar ch)
{
    if (ch == '.' || ch == '-' || ch == '_')
        return true;

    return CharInfo(ch).isEnclosingMark() || IsIdentifierPart(ch);
}

inline bool
IsXMLNameStart(jschar ch)
{
    if (ch == '_' || ch == ':')
        return true;

    return CharInfo(ch).isCombiningSpacingMark() || IsIdentifierStart(ch);
}

inline bool
IsXMLNamePart(jschar ch)
{
    if (ch == '.' || ch == '-' || ch == '_' || ch == ':')
        return true;

    return CharInfo(ch).isEnclosingMark() || IsIdentifierPart(ch);
}


} /* namespace unicode */
} /* namespace js */

#endif /* Unicode_h__ */
