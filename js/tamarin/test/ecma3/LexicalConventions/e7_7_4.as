/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is [Open Source Virtual Machine.].
 *
 * The Initial Developer of the Original Code is
 * Adobe System Incorporated.
 * Portions created by the Initial Developer are Copyright (C) 2005-2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Adobe AS3 Team
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

    var SECTION = "7.7.4";
    var VERSION = "ECMA_1";
    startTest();
    var TITLE   = "String Literals";

    writeHeaderToLog( SECTION + " "+ TITLE);

    var testcases = getTestCases();
    test();

function getTestCases() {
    var array = new Array();
    var item = 0;

    // StringLiteral:: "" and ''

    array[item++] = new TestCase( SECTION, "\"\"",     "",     "" );
    array[item++] = new TestCase( SECTION, "\'\'",     "",      '' );

    // DoubleStringCharacters:: DoubleStringCharacter :: EscapeSequence :: CharacterEscapeSequence
    array[item++] = new TestCase( SECTION, "\\\"",        String.fromCharCode(0x0022),     "\"" );
    array[item++] = new TestCase( SECTION, "\\\'",        String.fromCharCode(0x0027),     "\'" );
    array[item++] = new TestCase( SECTION, "\\",         String.fromCharCode(0x005C),     "\\" );
    array[item++] = new TestCase( SECTION, "\\b",        String.fromCharCode(0x0008),     "\b" );
    array[item++] = new TestCase( SECTION, "\\f",        String.fromCharCode(0x000C),     "\f" );
    array[item++] = new TestCase( SECTION, "\\n",        String.fromCharCode(0x000A),     "\n" );
    array[item++] = new TestCase( SECTION, "\\r",        String.fromCharCode(0x000D),     "\r" );
    array[item++] = new TestCase( SECTION, "\\t",        String.fromCharCode(0x0009),     "\t" );
    array[item++] = new TestCase( SECTION, "\\v",        String.fromCharCode(0x000B),        "\v" );

//  following line commented out as it causes a compile time error
//    array[item++] = new TestCase( SECTION, "\\444",      "444",                         "\444" );

    // DoubleStringCharacters:DoubleStringCharacter::EscapeSequence::HexEscapeSequence
/*
    array[item++] = new TestCase( SECTION, "\\x0",      String.fromCharCode(0),         "\x0" );
    array[item++] = new TestCase( SECTION, "\\x1",      String.fromCharCode(1),         "\x1" );
    array[item++] = new TestCase( SECTION, "\\x2",      String.fromCharCode(2),         "\x2" );
    array[item++] = new TestCase( SECTION, "\\x3",      String.fromCharCode(3),         "\x3" );
    array[item++] = new TestCase( SECTION, "\\x4",      String.fromCharCode(4),         "\x4" );
    array[item++] = new TestCase( SECTION, "\\x5",      String.fromCharCode(5),         "\x5" );
    array[item++] = new TestCase( SECTION, "\\x6",      String.fromCharCode(6),         "\x6" );
    array[item++] = new TestCase( SECTION, "\\x7",      String.fromCharCode(7),         "\x7" );
    array[item++] = new TestCase( SECTION, "\\x8",      String.fromCharCode(8),         "\x8" );
    array[item++] = new TestCase( SECTION, "\\x9",      String.fromCharCode(9),         "\x9" );
    array[item++] = new TestCase( SECTION, "\\xA",      String.fromCharCode(10),         "\xA" );
    array[item++] = new TestCase( SECTION, "\\xB",      String.fromCharCode(11),         "\xB" );
    array[item++] = new TestCase( SECTION, "\\xC",      String.fromCharCode(12),         "\xC" );
    array[item++] = new TestCase( SECTION, "\\xD",      String.fromCharCode(13),         "\xD" );
    array[item++] = new TestCase( SECTION, "\\xE",      String.fromCharCode(14),         "\xE" );
    array[item++] = new TestCase( SECTION, "\\xF",      String.fromCharCode(15),         "\xF" );

*/
    array[item++] = new TestCase( SECTION, "\\xF0",      String.fromCharCode(240),         "\xF0" );
    array[item++] = new TestCase( SECTION, "\\xE1",      String.fromCharCode(225),         "\xE1" );
    array[item++] = new TestCase( SECTION, "\\xD2",      String.fromCharCode(210),         "\xD2" );
    array[item++] = new TestCase( SECTION, "\\xC3",      String.fromCharCode(195),         "\xC3" );
    array[item++] = new TestCase( SECTION, "\\xB4",      String.fromCharCode(180),         "\xB4" );
    array[item++] = new TestCase( SECTION, "\\xA5",      String.fromCharCode(165),         "\xA5" );
    array[item++] = new TestCase( SECTION, "\\x96",      String.fromCharCode(150),         "\x96" );
    array[item++] = new TestCase( SECTION, "\\x87",      String.fromCharCode(135),         "\x87" );
    array[item++] = new TestCase( SECTION, "\\x78",      String.fromCharCode(120),         "\x78" );
    array[item++] = new TestCase( SECTION, "\\x69",      String.fromCharCode(105),         "\x69" );
    array[item++] = new TestCase( SECTION, "\\x5A",      String.fromCharCode(90),         "\x5A" );
    array[item++] = new TestCase( SECTION, "\\x4B",      String.fromCharCode(75),         "\x4B" );
    array[item++] = new TestCase( SECTION, "\\x3C",      String.fromCharCode(60),         "\x3C" );
    array[item++] = new TestCase( SECTION, "\\x2D",      String.fromCharCode(45),         "\x2D" );
    array[item++] = new TestCase( SECTION, "\\x1E",      String.fromCharCode(30),         "\x1E" );
    array[item++] = new TestCase( SECTION, "\\x0F",      String.fromCharCode(15),         "\x0F" );

    // string literals only take up to two hext digits.  therefore, the third character in this string
    // should be interpreted as a StringCharacter and not part of the HextEscapeSequence

    array[item++] = new TestCase( SECTION, "\\xF0F",      String.fromCharCode(240)+"F",         "\xF0F" );
    array[item++] = new TestCase( SECTION, "\\xE1E",      String.fromCharCode(225)+"E",         "\xE1E" );
    array[item++] = new TestCase( SECTION, "\\xD2D",      String.fromCharCode(210)+"D",         "\xD2D" );
    array[item++] = new TestCase( SECTION, "\\xC3C",      String.fromCharCode(195)+"C",         "\xC3C" );
    array[item++] = new TestCase( SECTION, "\\xB4B",      String.fromCharCode(180)+"B",         "\xB4B" );
    array[item++] = new TestCase( SECTION, "\\xA5A",      String.fromCharCode(165)+"A",         "\xA5A" );
    array[item++] = new TestCase( SECTION, "\\x969",      String.fromCharCode(150)+"9",         "\x969" );
    array[item++] = new TestCase( SECTION, "\\x878",      String.fromCharCode(135)+"8",         "\x878" );
    array[item++] = new TestCase( SECTION, "\\x787",      String.fromCharCode(120)+"7",         "\x787" );
    array[item++] = new TestCase( SECTION, "\\x696",      String.fromCharCode(105)+"6",         "\x696" );
    array[item++] = new TestCase( SECTION, "\\x5A5",      String.fromCharCode(90)+"5",         "\x5A5" );
    array[item++] = new TestCase( SECTION, "\\x4B4",      String.fromCharCode(75)+"4",         "\x4B4" );
    array[item++] = new TestCase( SECTION, "\\x3C3",      String.fromCharCode(60)+"3",         "\x3C3" );
    array[item++] = new TestCase( SECTION, "\\x2D2",      String.fromCharCode(45)+"2",         "\x2D2" );
    array[item++] = new TestCase( SECTION, "\\x1E1",      String.fromCharCode(30)+"1",         "\x1E1" );
    array[item++] = new TestCase( SECTION, "\\x0F0",      String.fromCharCode(15)+"0",         "\x0F0" );

    // G is out of hex range

    array[item++] = new TestCase( SECTION, "\\xG",        "xG",                                 "\xG" );
    array[item++] = new TestCase( SECTION, "\\xCG",       "xCG",      				"\xCG" );

    // DoubleStringCharacter::EscapeSequence::CharacterEscapeSequence::\ NonEscapeCharacter
    array[item++] = new TestCase( SECTION, "\\a",    "a",        "\a" );
    array[item++] = new TestCase( SECTION, "\\c",    "c",        "\c" );
    array[item++] = new TestCase( SECTION, "\\d",    "d",        "\d" );
    array[item++] = new TestCase( SECTION, "\\e",    "e",        "\e" );
    array[item++] = new TestCase( SECTION, "\\g",    "g",        "\g" );
    array[item++] = new TestCase( SECTION, "\\h",    "h",        "\h" );
    array[item++] = new TestCase( SECTION, "\\i",    "i",        "\i" );
    array[item++] = new TestCase( SECTION, "\\j",    "j",        "\j" );
    array[item++] = new TestCase( SECTION, "\\k",    "k",        "\k" );
    array[item++] = new TestCase( SECTION, "\\l",    "l",        "\l" );
    array[item++] = new TestCase( SECTION, "\\m",    "m",        "\m" );
    array[item++] = new TestCase( SECTION, "\\o",    "o",        "\o" );
    array[item++] = new TestCase( SECTION, "\\p",    "p",        "\p" );
    array[item++] = new TestCase( SECTION, "\\q",    "q",        "\q" );
    array[item++] = new TestCase( SECTION, "\\s",    "s",        "\s" );
    array[item++] = new TestCase( SECTION, "\\u",    "u",        "\u" );

    array[item++] = new TestCase( SECTION, "\\w",    "w",        "\w" );
    array[item++] = new TestCase( SECTION, "\\x",    "x",        "\x" );
    array[item++] = new TestCase( SECTION, "\\y",    "y",        "\y" );
    array[item++] = new TestCase( SECTION, "\\z",    "z",        "\z" );
    array[item++] = new TestCase( SECTION, "\\9",    "9",        "\9" );

    array[item++] = new TestCase( SECTION, "\\A",    "A",        "\A" );
    array[item++] = new TestCase( SECTION, "\\B",    "B",        "\B" );
    array[item++] = new TestCase( SECTION, "\\C",    "C",        "\C" );
    array[item++] = new TestCase( SECTION, "\\D",    "D",        "\D" );
    array[item++] = new TestCase( SECTION, "\\E",    "E",        "\E" );
    array[item++] = new TestCase( SECTION, "\\F",    "F",        "\F" );
    array[item++] = new TestCase( SECTION, "\\G",    "G",        "\G" );
    array[item++] = new TestCase( SECTION, "\\H",    "H",        "\H" );
    array[item++] = new TestCase( SECTION, "\\I",    "I",        "\I" );
    array[item++] = new TestCase( SECTION, "\\J",    "J",        "\J" );
    array[item++] = new TestCase( SECTION, "\\K",    "K",        "\K" );
    array[item++] = new TestCase( SECTION, "\\L",    "L",        "\L" );
    array[item++] = new TestCase( SECTION, "\\M",    "M",        "\M" );
    array[item++] = new TestCase( SECTION, "\\N",    "N",        "\N" );
    array[item++] = new TestCase( SECTION, "\\O",    "O",        "\O" );
    array[item++] = new TestCase( SECTION, "\\P",    "P",        "\P" );
    array[item++] = new TestCase( SECTION, "\\Q",    "Q",        "\Q" );
    array[item++] = new TestCase( SECTION, "\\R",    "R",        "\R" );
    array[item++] = new TestCase( SECTION, "\\S",    "S",        "\S" );
    array[item++] = new TestCase( SECTION, "\\T",    "T",        "\T" );
    array[item++] = new TestCase( SECTION, "\\U",    "U",        "\U" );
    array[item++] = new TestCase( SECTION, "\\V",    "V",        "\V" );
    array[item++] = new TestCase( SECTION, "\\W",    "W",        "\W" );
    array[item++] = new TestCase( SECTION, "\\X",    "X",        "\X" );
    array[item++] = new TestCase( SECTION, "\\Y",    "Y",        "\Y" );
    array[item++] = new TestCase( SECTION, "\\Z",    "Z",        "\Z" );

    // DoubleStringCharacter::EscapeSequence::UnicodeEscapeSequence

    array[item++] = new TestCase( SECTION,  "\\u0020",  " ",        "\u0020" );
    array[item++] = new TestCase( SECTION,  "\\u0021",  "!",        "\u0021" );
    array[item++] = new TestCase( SECTION,  "\\u0022",  "\"",       "\u0022" );
    array[item++] = new TestCase( SECTION,  "\\u0023",  "#",        "\u0023" );
    array[item++] = new TestCase( SECTION,  "\\u0024",  "$",        "\u0024" );
    array[item++] = new TestCase( SECTION,  "\\u0025",  "%",        "\u0025" );
    array[item++] = new TestCase( SECTION,  "\\u0026",  "&",        "\u0026" );
    array[item++] = new TestCase( SECTION,  "\\u0027",  "'",        "\u0027" );
    array[item++] = new TestCase( SECTION,  "\\u0028",  "(",        "\u0028" );
    array[item++] = new TestCase( SECTION,  "\\u0029",  ")",        "\u0029" );
    array[item++] = new TestCase( SECTION,  "\\u002A",  "*",        "\u002A" );
    array[item++] = new TestCase( SECTION,  "\\u002B",  "+",        "\u002B" );
    array[item++] = new TestCase( SECTION,  "\\u002C",  ",",        "\u002C" );
    array[item++] = new TestCase( SECTION,  "\\u002D",  "-",        "\u002D" );
    array[item++] = new TestCase( SECTION,  "\\u002E",  ".",        "\u002E" );
    array[item++] = new TestCase( SECTION,  "\\u002F",  "/",        "\u002F" );
    array[item++] = new TestCase( SECTION,  "\\u0030",  "0",        "\u0030" );
    array[item++] = new TestCase( SECTION,  "\\u0031",  "1",        "\u0031" );
    array[item++] = new TestCase( SECTION,  "\\u0032",  "2",        "\u0032" );
    array[item++] = new TestCase( SECTION,  "\\u0033",  "3",        "\u0033" );
    array[item++] = new TestCase( SECTION,  "\\u0034",  "4",        "\u0034" );
    array[item++] = new TestCase( SECTION,  "\\u0035",  "5",        "\u0035" );
    array[item++] = new TestCase( SECTION,  "\\u0036",  "6",        "\u0036" );
    array[item++] = new TestCase( SECTION,  "\\u0037",  "7",        "\u0037" );
    array[item++] = new TestCase( SECTION,  "\\u0038",  "8",        "\u0038" );
    array[item++] = new TestCase( SECTION,  "\\u0039",  "9",        "\u0039" );


    return ( array );
}
