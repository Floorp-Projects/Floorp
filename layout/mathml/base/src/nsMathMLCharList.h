/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla MathML Project.
 *
 * The Initial Developer of the Original Code is The University Of
 * Queensland.  Portions created by The University Of Queensland are
 * Copyright (C) 1999 The University Of Queensland.  All Rights Reserved.
 *
 * Contributor(s):
 *   Roger B. Sidje <rbs@maths.uq.edu.au>
 */

/******

  This file contains the list of all stretchy MathML operators.

  It is designed to be used as inline input to nsMathMLChar.cpp *only*
  through the magic of C preprocessing.

  All entries must be enclosed in the macro MATHML_CHAR which will have cruel
  and unusual things done to them.

  To add support for a particular stretchy char, you should move its
  MATHML_CHAR entry out of the #ifdef list and append it in the active list.

  The first argument of MATHML_CHAR will be converted in an enum of
  the form, e.g., eMathMLChar_LeftParenthesis, which will be used in
  the code.  Therefore the ORDER OF THE ACTIVE LIST IS SIGNIFICANT. 
  (The order in which chars are gradually appended in the active list is 
  not-significant, but once added in the active list, they should remain
  in their positions. If you want to shuffle the active list, you should
  also make sure to adjust the related data structures in nsMathMLChar.cpp)
 
 ******/

// short names for a short while...
#define STRETCH_UNSUPPORTED NS_STRETCH_DIRECTION_UNSUPPORTED
#define STRETCH_HORIZONTAL  NS_STRETCH_DIRECTION_HORIZONTAL
#define STRETCH_VERTICAL    NS_STRETCH_DIRECTION_VERTICAL

/* active list - these are chars that we know something about */
MATHML_CHAR(LeftParenthesis,                  '('  , STRETCH_VERTICAL)
MATHML_CHAR(RightParenthesis,                 ')'  , STRETCH_VERTICAL)
MATHML_CHAR(Integral,                        0x222B, STRETCH_VERTICAL)
MATHML_CHAR(LeftSquareBracket,                '['  , STRETCH_VERTICAL)
MATHML_CHAR(RightSquareBracket,               ']'  , STRETCH_VERTICAL)
MATHML_CHAR(LeftCurlyBracket,                 '{'  , STRETCH_VERTICAL)
MATHML_CHAR(RightCurlyBracket,                '}'  , STRETCH_VERTICAL)
MATHML_CHAR(DownArrow,                       0x2193, STRETCH_VERTICAL)
MATHML_CHAR(UpArrow,                         0x2191, STRETCH_VERTICAL)
MATHML_CHAR(LeftArrow,                       0x2190, STRETCH_HORIZONTAL)
MATHML_CHAR(RightArrow,                      0x2192, STRETCH_HORIZONTAL)
MATHML_CHAR(OverBar,                         0x00AF, STRETCH_HORIZONTAL)
MATHML_CHAR(VerticalBar,                      '|'  , STRETCH_VERTICAL)
MATHML_CHAR(Sqrt,                            0x221A, STRETCH_UNSUPPORTED)

#if 0
MATHML_CHAR(LeftAngleBracket,                0x3008, STRETCH_UNSUPPORTED)
MATHML_CHAR(LeftBracketingBar,               0xF603, STRETCH_UNSUPPORTED)
MATHML_CHAR(LeftCeiling,                     0x2308, STRETCH_UNSUPPORTED)
MATHML_CHAR(LeftDoubleBracket,               0x301A, STRETCH_UNSUPPORTED)
MATHML_CHAR(LeftDoubleBracketingBar,         0xF605, STRETCH_UNSUPPORTED)
MATHML_CHAR(LeftFloor,                       0x230A, STRETCH_UNSUPPORTED)
MATHML_CHAR(RightAngleBracket,               0x3009, STRETCH_UNSUPPORTED)
MATHML_CHAR(RightBracketingBar,              0xF604, STRETCH_UNSUPPORTED)
MATHML_CHAR(RightCeiling,                    0x2309, STRETCH_UNSUPPORTED)
MATHML_CHAR(RightDoubleBracket,              0x301B, STRETCH_UNSUPPORTED)
MATHML_CHAR(RightDoubleBracketingBar,        0xF606, STRETCH_UNSUPPORTED)
MATHML_CHAR(RightFloor,                      0x230B, STRETCH_UNSUPPORTED)
MATHML_CHAR(HorizontalLine,                  0xE859, STRETCH_UNSUPPORTED)
MATHML_CHAR(VerticalLine,                    0xE85A, STRETCH_UNSUPPORTED)
MATHML_CHAR(VerticalSeparator,               0xE85C, STRETCH_UNSUPPORTED)
MATHML_CHAR(Implies,                         0x21D2, STRETCH_UNSUPPORTED)
MATHML_CHAR(Or,                              0xE375, STRETCH_UNSUPPORTED)
MATHML_CHAR(And,                             0xE374, STRETCH_UNSUPPORTED)
MATHML_CHAR(DoubleLeftArrow,                 0x21D0, STRETCH_UNSUPPORTED)
MATHML_CHAR(DoubleLeftRightArrow,            0x21D4, STRETCH_UNSUPPORTED)
MATHML_CHAR(DoubleRightArrow,                0x21D2, STRETCH_UNSUPPORTED)
MATHML_CHAR(DownLeftRightVector,             0xF50B, STRETCH_UNSUPPORTED)
MATHML_CHAR(DownLeftTeeVector,               0xF50E, STRETCH_UNSUPPORTED)
MATHML_CHAR(DownLeftVector,                  0x21BD, STRETCH_UNSUPPORTED)
MATHML_CHAR(DownLeftVectorBar,               0xF50C, STRETCH_UNSUPPORTED)
MATHML_CHAR(DownRightTeeVector,              0xF50F, STRETCH_UNSUPPORTED)
MATHML_CHAR(DownRightVector,                 0x21C1, STRETCH_UNSUPPORTED)
MATHML_CHAR(DownRightVectorBar,              0xF50D, STRETCH_UNSUPPORTED)
MATHML_CHAR(LeftArrowBar,                    0x21E4, STRETCH_UNSUPPORTED)
MATHML_CHAR(LeftArrowRightArrow,             0x21C6, STRETCH_UNSUPPORTED)
MATHML_CHAR(LeftRightArrow,                  0x2194, STRETCH_UNSUPPORTED)
MATHML_CHAR(LeftRightVector,                 0xF505, STRETCH_UNSUPPORTED)
MATHML_CHAR(LeftTeeArrow,                    0x21A4, STRETCH_UNSUPPORTED)
MATHML_CHAR(LeftTeeVector,                   0xF509, STRETCH_UNSUPPORTED)
MATHML_CHAR(LeftVector,                      0x21BC, STRETCH_UNSUPPORTED)
MATHML_CHAR(LeftVectorBar,                   0xF507, STRETCH_UNSUPPORTED)
MATHML_CHAR(LowerLeftArrow,                  0x2199, STRETCH_UNSUPPORTED)
MATHML_CHAR(LowerRightArrow,                 0x2198, STRETCH_UNSUPPORTED)
MATHML_CHAR(RightArrowBar,                   0x21E5, STRETCH_UNSUPPORTED)
MATHML_CHAR(RightArrowLeftArrow,             0x21C4, STRETCH_UNSUPPORTED)
MATHML_CHAR(RightTeeArrow,                   0x21A6, STRETCH_UNSUPPORTED)
MATHML_CHAR(RightTeeVector,                  0xF50A, STRETCH_UNSUPPORTED)
MATHML_CHAR(RightVector,                     0x21C0, STRETCH_UNSUPPORTED)
MATHML_CHAR(RightVectorBar,                  0xF508, STRETCH_UNSUPPORTED)
MATHML_CHAR(UpperLeftArrow,                  0x2196, STRETCH_UNSUPPORTED)
MATHML_CHAR(UpperRightArrow,                 0x2197, STRETCH_UNSUPPORTED)
MATHML_CHAR(Equilibrium,                     0x21CC, STRETCH_UNSUPPORTED)
MATHML_CHAR(ReverseEquilibrium,              0x21CB, STRETCH_UNSUPPORTED)
MATHML_CHAR(SquareUnion,                     0x2294, STRETCH_UNSUPPORTED)
MATHML_CHAR(Union,                           0x22C3, STRETCH_UNSUPPORTED)
MATHML_CHAR(UnionPlus,                       0x228E, STRETCH_UNSUPPORTED)
MATHML_CHAR(Intersection,                    0x22C2, STRETCH_UNSUPPORTED)
MATHML_CHAR(SquareIntersection,              0x2293, STRETCH_UNSUPPORTED)
MATHML_CHAR(Vee,                             0x22C1, STRETCH_UNSUPPORTED)
MATHML_CHAR(Sum,                             0x2211, STRETCH_UNSUPPORTED)
MATHML_CHAR(Union,                           0x22C3, STRETCH_UNSUPPORTED)
MATHML_CHAR(UnionPlus,                       0x228E, STRETCH_UNSUPPORTED)
MATHML_CHAR(ClockwiseContourIntegral,        0x2232, STRETCH_UNSUPPORTED)
MATHML_CHAR(ContourIntegral,                 0x222E, STRETCH_UNSUPPORTED)
MATHML_CHAR(CounterClockwiseContourIntegral, 0x2233, STRETCH_UNSUPPORTED)
MATHML_CHAR(DoubleContourIntegral,           0x222F, STRETCH_UNSUPPORTED)
MATHML_CHAR(Wedge,                           0x22C0, STRETCH_UNSUPPORTED)
MATHML_CHAR(Coproduct,                       0x2210, STRETCH_UNSUPPORTED)
MATHML_CHAR(Product,                         0x220F, STRETCH_UNSUPPORTED)
MATHML_CHAR(Intersection,                    0x22C2, STRETCH_UNSUPPORTED)
MATHML_CHAR(Backslash,                       0x2216, STRETCH_UNSUPPORTED)
MATHML_CHAR(Divide,                           '/'  , STRETCH_UNSUPPORTED)
MATHML_CHAR(DoubleDownArrow,                 0x21D3, STRETCH_UNSUPPORTED)
MATHML_CHAR(DoubleLongLeftArrow,             0xE200, STRETCH_UNSUPPORTED)
MATHML_CHAR(DoubleLongLeftRightArrow,        0xE202, STRETCH_UNSUPPORTED)
MATHML_CHAR(DoubleLongRightArrow,            0xE204, STRETCH_UNSUPPORTED)
MATHML_CHAR(DoubleUpArrow,                   0x21D1, STRETCH_UNSUPPORTED)
MATHML_CHAR(DoubleUpDownArrow,               0x21D5, STRETCH_UNSUPPORTED)
MATHML_CHAR(DownArrowBar,                    0xF504, STRETCH_UNSUPPORTED)
MATHML_CHAR(DownArrowUpArrow,                0xE216, STRETCH_UNSUPPORTED)
MATHML_CHAR(DownTeeArrow,                    0x21A7, STRETCH_UNSUPPORTED)
MATHML_CHAR(LeftDownTeeVector,               0xF519, STRETCH_UNSUPPORTED)
MATHML_CHAR(LeftDownVector,                  0x21C3, STRETCH_UNSUPPORTED)
MATHML_CHAR(LeftDownVectorBar,               0xF517, STRETCH_UNSUPPORTED)
MATHML_CHAR(LeftUpDownVector,                0xF515, STRETCH_UNSUPPORTED)
MATHML_CHAR(LeftUpTeeVector,                 0xF518, STRETCH_UNSUPPORTED)
MATHML_CHAR(LeftUpVector,                    0x21BF, STRETCH_UNSUPPORTED)
MATHML_CHAR(LeftUpVectorBar,                 0xF516, STRETCH_UNSUPPORTED)
MATHML_CHAR(LongLeftArrow,                   0xE201, STRETCH_UNSUPPORTED)
MATHML_CHAR(LongLeftRightArrow,              0xE203, STRETCH_UNSUPPORTED)
MATHML_CHAR(LongRightArrow,                  0xE205, STRETCH_UNSUPPORTED)
MATHML_CHAR(ReverseUpEquilibrium,            0xE217, STRETCH_UNSUPPORTED)
MATHML_CHAR(RightDownTeeVector,              0xF514, STRETCH_UNSUPPORTED)
MATHML_CHAR(RightDownVector,                 0x21C2, STRETCH_UNSUPPORTED)
MATHML_CHAR(RightDownVectorBar,              0xF512, STRETCH_UNSUPPORTED)
MATHML_CHAR(RightUpDownVector,               0xF510, STRETCH_UNSUPPORTED)
MATHML_CHAR(RightUpTeeVector,                0xF513, STRETCH_UNSUPPORTED)
MATHML_CHAR(RightUpVector,                   0x21BE, STRETCH_UNSUPPORTED)
MATHML_CHAR(RightUpVectorBar,                0xF511, STRETCH_UNSUPPORTED)
MATHML_CHAR(UpArrowBar,                      0xF503, STRETCH_UNSUPPORTED)
MATHML_CHAR(UpArrowDownArrow,                0x21C5, STRETCH_UNSUPPORTED)
MATHML_CHAR(UpDownArrow,                     0x2195, STRETCH_UNSUPPORTED)
MATHML_CHAR(UpEquilibrium,                   0xE218, STRETCH_UNSUPPORTED)
MATHML_CHAR(UpTeeArrow,                      0x21A5, STRETCH_UNSUPPORTED)
MATHML_CHAR(DiacriticalTilde,                0x02DC, STRETCH_UNSUPPORTED)
MATHML_CHAR(Hacek,                           0x02C7, STRETCH_UNSUPPORTED)
MATHML_CHAR(Hat,                             0x0302, STRETCH_UNSUPPORTED)
MATHML_CHAR(OverBrace,                       0xF612, STRETCH_UNSUPPORTED)
MATHML_CHAR(OverBracket,                     0xF614, STRETCH_UNSUPPORTED)
MATHML_CHAR(OverParenthesis,                 0xF610, STRETCH_UNSUPPORTED)
MATHML_CHAR(UnderBar,                        0x0332, STRETCH_UNSUPPORTED)
MATHML_CHAR(UnderBrace,                      0xF613, STRETCH_UNSUPPORTED)
MATHML_CHAR(UnderBracket,                    0xF615, STRETCH_UNSUPPORTED)
MATHML_CHAR(UnderParenthesis,                0xF611, STRETCH_UNSUPPORTED)
#endif


#undef STRETCH_UNSUPPORTED
#undef STRETCH_HORIZONTAL
#undef STRETCH_VERTICAL
