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

#ifndef strings_h___
#define strings_h___

#include <string>

#include "utilities.h"
#include "mem.h"

namespace JavaScript
{
    // Special char16s
    namespace uni {
        const char16 null = '\0';
        const char16 cr = '\r';
        const char16 lf = '\n';
        const char16 space = ' ';
        const char16 ls = 0x2028;
        const char16 ps = 0x2029;
    }
    const uint16 firstFormatChar = 0x200C;  // Lowest Unicode Cf character

    inline char16 widen(char ch) {return static_cast<char16>(static_cast<uchar>(ch));}

    // Use char16Value to compare char16's for inequality because an
    // implementation may have char16's be either signed or unsigned.
    inline uint16 char16Value(char16 ch) {return static_cast<uint16>(ch);}

    // A type that can hold any char16 plus one special value: ueof.
    typedef uint32 char16orEOF;
    const char16orEOF char16eof = static_cast<char16orEOF>(-1);

    // If c is a char16, return it; if c is char16eof, return the character \uFFFF.
    inline char16 char16orEOFToChar16(char16orEOF c) {return static_cast<char16>(c);}


//
// Unicode UTF-16 characters and strings
//

    // A string of UTF-16 characters.  Nulls are allowed just like any other
    // character. The string is not null-terminated.
    // Use wstring if char16 is wchar_t.  Otherwise use basic_string<uint16>.
    //
    // Eventually we'll want to use a custom class better suited for JavaScript that
    // generates less code bloat and separates the concepts of a fixed, read-only
    // string from a mutable buffer that is expanding.  For now, though, we use the
    // standard basic_string.
    typedef std::basic_string<char16> String;
    typedef String string16;
    typedef string16::const_iterator string16_citer;
    typedef string string8;
    typedef string8::const_iterator string8_citer;

    String &newArenaString(Arena &arena);
    String &newArenaString(Arena &arena, const String &str);

#ifndef _WIN32
    // Return a String containing the characters of the null-terminated C
    // string cstr (without the trailing null).
    //
    // This function is inline in the vain hope that some compiler would do a return
    // value optimization and avoid creating and destroying a temporary String when
    // widenCString is called.  We can still hope....
    inline String widenCString(const char *cstr) {
        size_t len = strlen(cstr);
        const uchar *ucstr = reinterpret_cast<const uchar *>(cstr);
        return String(ucstr, ucstr+len);
    }

    // Widen and append length characters starting at chars to the end of str.
    inline void appendChars(String &str, const char *chars, size_t length) {
        const uchar *uchars = reinterpret_cast<const uchar *>(chars);
        str.append(uchars, uchars + length);
    }

        // Widen and append characters between begin and end to the end of str.
    inline void appendChars(String &str, const char *begin, const char *end) {
        ASSERT(begin <= end);
        str.append(reinterpret_cast<const uchar *>(begin), reinterpret_cast<const uchar *>(end));
    }

    // Widen and insert length characters starting at chars into the given
    // position of str.
    inline void insertChars(String &str, String::size_type pos, const char *chars, size_t length) {
        ASSERT(pos <= str.size());
        const uchar *uchars = reinterpret_cast<const uchar *>(chars);
        str.insert(str.begin() + pos, uchars, uchars + length);
    }
#else
    // Microsoft VC6 bug: String constructor and append limited to char16 iterators
    String widenCString(const char *cstr);
    void appendChars(String &str, const char *chars, size_t length);
    inline void appendChars(String &str, const char *begin, const char *end) {
        ASSERT(begin <= end);
        appendChars(str, begin, toSize_t(end - begin));
    }

    void insertChars(String &str, String::size_type pos, const char *chars, size_t length);
#endif

    void insertChars(String &str, String::size_type pos, const char *cstr);

    // Compare a region of string 1, starting at offset 1 with string 2, offset 2
    // for a maximum of count characters and return true or false if the regions match
    bool regionMatches(const String &str1, uint32 offset1, const String &str2, uint32 offset2, uint32 count, bool ignoreCase);
    // ditto, only string 1 is a char* and offset 1 is zero
    bool regionMatches(const char *str1, const String &str2, uint32 offset2, uint32 count, bool ignoreCase);


    String &operator+=(String &str, const char *cstr);
    String operator+(const String &str, const char *cstr);
    String operator+(const char *cstr, const String &str);
    inline String &operator+=(String &str, char c) {return str += widen(c);}
    inline void clear(String &s) {s.resize(0);}


    class CharInfo {
        uint32 info;                    // Word from table a.

        // Unicode character attribute lookup tables
        static const uint8 x[];
        static const uint8 y[];
        static const uint32 a[];

      public:
        // Enumerated Unicode general category types
        enum Type {
            Unassigned            = 0,  // Cn
            UppercaseLetter       = 1,  // Lu
            LowercaseLetter       = 2,  // Ll
            TitlecaseLetter       = 3,  // Lt
            ModifierLetter        = 4,  // Lm
            OtherLetter           = 5,  // Lo
            NonSpacingMark        = 6,  // Mn
            EnclosingMark         = 7,  // Me
            CombiningSpacingMark  = 8,  // Mc
            DecimalDigitNumber    = 9,  // Nd
            LetterNumber          = 10, // Nl
            OtherNumber           = 11, // No
            SpaceSeparator        = 12, // Zs
            LineSeparator         = 13, // Zl
            ParagraphSeparator    = 14, // Zp
            Control               = 15, // Cc
            Format                = 16, // Cf
            PrivateUse            = 18, // Co
            Surrogate             = 19, // Cs
            DashPunctuation       = 20, // Pd
            StartPunctuation      = 21, // Ps
            EndPunctuation        = 22, // Pe
            ConnectorPunctuation  = 23, // Pc
            OtherPunctuation      = 24, // Po
            MathSymbol            = 25, // Sm
            CurrencySymbol        = 26, // Sc
            ModifierSymbol        = 27, // Sk
            OtherSymbol           = 28  // So
        };

        enum Group {
            NonIdGroup,         // 0  May not be part of an identifier
            FormatGroup,        // 1  Format control
            IdGroup,            // 2  May start or continue a JS identifier (includes $ and _)
            IdContinueGroup,    // 3  May continue a JS identifier [(IdContinueGroup & -2) == IdGroup]
            WhiteGroup,         // 4  White space character (but not line break)
            LineBreakGroup      // 5  Line break character [(LineBreakGroup & -2) == WhiteGroup]
        };

        CharInfo() {}
        CharInfo(char16 c): info(a[y[x[static_cast<uint16>(c)>>6]<<6 | c&0x3F]]) {}
        CharInfo(const CharInfo &ci): info(ci.info) {}

        friend Type cType(const CharInfo &ci) {return static_cast<Type>(ci.info & 0x1F);}
        friend Group cGroup(const CharInfo &ci) {return static_cast<Group>(ci.info >> 16 & 7);}

        friend bool isAlpha(const CharInfo &ci) {
            return ((1<<UppercaseLetter | 1<<LowercaseLetter | 1<<TitlecaseLetter | 1<<ModifierLetter | 1<<OtherLetter) >> cType(ci) & 1) != 0;
        }

        friend bool isAlphanumeric(const CharInfo &ci) {
            return ((1<<UppercaseLetter | 1<<LowercaseLetter | 1<<TitlecaseLetter | 1<<ModifierLetter | 1<<OtherLetter |
                     1<<DecimalDigitNumber | 1<<LetterNumber) >> cType(ci) & 1) != 0;
        }

        // Return true if this character can start a JavaScript identifier
        friend bool isIdLeading(const CharInfo &ci) {return cGroup(ci) == IdGroup;}
        // Return true if this character can continue a JavaScript identifier
        friend bool isIdContinuing(const CharInfo &ci) {return (cGroup(ci) & -2) == IdGroup;}

        // Return true if this character is a Unicode decimal digit (Nd) character
        friend bool isDecimalDigit(const CharInfo &ci) {return cType(ci) == DecimalDigitNumber;}
        // Return true if this character is a Unicode white space or line break character
        friend bool isSpace(const CharInfo &ci) {return (cGroup(ci) & -2) == WhiteGroup;}
        // Return true if this character is a Unicode line break character (LF, CR, LS, or PS)
        friend bool isLineBreak(const CharInfo &ci) {return cGroup(ci) == LineBreakGroup;}
        // Return true if this character is a Unicode format control character (Cf)
        friend bool isFormat(const CharInfo &ci) {return cGroup(ci) == FormatGroup;}

        friend bool isUpper(const CharInfo &ci) {return cType(ci) == UppercaseLetter;}
        friend bool isLower(const CharInfo &ci) {return cType(ci) == LowercaseLetter;}

        friend char16 toUpper(char16 c);
        friend char16 toLower(char16 c);
    };

    inline bool isASCIIDecimalDigit(char16 c) {return c >= '0' && c <= '9';}
    inline bool isASCIIOctalDigit(char16 c) {return c >= '0' && c <= '7';}
    bool isASCIIHexDigit(char16 c, uint &digit);

    const char16 *skipWhiteSpace(const char16 *str, const char16 *strEnd);
}

#define JS7_ISHEX(c)    ((c) < 128 && isxdigit(c))
#define JS7_UNHEX(c)    (uint32)(isdigit(c) ? (c) - '0' : 10 + tolower(c) - 'a')

#endif /* strings_h___ */
