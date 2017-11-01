/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          7.7.4.js
   ECMA Section:       7.7.4 String Literals

   Description:        A string literal is zero or more characters enclosed in
   single or double quotes.  Each character may be
   represented by an escape sequence.


   Author:             christine@netscape.com
   Date:               16 september 1997
*/

var SECTION = "7.7.4";
var TITLE   = "String Literals";

writeHeaderToLog( SECTION + " "+ TITLE);

// StringLiteral:: "" and ''

new TestCase( "\"\"",     "",     "" );
new TestCase( "\'\'",     "",      '' );

// DoubleStringCharacters:: DoubleStringCharacter :: EscapeSequence :: CharacterEscapeSequence
new TestCase( "\\\"",        String.fromCharCode(0x0022),     "\"" );
new TestCase( "\\\'",        String.fromCharCode(0x0027),     "\'" );
new TestCase( "\\",         String.fromCharCode(0x005C),     "\\" );
new TestCase( "\\b",        String.fromCharCode(0x0008),     "\b" );
new TestCase( "\\f",        String.fromCharCode(0x000C),     "\f" );
new TestCase( "\\n",        String.fromCharCode(0x000A),     "\n" );
new TestCase( "\\r",        String.fromCharCode(0x000D),     "\r" );
new TestCase( "\\t",        String.fromCharCode(0x0009),     "\t" );
new TestCase( "\\v",        String.fromCharCode(0x000B),        "\v" );

// DoubleStringCharacters:DoubleStringCharacter::EscapeSequence::OctalEscapeSequence

new TestCase( "\\00",      String.fromCharCode(0x0000),    "\00" );
new TestCase( "\\01",      String.fromCharCode(0x0001),    "\01" );
new TestCase( "\\02",      String.fromCharCode(0x0002),    "\02" );
new TestCase( "\\03",      String.fromCharCode(0x0003),    "\03" );
new TestCase( "\\04",      String.fromCharCode(0x0004),    "\04" );
new TestCase( "\\05",      String.fromCharCode(0x0005),    "\05" );
new TestCase( "\\06",      String.fromCharCode(0x0006),    "\06" );
new TestCase( "\\07",      String.fromCharCode(0x0007),    "\07" );

new TestCase( "\\010",      String.fromCharCode(0x0008),    "\010" );
new TestCase( "\\011",      String.fromCharCode(0x0009),    "\011" );
new TestCase( "\\012",      String.fromCharCode(0x000A),    "\012" );
new TestCase( "\\013",      String.fromCharCode(0x000B),    "\013" );
new TestCase( "\\014",      String.fromCharCode(0x000C),    "\014" );
new TestCase( "\\015",      String.fromCharCode(0x000D),    "\015" );
new TestCase( "\\016",      String.fromCharCode(0x000E),    "\016" );
new TestCase( "\\017",      String.fromCharCode(0x000F),    "\017" );
new TestCase( "\\020",      String.fromCharCode(0x0010),    "\020" );
new TestCase( "\\042",      String.fromCharCode(0x0022),    "\042" );

new TestCase( "\\0",      String.fromCharCode(0x0000),    "\0" );
new TestCase( "\\1",      String.fromCharCode(0x0001),    "\1" );
new TestCase( "\\2",      String.fromCharCode(0x0002),    "\2" );
new TestCase( "\\3",      String.fromCharCode(0x0003),    "\3" );
new TestCase( "\\4",      String.fromCharCode(0x0004),    "\4" );
new TestCase( "\\5",      String.fromCharCode(0x0005),    "\5" );
new TestCase( "\\6",      String.fromCharCode(0x0006),    "\6" );
new TestCase( "\\7",      String.fromCharCode(0x0007),    "\7" );

new TestCase( "\\10",      String.fromCharCode(0x0008),    "\10" );
new TestCase( "\\11",      String.fromCharCode(0x0009),    "\11" );
new TestCase( "\\12",      String.fromCharCode(0x000A),    "\12" );
new TestCase( "\\13",      String.fromCharCode(0x000B),    "\13" );
new TestCase( "\\14",      String.fromCharCode(0x000C),    "\14" );
new TestCase( "\\15",      String.fromCharCode(0x000D),    "\15" );
new TestCase( "\\16",      String.fromCharCode(0x000E),    "\16" );
new TestCase( "\\17",      String.fromCharCode(0x000F),    "\17" );
new TestCase( "\\20",      String.fromCharCode(0x0010),    "\20" );
new TestCase( "\\42",      String.fromCharCode(0x0022),    "\42" );

new TestCase( "\\000",      String.fromCharCode(0),        "\000" );
new TestCase( "\\111",      String.fromCharCode(73),       "\111" );
new TestCase( "\\222",      String.fromCharCode(146),      "\222" );
new TestCase( "\\333",      String.fromCharCode(219),      "\333" );

//  following line commented out as it causes a compile time error
//    new TestCase( "\\444",      "444",                         "\444" );

// DoubleStringCharacters:DoubleStringCharacter::EscapeSequence::HexEscapeSequence
new TestCase( "\\xF0",      String.fromCharCode(240),         "\xF0" );
new TestCase( "\\xE1",      String.fromCharCode(225),         "\xE1" );
new TestCase( "\\xD2",      String.fromCharCode(210),         "\xD2" );
new TestCase( "\\xC3",      String.fromCharCode(195),         "\xC3" );
new TestCase( "\\xB4",      String.fromCharCode(180),         "\xB4" );
new TestCase( "\\xA5",      String.fromCharCode(165),         "\xA5" );
new TestCase( "\\x96",      String.fromCharCode(150),         "\x96" );
new TestCase( "\\x87",      String.fromCharCode(135),         "\x87" );
new TestCase( "\\x78",      String.fromCharCode(120),         "\x78" );
new TestCase( "\\x69",      String.fromCharCode(105),         "\x69" );
new TestCase( "\\x5A",      String.fromCharCode(90),         "\x5A" );
new TestCase( "\\x4B",      String.fromCharCode(75),         "\x4B" );
new TestCase( "\\x3C",      String.fromCharCode(60),         "\x3C" );
new TestCase( "\\x2D",      String.fromCharCode(45),         "\x2D" );
new TestCase( "\\x1E",      String.fromCharCode(30),         "\x1E" );
new TestCase( "\\x0F",      String.fromCharCode(15),         "\x0F" );

// string literals only take up to two hext digits.  therefore, the third character in this string
// should be interpreted as a StringCharacter and not part of the HextEscapeSequence

new TestCase( "\\xF0F",      String.fromCharCode(240)+"F",         "\xF0F" );
new TestCase( "\\xE1E",      String.fromCharCode(225)+"E",         "\xE1E" );
new TestCase( "\\xD2D",      String.fromCharCode(210)+"D",         "\xD2D" );
new TestCase( "\\xC3C",      String.fromCharCode(195)+"C",         "\xC3C" );
new TestCase( "\\xB4B",      String.fromCharCode(180)+"B",         "\xB4B" );
new TestCase( "\\xA5A",      String.fromCharCode(165)+"A",         "\xA5A" );
new TestCase( "\\x969",      String.fromCharCode(150)+"9",         "\x969" );
new TestCase( "\\x878",      String.fromCharCode(135)+"8",         "\x878" );
new TestCase( "\\x787",      String.fromCharCode(120)+"7",         "\x787" );
new TestCase( "\\x696",      String.fromCharCode(105)+"6",         "\x696" );
new TestCase( "\\x5A5",      String.fromCharCode(90)+"5",         "\x5A5" );
new TestCase( "\\x4B4",      String.fromCharCode(75)+"4",         "\x4B4" );
new TestCase( "\\x3C3",      String.fromCharCode(60)+"3",         "\x3C3" );
new TestCase( "\\x2D2",      String.fromCharCode(45)+"2",         "\x2D2" );
new TestCase( "\\x1E1",      String.fromCharCode(30)+"1",         "\x1E1" );
new TestCase( "\\x0F0",      String.fromCharCode(15)+"0",         "\x0F0" );

// DoubleStringCharacter::EscapeSequence::CharacterEscapeSequence::\ NonEscapeCharacter
new TestCase( "\\a",    "a",        "\a" );
new TestCase( "\\c",    "c",        "\c" );
new TestCase( "\\d",    "d",        "\d" );
new TestCase( "\\e",    "e",        "\e" );
new TestCase( "\\g",    "g",        "\g" );
new TestCase( "\\h",    "h",        "\h" );
new TestCase( "\\i",    "i",        "\i" );
new TestCase( "\\j",    "j",        "\j" );
new TestCase( "\\k",    "k",        "\k" );
new TestCase( "\\l",    "l",        "\l" );
new TestCase( "\\m",    "m",        "\m" );
new TestCase( "\\o",    "o",        "\o" );
new TestCase( "\\p",    "p",        "\p" );
new TestCase( "\\q",    "q",        "\q" );
new TestCase( "\\s",    "s",        "\s" );
new TestCase( "\\w",    "w",        "\w" );
new TestCase( "\\y",    "y",        "\y" );
new TestCase( "\\z",    "z",        "\z" );
new TestCase( "\\9",    "9",        "\9" );

new TestCase( "\\A",    "A",        "\A" );
new TestCase( "\\B",    "B",        "\B" );
new TestCase( "\\C",    "C",        "\C" );
new TestCase( "\\D",    "D",        "\D" );
new TestCase( "\\E",    "E",        "\E" );
new TestCase( "\\F",    "F",        "\F" );
new TestCase( "\\G",    "G",        "\G" );
new TestCase( "\\H",    "H",        "\H" );
new TestCase( "\\I",    "I",        "\I" );
new TestCase( "\\J",    "J",        "\J" );
new TestCase( "\\K",    "K",        "\K" );
new TestCase( "\\L",    "L",        "\L" );
new TestCase( "\\M",    "M",        "\M" );
new TestCase( "\\N",    "N",        "\N" );
new TestCase( "\\O",    "O",        "\O" );
new TestCase( "\\P",    "P",        "\P" );
new TestCase( "\\Q",    "Q",        "\Q" );
new TestCase( "\\R",    "R",        "\R" );
new TestCase( "\\S",    "S",        "\S" );
new TestCase( "\\T",    "T",        "\T" );
new TestCase( "\\U",    "U",        "\U" );
new TestCase( "\\V",    "V",        "\V" );
new TestCase( "\\W",    "W",        "\W" );
new TestCase( "\\X",    "X",        "\X" );
new TestCase( "\\Y",    "Y",        "\Y" );
new TestCase( "\\Z",    "Z",        "\Z" );

// DoubleStringCharacter::EscapeSequence::UnicodeEscapeSequence

new TestCase( "\\u0020",  " ",        "\u0020" );
new TestCase( "\\u0021",  "!",        "\u0021" );
new TestCase( "\\u0022",  "\"",       "\u0022" );
new TestCase( "\\u0023",  "#",        "\u0023" );
new TestCase( "\\u0024",  "$",        "\u0024" );
new TestCase( "\\u0025",  "%",        "\u0025" );
new TestCase( "\\u0026",  "&",        "\u0026" );
new TestCase( "\\u0027",  "'",        "\u0027" );
new TestCase( "\\u0028",  "(",        "\u0028" );
new TestCase( "\\u0029",  ")",        "\u0029" );
new TestCase( "\\u002A",  "*",        "\u002A" );
new TestCase( "\\u002B",  "+",        "\u002B" );
new TestCase( "\\u002C",  ",",        "\u002C" );
new TestCase( "\\u002D",  "-",        "\u002D" );
new TestCase( "\\u002E",  ".",        "\u002E" );
new TestCase( "\\u002F",  "/",        "\u002F" );
new TestCase( "\\u0030",  "0",        "\u0030" );
new TestCase( "\\u0031",  "1",        "\u0031" );
new TestCase( "\\u0032",  "2",        "\u0032" );
new TestCase( "\\u0033",  "3",        "\u0033" );
new TestCase( "\\u0034",  "4",        "\u0034" );
new TestCase( "\\u0035",  "5",        "\u0035" );
new TestCase( "\\u0036",  "6",        "\u0036" );
new TestCase( "\\u0037",  "7",        "\u0037" );
new TestCase( "\\u0038",  "8",        "\u0038" );
new TestCase( "\\u0039",  "9",        "\u0039" );

test();
