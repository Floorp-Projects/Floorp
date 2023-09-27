/* Copyright 2021 Mozilla Foundation
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

// ./test/core/names.wast

// ./test/core/names.wast:4
let $0 = instantiate(`(module
  (func (export "foo") (result i32) (i32.const 0))
)`);

// ./test/core/names.wast:8
assert_return(() => invoke($0, `foo`, []), [value("i32", 0)]);

// ./test/core/names.wast:12
let $1 = instantiate(`(module
  (func (export "foo") (result i32) (i32.const 1))
)`);

// ./test/core/names.wast:16
assert_return(() => invoke($1, `foo`, []), [value("i32", 1)]);

// ./test/core/names.wast:19
let $2 = instantiate(`(module
  ;; Test that we can use the empty string as a symbol.
  (func (export "") (result i32) (i32.const 0))

  ;; Test that we can use names beginning with a digit.
  (func (export "0") (result i32) (i32.const 1))

  ;; Test that we can use names beginning with a dash.
  (func (export "-0") (result i32) (i32.const 2))

  ;; Test that we can use names beginning with an underscore.
  (func (export "_") (result i32) (i32.const 3))

  ;; Test that we can use names beginning with a dollar sign.
  (func (export "$$") (result i32) (i32.const 4))

  ;; Test that we can use names beginning with an at sign.
  (func (export "@") (result i32) (i32.const 5))

  ;; Test that we can use non-alphanumeric names.
  (func (export "~!@#$$%^&*()_+\`-={}|[]\\\\:\\";'<>?,./ ") (result i32) (i32.const 6))

  ;; Test that we can use names that have special meaning in JS.
  (func (export "NaN") (result i32) (i32.const 7))
  (func (export "Infinity") (result i32) (i32.const 8))
  (func (export "if") (result i32) (i32.const 9))

  ;; Test that we can use common libc names without conflict.
  (func (export "malloc") (result i32) (i32.const 10))

  ;; Test that we can use some libc hidden names without conflict.
  (func (export "_malloc") (result i32) (i32.const 11))
  (func (export "__malloc") (result i32) (i32.const 12))

  ;; Test that names are case-sensitive.
  (func (export "a") (result i32) (i32.const 13))
  (func (export "A") (result i32) (i32.const 14))

  ;; Test that UTF-8 BOM code points can appear in identifiers.
  (func (export "\u{feff}") (result i32) (i32.const 15))

  ;; Test that Unicode normalization is not applied. These function names
  ;; contain different codepoints which normalize to the same thing under
  ;; NFC or NFD.
  (func (export "\u{c5}") (result i32) (i32.const 16))
  (func (export "A\u{30a}") (result i32) (i32.const 17))
  (func (export "\u{212b}") (result i32) (i32.const 18))

  ;; Test that Unicode compatibility normalization is not applied. These
  ;; function names contain different codepoints which normalize to the
  ;; same thing under NFKC or NFKD.
  (func (export "\u{fb03}") (result i32) (i32.const 19))
  (func (export "f\u{fb01}") (result i32) (i32.const 20))
  (func (export "ffi") (result i32) (i32.const 21))

  ;; Test the C0 control codes.
  (func (export "\\00\\01\\02\\03\\04\\05\\06\\07\\08\\09\\0a\\0b\\0c\\0d\\0e\\0f") (result i32) (i32.const 22))
  (func (export "\\10\\11\\12\\13\\14\\15\\16\\17\\18\\19\\1a\\1b\\1c\\1d\\1e\\1f") (result i32) (i32.const 23))
  ;; Test miscellaneous control codes.
  (func (export " \\7f") (result i32) (i32.const 24))
  ;; Test the C1 control codes.
  (func (export "\\c2\\80\\c2\\81\\c2\\82\\c2\\83\\c2\\84\\c2\\85\\c2\\86\\c2\\87\\c2\\88\\c2\\89\\c2\\8a\\c2\\8b\\c2\\8c\\c2\\8d\\c2\\8e\\c2\\8f") (result i32) (i32.const 25))
  (func (export "\\c2\\90\\c2\\91\\c2\\92\\c2\\93\\c2\\94\\c2\\95\\c2\\96\\c2\\97\\c2\\98\\c2\\99\\c2\\9a\\c2\\9b\\c2\\9c\\c2\\9d\\c2\\9e\\c2\\9f") (result i32) (i32.const 26))
  ;; Test the Unicode Specials.
  (func (export "\\ef\\bf\\b0\\ef\\bf\\b1\\ef\\bf\\b2\\ef\\bf\\b3\\ef\\bf\\b4\\ef\\bf\\b5\\ef\\bf\\b6\\ef\\bf\\b7") (result i32) (i32.const 27))
  (func (export "\\ef\\bf\\b8\\ef\\bf\\b9\\ef\\bf\\ba\\ef\\bf\\bb\\ef\\bf\\bc\\ef\\bf\\bd\\ef\\bf\\be\\ef\\bf\\bf") (result i32) (i32.const 28))

  ;; Test that the control pictures are distinct from the control codes they
  ;; depict. These correspond to the C0 and miscellaneous control code tests
  ;; above.
  (func (export "\u{2400}\u{2401}\u{2402}\u{2403}\u{2404}\u{2405}\u{2406}\u{2407}\u{2408}\u{2409}\u{240a}\u{240b}\u{240c}\u{240d}\u{240e}\u{240f}") (result i32) (i32.const 29))
  (func (export "\u{2410}\u{2411}\u{2412}\u{2413}\u{2414}\u{2415}\u{2416}\u{2417}\u{2418}\u{2419}\u{241a}\u{241b}\u{241c}\u{241d}\u{241e}\u{241f}") (result i32) (i32.const 30))
  (func (export "\u{2420}\u{2421}") (result i32) (i32.const 31))

  ;; Test the Unicode Specials in non-escaped form (excluding U+FFFE and
  ;; U+FFFF, so that generic tools don't detect this file as non-UTF-8).
  (func (export "\u{fff0}\u{fff1}\u{fff2}\u{fff3}\u{fff4}\u{fff5}\u{fff6}\u{fff7}\u{fff8}\u{fff9}\u{fffa}\u{fffb}\u{fffc}\u{fffd}") (result i32) (i32.const 32))

  ;; Test a bare ZWJ code point.
  (func (export "\u{200d}") (result i32) (i32.const 33))
  ;; Test a bare ZWNJ code point.
  (func (export "\u{200c}") (result i32) (i32.const 34))

  ;; Test various bare joiner code points.
  (func (export "\u{34f}") (result i32) (i32.const 35))
  (func (export "\u{2060}") (result i32) (i32.const 36))
  (func (export "\u{2d7f}") (result i32) (i32.const 37))
  (func (export "\u{1107f}") (result i32) (i32.const 38))
  (func (export "\u{180e}") (result i32) (i32.const 39))

  ;; Test various interesting code points: reverse BOM, zero-width space,
  ;; no-break space, soft hyphen, word joiner, ogham space mark,
  ;; right-to-left override, left-to-right override.
  (func (export "\u{ffef}\u{200b}\u{a0}\u{ad}\u{2060}\u{1680}\u{202e}\u{202d}") (result i32) (i32.const 40))

  ;; Test more interesting code points: left-to-right mark, right-to-left mark,
  ;; non-breaking hyphen, line separator, paragraph separator,
  ;; left-to-right embedding, right-to-left embedding,
  ;; pop directional formatting, narrow no-break space, left-to-right isolate,
  ;; right-to-left isolate, first strong isolate, pop directional isolate.
  (func (export "\u{200e}\u{200f}\u{2011}\u{2028}\u{2029}\u{202a}\u{202b}\u{202c}\u{202f}\u{2066}\u{2067}\u{2068}\u{2069}") (result i32) (i32.const 41))

  ;; Test some deprecated code points: inhibit symmetric swapping,
  ;; activate symmetric swapping, inhibit arabic form shaping,
  ;; activate arabic form shaping, national digit shapes, nominal digit shapes.
  (func (export "\u{206a}\u{206b}\u{206c}\u{206d}\u{206e}\u{206f}") (result i32) (i32.const 42))

  ;; Test "invisible" operator code points.
  (func (export "\u{2061}\u{2062}\u{2063}\u{2064}") (result i32) (i32.const 43))

  ;; Test that code points outside the BMP are supported.
  (func (export "\u{10000}\u{dffff}\u{10ffff}") (result i32) (i32.const 44))

  ;; Test that WebAssembly implementations cope in the presence of Zalgo.
  (func (export "Z\u{30f}\u{346}\u{306}\u{35b}\u{34c}\u{334}\u{358}\u{35e}\u{347}\u{32b}\u{325}\u{32a}\u{353}\u{348}\u{354}\u{34e}\u{317}\u{31e}\u{33a}\u{32f}\u{331}\u{31e}\u{319}\u{331}\u{31c}\u{316}\u{320}a\u{357}\u{368}\u{30e}\u{304}\u{306}\u{357}\u{33f}\u{361}\u{35f}\u{340}\u{336}\u{341}\u{325}\u{330}\u{333}\u{32d}\u{359}\u{332}\u{331}\u{339}\u{31d}\u{34e}\u{33c}l\u{344}\u{34a}\u{31a}\u{357}\u{366}\u{344}\u{36b}\u{307}\u{341}\u{336}\u{337}\u{349}\u{329}\u{339}\u{32b}\u{31d}\u{356}\u{345}\u{319}\u{332}\u{33c}\u{347}\u{35a}\u{34d}\u{32e}\u{34e}\u{325}\u{345}\u{31e}g\u{343}\u{310}\u{305}\u{36e}\u{314}\u{310}\u{30e}\u{302}\u{30f}\u{33e}\u{34a}\u{30d}\u{34b}\u{34a}\u{367}\u{301}\u{306}\u{366}\u{35e}\u{336}\u{355}\u{354}\u{35a}\u{329}o\u{34b}\u{314}\u{350}\u{36a}\u{369}\u{321}\u{34f}\u{322}\u{327}\u{341}\u{32b}\u{319}\u{324}\u{32e}\u{356}\u{359}\u{353}\u{33a}\u{31c}\u{329}\u{33c}\u{318}\u{320}") (result i32) (i32.const 45))

  ;; Test Hangul filler code points.
  (func (export "\u{115f}\u{1160}\u{3164}\u{ffa0}") (result i32) (i32.const 46))

  ;; Test variation selectors (which are also ID_Continue code points).
  (func (export "\u{fe00}") (result i32) (i32.const 47))
  (func (export "\u{fe04}") (result i32) (i32.const 48))
  (func (export "\u{e0100}") (result i32) (i32.const 49))
  (func (export "\u{e01ef}") (result i32) (i32.const 50))

  ;; Test an uncombined combining code point.
  (func (export "\u{308}") (result i32) (i32.const 51))

  ;; Test that numerous different present and historical representations of the
  ;; "newline" concept are distinct. Tests largely inspired by:
  ;;   https://en.wikipedia.org/wiki/Newline#Representations
  ;;   https://en.wikipedia.org/wiki/Newline#Unicode and
  ;;   https://en.wikipedia.org/wiki/Newline#Reverse_and_partial_line_feeds
  (func (export "\\0a") (result i32) (i32.const 52))
  (func (export "\u{2424}") (result i32) (i32.const 53))
  (func (export "\u{2028}") (result i32) (i32.const 54))
  (func (export "\\0d") (result i32) (i32.const 55))
  (func (export "\\0d\\0a") (result i32) (i32.const 56))
  (func (export "\\0a\\0d") (result i32) (i32.const 57))
  (func (export "\\1e") (result i32) (i32.const 58))
  (func (export "\\0b") (result i32) (i32.const 59))
  (func (export "\\0c") (result i32) (i32.const 60))
  (func (export "\\c2\\85") (result i32) (i32.const 61))
  (func (export "\u{2029}") (result i32) (i32.const 62))
  (func (export "\u{2026}") (result i32) (i32.const 63))
  (func (export "\u{23ce}") (result i32) (i32.const 64))
  (func (export "\\c2\\8b") (result i32) (i32.const 65))
  (func (export "\\c2\\8c") (result i32) (i32.const 66))
  (func (export "\\c2\\8d") (result i32) (i32.const 67))
  (func (export "\u{21b5}") (result i32) (i32.const 68))
  (func (export "\u{21a9}") (result i32) (i32.const 69))
  (func (export "\u{2324}") (result i32) (i32.const 70))
  (func (export "\u{2936}") (result i32) (i32.const 71))
  (func (export "\u{21b2}") (result i32) (i32.const 72))
  (func (export "\u{2ba8}") (result i32) (i32.const 73))
  (func (export "\u{2bb0}") (result i32) (i32.const 74))

  ;; Test that non-characters are not replaced by the replacement character.
  (func (export "\u{fffd}") (result i32) (i32.const 75))
  (func (export "\\ef\\b7\\90") (result i32) (i32.const 76))
  (func (export "\\ef\\b7\\91") (result i32) (i32.const 77))
  (func (export "\\ef\\b7\\92") (result i32) (i32.const 78))
  (func (export "\\ef\\b7\\93") (result i32) (i32.const 79))
  (func (export "\\ef\\b7\\94") (result i32) (i32.const 80))
  (func (export "\\ef\\b7\\95") (result i32) (i32.const 81))
  (func (export "\\ef\\b7\\96") (result i32) (i32.const 82))
  (func (export "\\ef\\b7\\97") (result i32) (i32.const 83))
  (func (export "\\ef\\b7\\98") (result i32) (i32.const 84))
  (func (export "\\ef\\b7\\99") (result i32) (i32.const 85))
  (func (export "\\ef\\b7\\9a") (result i32) (i32.const 86))
  (func (export "\\ef\\b7\\9b") (result i32) (i32.const 87))
  (func (export "\\ef\\b7\\9c") (result i32) (i32.const 88))
  (func (export "\\ef\\b7\\9d") (result i32) (i32.const 89))
  (func (export "\\ef\\b7\\9e") (result i32) (i32.const 90))
  (func (export "\\ef\\b7\\9f") (result i32) (i32.const 91))
  (func (export "\\ef\\b7\\a0") (result i32) (i32.const 92))
  (func (export "\\ef\\b7\\a1") (result i32) (i32.const 93))
  (func (export "\\ef\\b7\\a2") (result i32) (i32.const 94))
  (func (export "\\ef\\b7\\a3") (result i32) (i32.const 95))
  (func (export "\\ef\\b7\\a4") (result i32) (i32.const 96))
  (func (export "\\ef\\b7\\a5") (result i32) (i32.const 97))
  (func (export "\\ef\\b7\\a6") (result i32) (i32.const 98))
  (func (export "\\ef\\b7\\a7") (result i32) (i32.const 99))
  (func (export "\\ef\\b7\\a8") (result i32) (i32.const 100))
  (func (export "\\ef\\b7\\a9") (result i32) (i32.const 101))
  (func (export "\\ef\\b7\\aa") (result i32) (i32.const 102))
  (func (export "\\ef\\b7\\ab") (result i32) (i32.const 103))
  (func (export "\\ef\\b7\\ac") (result i32) (i32.const 104))
  (func (export "\\ef\\b7\\ad") (result i32) (i32.const 105))
  (func (export "\\ef\\b7\\ae") (result i32) (i32.const 106))
  (func (export "\\ef\\b7\\af") (result i32) (i32.const 107))
  (func (export "\\ef\\bf\\be") (result i32) (i32.const 108))
  (func (export "\\ef\\bf\\bf") (result i32) (i32.const 109))
  (func (export "\\f0\\9f\\bf\\be") (result i32) (i32.const 110))
  (func (export "\\f0\\9f\\bf\\bf") (result i32) (i32.const 111))
  (func (export "\\f0\\af\\bf\\be") (result i32) (i32.const 112))
  (func (export "\\f0\\af\\bf\\bf") (result i32) (i32.const 113))
  (func (export "\\f0\\bf\\bf\\be") (result i32) (i32.const 114))
  (func (export "\\f0\\bf\\bf\\bf") (result i32) (i32.const 115))
  (func (export "\\f1\\8f\\bf\\be") (result i32) (i32.const 116))
  (func (export "\\f1\\8f\\bf\\bf") (result i32) (i32.const 117))
  (func (export "\\f1\\9f\\bf\\be") (result i32) (i32.const 118))
  (func (export "\\f1\\9f\\bf\\bf") (result i32) (i32.const 119))
  (func (export "\\f1\\af\\bf\\be") (result i32) (i32.const 120))
  (func (export "\\f1\\af\\bf\\bf") (result i32) (i32.const 121))
  (func (export "\\f1\\bf\\bf\\be") (result i32) (i32.const 122))
  (func (export "\\f1\\bf\\bf\\bf") (result i32) (i32.const 123))
  (func (export "\\f2\\8f\\bf\\be") (result i32) (i32.const 124))
  (func (export "\\f2\\8f\\bf\\bf") (result i32) (i32.const 125))
  (func (export "\\f2\\9f\\bf\\be") (result i32) (i32.const 126))
  (func (export "\\f2\\9f\\bf\\bf") (result i32) (i32.const 127))
  (func (export "\\f2\\af\\bf\\be") (result i32) (i32.const 128))
  (func (export "\\f2\\af\\bf\\bf") (result i32) (i32.const 129))
  (func (export "\\f2\\bf\\bf\\be") (result i32) (i32.const 130))
  (func (export "\\f2\\bf\\bf\\bf") (result i32) (i32.const 131))
  (func (export "\\f3\\8f\\bf\\be") (result i32) (i32.const 132))
  (func (export "\\f3\\8f\\bf\\bf") (result i32) (i32.const 133))
  (func (export "\\f3\\9f\\bf\\be") (result i32) (i32.const 134))
  (func (export "\\f3\\9f\\bf\\bf") (result i32) (i32.const 135))
  (func (export "\\f3\\af\\bf\\be") (result i32) (i32.const 136))
  (func (export "\\f3\\af\\bf\\bf") (result i32) (i32.const 137))
  (func (export "\\f3\\bf\\bf\\be") (result i32) (i32.const 138))
  (func (export "\\f3\\bf\\bf\\bf") (result i32) (i32.const 139))
  (func (export "\\f4\\8f\\bf\\be") (result i32) (i32.const 140))
  (func (export "\\f4\\8f\\bf\\bf") (result i32) (i32.const 141))

  ;; Test an interrobang with combining diacritical marks above.
  ;; https://xkcd.com/1209/
  (func (export "\u{308}\u{203d}\u{308}\u{309}") (result i32) (i32.const 142))

  ;; Test that RLM/LRM don't change the logical byte order.
  (func (export "abc") (result i32) (i32.const 143))
  (func (export "\u{202d}abc") (result i32) (i32.const 144))
  (func (export "\u{202e}cba") (result i32) (i32.const 145))
  (func (export "\u{202d}abc\u{202e}") (result i32) (i32.const 146))
  (func (export "\u{202e}cba\u{202d}") (result i32) (i32.const 147))

  ;; Test that Unicode font variations are preserved.
  (func (export "\u{1d468}") (result i32) (i32.const 148))
  (func (export "\u{1d434}") (result i32) (i32.const 149))
  (func (export "\u{1d608}") (result i32) (i32.const 150))
  (func (export "\u{1d63c}") (result i32) (i32.const 151))
  (func (export "\u{1d400}") (result i32) (i32.const 152))
  (func (export "\u{1d4d0}") (result i32) (i32.const 153))
  (func (export "\u{1d56c}") (result i32) (i32.const 154))
  (func (export "\u{1d5d4}") (result i32) (i32.const 155))
  (func (export "\u{1d49c}") (result i32) (i32.const 156))
  (func (export "\u{1d504}") (result i32) (i32.const 157))
  (func (export "\u{1d538}") (result i32) (i32.const 158))
  (func (export "\u{1d5a0}") (result i32) (i32.const 159))
  (func (export "\u{1d670}") (result i32) (i32.const 160))
  (func (export "\u{1d00}") (result i32) (i32.const 161))

  ;; Test that various additional letter variations are preserved.
  ;; (U+0040, U+0061, U+0041, U+00C5, U+0041 U+030A, U+212B, and the font
  ;; variations are covered above.)
  (func (export "\u{1d2c}") (result i32) (i32.const 162))
  (func (export "\u{24b6}") (result i32) (i32.const 163))
  (func (export "\u{ff21}") (result i32) (i32.const 164))
  (func (export "\u{1f110}") (result i32) (i32.const 165))
  (func (export "\u{1f130}") (result i32) (i32.const 166))
  (func (export "\u{e0041}") (result i32) (i32.const 167))
  (func (export "U+0041") (result i32) (i32.const 168))
  (func (export "A\u{200b}") (result i32) (i32.const 169))
  (func (export "\u{410}") (result i32) (i32.const 170))
  (func (export "\u{a656}") (result i32) (i32.const 171))
  (func (export "\u{2dfc}") (result i32) (i32.const 172))
  (func (export "\u{2df6}") (result i32) (i32.const 173))
  (func (export "\u{2c6f}") (result i32) (i32.const 174))
  (func (export "\u{1f150}") (result i32) (i32.const 175))
  (func (export "\u{1f170}") (result i32) (i32.const 176))
  (func (export "\u{2c2d}") (result i32) (i32.const 177))
  (func (export "\u{10402}") (result i32) (i32.const 178))
  (func (export "\u{10408}") (result i32) (i32.const 179))
  (func (export "\u{104b0}") (result i32) (i32.const 180))
  (func (export "\u{c0}") (result i32) (i32.const 181))
  (func (export "\u{c1}") (result i32) (i32.const 182))
  (func (export "\u{c2}") (result i32) (i32.const 183))
  (func (export "\u{c3}") (result i32) (i32.const 184))
  (func (export "\u{c4}") (result i32) (i32.const 185))
  (func (export "\u{100}") (result i32) (i32.const 186))
  (func (export "\u{102}") (result i32) (i32.const 187))
  (func (export "\u{104}") (result i32) (i32.const 188))
  (func (export "\u{1cd}") (result i32) (i32.const 189))
  (func (export "\u{1de}") (result i32) (i32.const 190))
  (func (export "\u{1e0}") (result i32) (i32.const 191))
  (func (export "\u{1fa}") (result i32) (i32.const 192))
  (func (export "\u{200}") (result i32) (i32.const 193))
  (func (export "\u{202}") (result i32) (i32.const 194))
  (func (export "\u{226}") (result i32) (i32.const 195))
  (func (export "\u{23a}") (result i32) (i32.const 196))
  (func (export "\u{4d0}") (result i32) (i32.const 197))
  (func (export "\u{4d2}") (result i32) (i32.const 198))
  (func (export "\u{7ca}") (result i32) (i32.const 199))
  (func (export "\u{821}") (result i32) (i32.const 200))
  (func (export "\u{822}") (result i32) (i32.const 201))
  (func (export "\u{823}") (result i32) (i32.const 202))
  (func (export "\u{824}") (result i32) (i32.const 203))
  (func (export "\u{825}") (result i32) (i32.const 204))
  (func (export "\u{904}") (result i32) (i32.const 205))
  (func (export "\u{905}") (result i32) (i32.const 206))
  (func (export "\u{972}") (result i32) (i32.const 207))
  (func (export "\u{985}") (result i32) (i32.const 208))
  (func (export "\u{a05}") (result i32) (i32.const 209))
  (func (export "\u{a85}") (result i32) (i32.const 210))
  (func (export "\u{b05}") (result i32) (i32.const 211))
  (func (export "\u{b85}") (result i32) (i32.const 212))
  (func (export "\u{c05}") (result i32) (i32.const 213))
  (func (export "\u{c85}") (result i32) (i32.const 214))
  (func (export "\u{d05}") (result i32) (i32.const 215))
  (func (export "\u{e30}") (result i32) (i32.const 216))
  (func (export "\u{eb0}") (result i32) (i32.const 217))
  (func (export "\u{f01}") (result i32) (i32.const 218))
  (func (export "\u{f68}") (result i32) (i32.const 219))
  (func (export "\u{fb8}") (result i32) (i32.const 220))
  (func (export "\u{1021}") (result i32) (i32.const 221))
  (func (export "\u{1022}") (result i32) (i32.const 222))
  (func (export "\u{109c}") (result i32) (i32.const 223))
  (func (export "\u{1161}") (result i32) (i32.const 224))
  (func (export "\u{12a0}") (result i32) (i32.const 225))
  (func (export "\u{12d0}") (result i32) (i32.const 226))
  (func (export "\u{13a0}") (result i32) (i32.const 227))
  (func (export "\u{140a}") (result i32) (i32.const 228))
  (func (export "\u{15b3}") (result i32) (i32.const 229))
  (func (export "\u{16a8}") (result i32) (i32.const 230))
  (func (export "\u{16aa}") (result i32) (i32.const 231))
  (func (export "\u{16c6}") (result i32) (i32.const 232))
  (func (export "\u{1700}") (result i32) (i32.const 233))
  (func (export "\u{1720}") (result i32) (i32.const 234))
  (func (export "\u{1740}") (result i32) (i32.const 235))
  (func (export "\u{1760}") (result i32) (i32.const 236))
  (func (export "\u{1820}") (result i32) (i32.const 237))
  (func (export "\u{1887}") (result i32) (i32.const 238))
  (func (export "\u{1920}") (result i32) (i32.const 239))
  (func (export "\u{1963}") (result i32) (i32.const 240))
  (func (export "\u{1a15}") (result i32) (i32.const 241))
  (func (export "\u{1a4b}") (result i32) (i32.const 242))
  (func (export "\u{1a61}") (result i32) (i32.const 243))
  (func (export "\u{1b83}") (result i32) (i32.const 244))
  (func (export "\u{1bc0}") (result i32) (i32.const 245))
  (func (export "\u{1bc1}") (result i32) (i32.const 246))
  (func (export "\u{1c23}") (result i32) (i32.const 247))
  (func (export "\u{1e00}") (result i32) (i32.const 248))
  (func (export "\u{1ea0}") (result i32) (i32.const 249))
  (func (export "\u{1ea2}") (result i32) (i32.const 250))
  (func (export "\u{1ea4}") (result i32) (i32.const 251))
  (func (export "\u{1ea6}") (result i32) (i32.const 252))
  (func (export "\u{1ea8}") (result i32) (i32.const 253))
  (func (export "\u{1eaa}") (result i32) (i32.const 254))
  (func (export "\u{1eac}") (result i32) (i32.const 255))
  (func (export "\u{1eae}") (result i32) (i32.const 256))
  (func (export "\u{1eb0}") (result i32) (i32.const 257))
  (func (export "\u{1eb2}") (result i32) (i32.const 258))
  (func (export "\u{1eb4}") (result i32) (i32.const 259))
  (func (export "\u{1eb6}") (result i32) (i32.const 260))
  (func (export "\u{3042}") (result i32) (i32.const 261))
  (func (export "\u{30a2}") (result i32) (i32.const 262))
  (func (export "\u{311a}") (result i32) (i32.const 263))
  (func (export "\u{314f}") (result i32) (i32.const 264))
  (func (export "\u{320e}") (result i32) (i32.const 265))
  (func (export "\u{320f}") (result i32) (i32.const 266))
  (func (export "\u{3210}") (result i32) (i32.const 267))
  (func (export "\u{3211}") (result i32) (i32.const 268))
  (func (export "\u{3212}") (result i32) (i32.const 269))
  (func (export "\u{3213}") (result i32) (i32.const 270))
  (func (export "\u{3214}") (result i32) (i32.const 271))
  (func (export "\u{3215}") (result i32) (i32.const 272))
  (func (export "\u{3216}") (result i32) (i32.const 273))
  (func (export "\u{3217}") (result i32) (i32.const 274))
  (func (export "\u{3218}") (result i32) (i32.const 275))
  (func (export "\u{3219}") (result i32) (i32.const 276))
  (func (export "\u{321a}") (result i32) (i32.const 277))
  (func (export "\u{321b}") (result i32) (i32.const 278))
  (func (export "\u{326e}") (result i32) (i32.const 279))
  (func (export "\u{326f}") (result i32) (i32.const 280))
  (func (export "\u{3270}") (result i32) (i32.const 281))
  (func (export "\u{3271}") (result i32) (i32.const 282))
  (func (export "\u{3272}") (result i32) (i32.const 283))
  (func (export "\u{3273}") (result i32) (i32.const 284))
  (func (export "\u{3274}") (result i32) (i32.const 285))
  (func (export "\u{3275}") (result i32) (i32.const 286))
  (func (export "\u{3276}") (result i32) (i32.const 287))
  (func (export "\u{3277}") (result i32) (i32.const 288))
  (func (export "\u{3278}") (result i32) (i32.const 289))
  (func (export "\u{3279}") (result i32) (i32.const 290))
  (func (export "\u{327a}") (result i32) (i32.const 291))
  (func (export "\u{327b}") (result i32) (i32.const 292))
  (func (export "\u{32d0}") (result i32) (i32.const 293))
  (func (export "\u{a00a}") (result i32) (i32.const 294))
  (func (export "\u{a4ee}") (result i32) (i32.const 295))
  (func (export "\u{a549}") (result i32) (i32.const 296))
  (func (export "\u{a6a0}") (result i32) (i32.const 297))
  (func (export "\u{a800}") (result i32) (i32.const 298))
  (func (export "\u{a823}") (result i32) (i32.const 299))
  (func (export "\u{a85d}") (result i32) (i32.const 300))
  (func (export "\u{a882}") (result i32) (i32.const 301))
  (func (export "\u{a8ea}") (result i32) (i32.const 302))
  (func (export "\u{a922}") (result i32) (i32.const 303))
  (func (export "\u{a946}") (result i32) (i32.const 304))
  (func (export "\u{a984}") (result i32) (i32.const 305))
  (func (export "\u{aa00}") (result i32) (i32.const 306))
  (func (export "\u{ff71}") (result i32) (i32.const 307))
  (func (export "\u{ffc2}") (result i32) (i32.const 308))
  (func (export "\u{10000}") (result i32) (i32.const 309))
  (func (export "\u{10280}") (result i32) (i32.const 310))
  (func (export "\u{102a0}") (result i32) (i32.const 311))
  (func (export "\u{10300}") (result i32) (i32.const 312))
  (func (export "\u{103a0}") (result i32) (i32.const 313))
  (func (export "\u{10496}") (result i32) (i32.const 314))
  (func (export "\u{10500}") (result i32) (i32.const 315))
  (func (export "\u{10740}") (result i32) (i32.const 316))
  (func (export "\u{10800}") (result i32) (i32.const 317))
  (func (export "\u{10920}") (result i32) (i32.const 318))
  (func (export "\u{10980}") (result i32) (i32.const 319))
  (func (export "\u{109a0}") (result i32) (i32.const 320))
  (func (export "\u{10a00}") (result i32) (i32.const 321))
  (func (export "\u{10b00}") (result i32) (i32.const 322))
  (func (export "\u{10c00}") (result i32) (i32.const 323))
  (func (export "\u{10c01}") (result i32) (i32.const 324))
  (func (export "\u{10c80}") (result i32) (i32.const 325))
  (func (export "\u{11005}") (result i32) (i32.const 326))
  (func (export "\u{11083}") (result i32) (i32.const 327))
  (func (export "\u{11127}") (result i32) (i32.const 328))
  (func (export "\u{11150}") (result i32) (i32.const 329))
  (func (export "\u{11183}") (result i32) (i32.const 330))
  (func (export "\u{11200}") (result i32) (i32.const 331))
  (func (export "\u{11280}") (result i32) (i32.const 332))
  (func (export "\u{112b0}") (result i32) (i32.const 333))
  (func (export "\u{11305}") (result i32) (i32.const 334))
  (func (export "\u{11370}") (result i32) (i32.const 335))
  (func (export "\u{11400}") (result i32) (i32.const 336))
  (func (export "\u{11481}") (result i32) (i32.const 337))
  (func (export "\u{11580}") (result i32) (i32.const 338))
  (func (export "\u{11600}") (result i32) (i32.const 339))
  (func (export "\u{11680}") (result i32) (i32.const 340))
  (func (export "\u{11712}") (result i32) (i32.const 341))
  (func (export "\u{11720}") (result i32) (i32.const 342))
  (func (export "\u{118a1}") (result i32) (i32.const 343))
  (func (export "\u{11ad5}") (result i32) (i32.const 344))
  (func (export "\u{11c00}") (result i32) (i32.const 345))
  (func (export "\u{11c8f}") (result i32) (i32.const 346))
  (func (export "\u{11caf}") (result i32) (i32.const 347))
  (func (export "\u{12000}") (result i32) (i32.const 348))
  (func (export "\u{169d5}") (result i32) (i32.const 349))
  (func (export "\u{16a46}") (result i32) (i32.const 350))
  (func (export "\u{16ae7}") (result i32) (i32.const 351))
  (func (export "\u{16f54}") (result i32) (i32.const 352))
  (func (export "\u{1bc41}") (result i32) (i32.const 353))
  (func (export "\u{1bc64}") (result i32) (i32.const 354))
  (func (export "\u{1e823}") (result i32) (i32.const 355))
  (func (export "\u{1f1e6}") (result i32) (i32.const 356))
  (func (export "\u{2c6d}") (result i32) (i32.const 357))
  (func (export "\u{39b}") (result i32) (i32.const 358))
  (func (export "\u{2c70}") (result i32) (i32.const 359))
  (func (export "\u{aa}") (result i32) (i32.const 360))
  (func (export "\u{2200}") (result i32) (i32.const 361))
  (func (export "\u{20b3}") (result i32) (i32.const 362))
  (func (export "\u{10900}") (result i32) (i32.const 363))
  (func (export "\u{2c80}") (result i32) (i32.const 364))
  (func (export "\u{10330}") (result i32) (i32.const 365))
  (func (export "\u{386}") (result i32) (i32.const 366))
  (func (export "\u{391}") (result i32) (i32.const 367))
  (func (export "\u{1f08}") (result i32) (i32.const 368))
  (func (export "\u{1f09}") (result i32) (i32.const 369))
  (func (export "\u{1f0a}") (result i32) (i32.const 370))
  (func (export "\u{1f0b}") (result i32) (i32.const 371))
  (func (export "\u{1f0c}") (result i32) (i32.const 372))
  (func (export "\u{1f0d}") (result i32) (i32.const 373))
  (func (export "\u{1f0e}") (result i32) (i32.const 374))
  (func (export "\u{1f0f}") (result i32) (i32.const 375))
  (func (export "\u{1f88}") (result i32) (i32.const 376))
  (func (export "\u{1f89}") (result i32) (i32.const 377))
  (func (export "\u{1f8a}") (result i32) (i32.const 378))
  (func (export "\u{1f8b}") (result i32) (i32.const 379))
  (func (export "\u{1f8c}") (result i32) (i32.const 380))
  (func (export "\u{1f8d}") (result i32) (i32.const 381))
  (func (export "\u{1f8e}") (result i32) (i32.const 382))
  (func (export "\u{1f8f}") (result i32) (i32.const 383))
  (func (export "\u{1fb8}") (result i32) (i32.const 384))
  (func (export "\u{1fb9}") (result i32) (i32.const 385))
  (func (export "\u{1fba}") (result i32) (i32.const 386))
  (func (export "\u{1fbb}") (result i32) (i32.const 387))
  (func (export "\u{1fbc}") (result i32) (i32.const 388))
  (func (export "\u{1d6a8}") (result i32) (i32.const 389))
  (func (export "\u{1d6e2}") (result i32) (i32.const 390))
  (func (export "\u{1d71c}") (result i32) (i32.const 391))
  (func (export "\u{1d756}") (result i32) (i32.const 392))
  (func (export "\u{1d790}") (result i32) (i32.const 393))
  (func (export "\u{2376}") (result i32) (i32.const 394))
  (func (export "\u{237a}") (result i32) (i32.const 395))
  (func (export "\u{2a5c}") (result i32) (i32.const 396))
  (func (export "\u{15c5}") (result i32) (i32.const 397))
  (func (export "\u{13aa}") (result i32) (i32.const 398))

  ;; Test unmatched "closing" and "opening" code points.
  (func (export ")\u{2fa}\u{2fc}\u{145cf}\u{1d174}\u{1d176}\u{1d178}\u{1d17a}\u{207e}\u{208e}\u{2769}\u{276b}\u{27ef}\u{fd3f}\u{fe36}\u{fe5a}\u{ff09}\u{ff60}\u{e0029}\u{2773}\u{2775}\u{27e7}\u{27e9}\u{27eb}\u{27ed}\u{2988}\u{298a}\u{2996}\u{2e23}\u{2e25}\u{fe18}\u{fe38}\u{fe3a}\u{fe3c}\u{fe3e}\u{fe40}\u{fe42}\u{fe44}\u{fe48}\u{fe5c}\u{fe5e}\u{ff3d}\u{ff5d}\u{ff63}\u{e005d}\u{e007d}\u{bb}\u{2019}\u{201d}\u{203a}\u{276f}") (result i32) (i32.const 399))
  (func (export "(\u{2f9}\u{2fb}\u{145ce}\u{1d173}\u{1d175}\u{1d177}\u{1d179}\u{207d}\u{208d}\u{2768}\u{276a}\u{27ee}\u{fd3e}\u{fe35}\u{fe59}\u{ff08}\u{ff5f}\u{e0028}\u{2772}\u{2774}\u{27e6}\u{27e8}\u{27ea}\u{27ec}\u{2987}\u{2989}\u{2995}\u{2e22}\u{2e24}\u{fe17}\u{fe37}\u{fe39}\u{fe3b}\u{fe3d}\u{fe3f}\u{fe41}\u{fe43}\u{fe47}\u{fe5b}\u{fe5d}\u{ff3b}\u{ff5b}\u{ff62}\u{e005b}\u{e007b}\u{ab}\u{2018}\u{201c}\u{2039}\u{276e}") (result i32) (i32.const 400))
  (func (export "\u{1da8b}\u{1daa4}") (result i32) (i32.const 401))
  (func (export "\u{1da8b}") (result i32) (i32.const 402))

  ;; Test that Unicode fraction normalization is not applied.
  (func (export "\u{bd}") (result i32) (i32.const 403))
  (func (export "1\u{2044}2") (result i32) (i32.const 404))
  (func (export "1/2") (result i32) (i32.const 405))
  (func (export "\u{b73}") (result i32) (i32.const 406))
  (func (export "\u{d74}") (result i32) (i32.const 407))
  (func (export "\u{2cfd}") (result i32) (i32.const 408))
  (func (export "\u{a831}") (result i32) (i32.const 409))
  (func (export "\u{10141}") (result i32) (i32.const 410))
  (func (export "\u{10175}") (result i32) (i32.const 411))
  (func (export "\u{10176}") (result i32) (i32.const 412))
  (func (export "\u{109bd}") (result i32) (i32.const 413))
  (func (export "\u{10e7b}") (result i32) (i32.const 414))

  ;; Test a full-width quote.
  (func (export "\u{ff02}") (result i32) (i32.const 415))

  ;; Test that different present and historical representations of the "delete"
  ;; concept are distinct.
  (func (export "\\7f") (result i32) (i32.const 416))
  (func (export "\\08") (result i32) (i32.const 417))
  (func (export "\u{232b}") (result i32) (i32.const 418))
  (func (export "\u{2326}") (result i32) (i32.const 419))
  (func (export "\u{2408}") (result i32) (i32.const 420))
  (func (export "\u{2421}") (result i32) (i32.const 421))
  (func (export "\u{1dfb}") (result i32) (i32.const 422))
  (func (export "\\0f") (result i32) (i32.const 423))
  (func (export "\u{2190}") (result i32) (i32.const 424))
  (func (export "\u{2327}") (result i32) (i32.const 425))
  (func (export "\u{2352}") (result i32) (i32.const 426))
  (func (export "\u{2354}") (result i32) (i32.const 427))
  (func (export "\u{2362}") (result i32) (i32.const 428))
  (func (export "\u{236b}") (result i32) (i32.const 429))

  ;; Test that different representations of the "substitute" concept are
  ;; distinct. (U+FFFD is covered above.)
  (func (export "\\1a") (result i32) (i32.const 430))
  (func (export "\u{2426}") (result i32) (i32.const 431))
  (func (export "\u{241a}") (result i32) (i32.const 432))
  (func (export "\u{fffc}") (result i32) (i32.const 433))
  (func (export "?") (result i32) (i32.const 434))
  (func (export "\u{bf}") (result i32) (i32.const 435))
  (func (export "\u{1945}") (result i32) (i32.const 436))
  (func (export "\u{37e}") (result i32) (i32.const 437))
  (func (export "\u{55e}") (result i32) (i32.const 438))
  (func (export "\u{61f}") (result i32) (i32.const 439))
  (func (export "\u{1367}") (result i32) (i32.const 440))
  (func (export "\u{2047}") (result i32) (i32.const 441))
  (func (export "\u{2370}") (result i32) (i32.const 442))
  (func (export "\u{2753}") (result i32) (i32.const 443))
  (func (export "\u{2754}") (result i32) (i32.const 444))
  (func (export "\u{2cfa}") (result i32) (i32.const 445))
  (func (export "\u{2cfb}") (result i32) (i32.const 446))
  (func (export "\u{2e2e}") (result i32) (i32.const 447))
  (func (export "\u{3244}") (result i32) (i32.const 448))
  (func (export "\u{a60f}") (result i32) (i32.const 449))
  (func (export "\u{a6f7}") (result i32) (i32.const 450))
  (func (export "\u{fe16}") (result i32) (i32.const 451))
  (func (export "\u{fe56}") (result i32) (i32.const 452))
  (func (export "\u{ff1f}") (result i32) (i32.const 453))
  (func (export "\u{11143}") (result i32) (i32.const 454))
  (func (export "\u{1e95f}") (result i32) (i32.const 455))
  (func (export "\u{e003f}") (result i32) (i32.const 456))
  (func (export "\u{16844}") (result i32) (i32.const 457))
  (func (export "\u{2bd1}") (result i32) (i32.const 458))

  ;; Test that different present and historical representations of the
  ;; "paragraph" concept are distinct. (U+2029 is covered above).
  (func (export "\u{b6}") (result i32) (i32.const 459))
  (func (export "\u{204b}") (result i32) (i32.const 460))
  (func (export "\u{700}") (result i32) (i32.const 461))
  (func (export "\u{10fb}") (result i32) (i32.const 462))
  (func (export "\u{1368}") (result i32) (i32.const 463))
  (func (export "\u{3037}") (result i32) (i32.const 464))
  (func (export "\u{2761}") (result i32) (i32.const 465))
  (func (export "\u{2e0f}") (result i32) (i32.const 466))
  (func (export "\u{2e10}") (result i32) (i32.const 467))
  (func (export "\u{2e11}") (result i32) (i32.const 468))
  (func (export "\u{2e0e}") (result i32) (i32.const 469))
  (func (export "\\14") (result i32) (i32.const 470)) ;; \u{b6} in CP437
  (func (export "\u{2619}") (result i32) (i32.const 471))
  (func (export "\u{2e3f}") (result i32) (i32.const 472))
  (func (export "\u{3007}") (result i32) (i32.const 473))
  (func (export "\u{e5b}") (result i32) (i32.const 474))

  ;; Test an unusual character.
  (func (export "\u{a66e}") (result i32) (i32.const 475))

  ;; Test the three characters whose normalization forms under NFC, NFD, NFKC,
  ;; and NFKD are all different.
  ;; http://unicode.org/faq/normalization.html#6
  (func (export "\u{3d3}") (result i32) (i32.const 476))
  (func (export "\u{3d4}") (result i32) (i32.const 477))
  (func (export "\u{1e9b}") (result i32) (i32.const 478))
)`);

// ./test/core/names.wast:615
assert_return(() => invoke($2, ``, []), [value("i32", 0)]);

// ./test/core/names.wast:616
assert_return(() => invoke($2, `0`, []), [value("i32", 1)]);

// ./test/core/names.wast:617
assert_return(() => invoke($2, `-0`, []), [value("i32", 2)]);

// ./test/core/names.wast:618
assert_return(() => invoke($2, `_`, []), [value("i32", 3)]);

// ./test/core/names.wast:619
assert_return(() => invoke($2, `$$`, []), [value("i32", 4)]);

// ./test/core/names.wast:620
assert_return(() => invoke($2, `@`, []), [value("i32", 5)]);

// ./test/core/names.wast:621
assert_return(() => invoke($2, `~!@#$$%^&*()_+\`-={}|[]\\:";'<>?,./ `, []), [value("i32", 6)]);

// ./test/core/names.wast:622
assert_return(() => invoke($2, `NaN`, []), [value("i32", 7)]);

// ./test/core/names.wast:623
assert_return(() => invoke($2, `Infinity`, []), [value("i32", 8)]);

// ./test/core/names.wast:624
assert_return(() => invoke($2, `if`, []), [value("i32", 9)]);

// ./test/core/names.wast:625
assert_return(() => invoke($2, `malloc`, []), [value("i32", 10)]);

// ./test/core/names.wast:626
assert_return(() => invoke($2, `_malloc`, []), [value("i32", 11)]);

// ./test/core/names.wast:627
assert_return(() => invoke($2, `__malloc`, []), [value("i32", 12)]);

// ./test/core/names.wast:628
assert_return(() => invoke($2, `a`, []), [value("i32", 13)]);

// ./test/core/names.wast:629
assert_return(() => invoke($2, `A`, []), [value("i32", 14)]);

// ./test/core/names.wast:630
assert_return(() => invoke($2, `\u{feff}`, []), [value("i32", 15)]);

// ./test/core/names.wast:631
assert_return(() => invoke($2, `\u{c5}`, []), [value("i32", 16)]);

// ./test/core/names.wast:632
assert_return(() => invoke($2, `A\u{30a}`, []), [value("i32", 17)]);

// ./test/core/names.wast:633
assert_return(() => invoke($2, `\u{212b}`, []), [value("i32", 18)]);

// ./test/core/names.wast:634
assert_return(() => invoke($2, `\u{fb03}`, []), [value("i32", 19)]);

// ./test/core/names.wast:635
assert_return(() => invoke($2, `f\u{fb01}`, []), [value("i32", 20)]);

// ./test/core/names.wast:636
assert_return(() => invoke($2, `ffi`, []), [value("i32", 21)]);

// ./test/core/names.wast:637
assert_return(
  () => invoke($2, `\x00\x01\x02\x03\x04\x05\x06\x07\x08	
\x0b\x0c\x0d\x0e\x0f`, []),
  [value("i32", 22)],
);

// ./test/core/names.wast:638
assert_return(
  () => invoke($2, `\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f`, []),
  [value("i32", 23)],
);

// ./test/core/names.wast:639
assert_return(() => invoke($2, ` \x7f`, []), [value("i32", 24)]);

// ./test/core/names.wast:640
assert_return(
  () => invoke($2,
    `\u{80}\u{81}\u{82}\u{83}\u{84}\u{85}\u{86}\u{87}\u{88}\u{89}\u{8a}\u{8b}\u{8c}\u{8d}\u{8e}\u{8f}`,
    [],
  ),
  [value("i32", 25)],
);

// ./test/core/names.wast:641
assert_return(
  () => invoke($2,
    `\u{90}\u{91}\u{92}\u{93}\u{94}\u{95}\u{96}\u{97}\u{98}\u{99}\u{9a}\u{9b}\u{9c}\u{9d}\u{9e}\u{9f}`,
    [],
  ),
  [value("i32", 26)],
);

// ./test/core/names.wast:642
assert_return(
  () => invoke($2, `\u{fff0}\u{fff1}\u{fff2}\u{fff3}\u{fff4}\u{fff5}\u{fff6}\u{fff7}`, []),
  [value("i32", 27)],
);

// ./test/core/names.wast:643
assert_return(
  () => invoke($2, `\u{fff8}\u{fff9}\u{fffa}\u{fffb}\u{fffc}\u{fffd}\u{fffe}\u{ffff}`, []),
  [value("i32", 28)],
);

// ./test/core/names.wast:644
assert_return(
  () => invoke($2,
    `\u{2400}\u{2401}\u{2402}\u{2403}\u{2404}\u{2405}\u{2406}\u{2407}\u{2408}\u{2409}\u{240a}\u{240b}\u{240c}\u{240d}\u{240e}\u{240f}`,
    [],
  ),
  [value("i32", 29)],
);

// ./test/core/names.wast:645
assert_return(
  () => invoke($2,
    `\u{2410}\u{2411}\u{2412}\u{2413}\u{2414}\u{2415}\u{2416}\u{2417}\u{2418}\u{2419}\u{241a}\u{241b}\u{241c}\u{241d}\u{241e}\u{241f}`,
    [],
  ),
  [value("i32", 30)],
);

// ./test/core/names.wast:646
assert_return(() => invoke($2, `\u{2420}\u{2421}`, []), [value("i32", 31)]);

// ./test/core/names.wast:647
assert_return(
  () => invoke($2,
    `\u{fff0}\u{fff1}\u{fff2}\u{fff3}\u{fff4}\u{fff5}\u{fff6}\u{fff7}\u{fff8}\u{fff9}\u{fffa}\u{fffb}\u{fffc}\u{fffd}`,
    [],
  ),
  [value("i32", 32)],
);

// ./test/core/names.wast:648
assert_return(() => invoke($2, `\u{200d}`, []), [value("i32", 33)]);

// ./test/core/names.wast:649
assert_return(() => invoke($2, `\u{200c}`, []), [value("i32", 34)]);

// ./test/core/names.wast:650
assert_return(() => invoke($2, `\u{34f}`, []), [value("i32", 35)]);

// ./test/core/names.wast:651
assert_return(() => invoke($2, `\u{2060}`, []), [value("i32", 36)]);

// ./test/core/names.wast:652
assert_return(() => invoke($2, `\u{2d7f}`, []), [value("i32", 37)]);

// ./test/core/names.wast:653
assert_return(() => invoke($2, `\u{1107f}`, []), [value("i32", 38)]);

// ./test/core/names.wast:654
assert_return(() => invoke($2, `\u{180e}`, []), [value("i32", 39)]);

// ./test/core/names.wast:655
assert_return(
  () => invoke($2, `\u{ffef}\u{200b}\u{a0}\u{ad}\u{2060}\u{1680}\u{202e}\u{202d}`, []),
  [value("i32", 40)],
);

// ./test/core/names.wast:656
assert_return(
  () => invoke($2,
    `\u{200e}\u{200f}\u{2011}\u{2028}\u{2029}\u{202a}\u{202b}\u{202c}\u{202f}\u{2066}\u{2067}\u{2068}\u{2069}`,
    [],
  ),
  [value("i32", 41)],
);

// ./test/core/names.wast:657
assert_return(
  () => invoke($2, `\u{206a}\u{206b}\u{206c}\u{206d}\u{206e}\u{206f}`, []),
  [value("i32", 42)],
);

// ./test/core/names.wast:658
assert_return(() => invoke($2, `\u{2061}\u{2062}\u{2063}\u{2064}`, []), [value("i32", 43)]);

// ./test/core/names.wast:659
assert_return(() => invoke($2, `\u{10000}\u{dffff}\u{10ffff}`, []), [value("i32", 44)]);

// ./test/core/names.wast:660
assert_return(
  () => invoke($2,
    `Z\u{30f}\u{346}\u{306}\u{35b}\u{34c}\u{334}\u{358}\u{35e}\u{347}\u{32b}\u{325}\u{32a}\u{353}\u{348}\u{354}\u{34e}\u{317}\u{31e}\u{33a}\u{32f}\u{331}\u{31e}\u{319}\u{331}\u{31c}\u{316}\u{320}a\u{357}\u{368}\u{30e}\u{304}\u{306}\u{357}\u{33f}\u{361}\u{35f}\u{340}\u{336}\u{341}\u{325}\u{330}\u{333}\u{32d}\u{359}\u{332}\u{331}\u{339}\u{31d}\u{34e}\u{33c}l\u{344}\u{34a}\u{31a}\u{357}\u{366}\u{344}\u{36b}\u{307}\u{341}\u{336}\u{337}\u{349}\u{329}\u{339}\u{32b}\u{31d}\u{356}\u{345}\u{319}\u{332}\u{33c}\u{347}\u{35a}\u{34d}\u{32e}\u{34e}\u{325}\u{345}\u{31e}g\u{343}\u{310}\u{305}\u{36e}\u{314}\u{310}\u{30e}\u{302}\u{30f}\u{33e}\u{34a}\u{30d}\u{34b}\u{34a}\u{367}\u{301}\u{306}\u{366}\u{35e}\u{336}\u{355}\u{354}\u{35a}\u{329}o\u{34b}\u{314}\u{350}\u{36a}\u{369}\u{321}\u{34f}\u{322}\u{327}\u{341}\u{32b}\u{319}\u{324}\u{32e}\u{356}\u{359}\u{353}\u{33a}\u{31c}\u{329}\u{33c}\u{318}\u{320}`,
    [],
  ),
  [value("i32", 45)],
);

// ./test/core/names.wast:661
assert_return(() => invoke($2, `\u{115f}\u{1160}\u{3164}\u{ffa0}`, []), [value("i32", 46)]);

// ./test/core/names.wast:662
assert_return(() => invoke($2, `\u{fe00}`, []), [value("i32", 47)]);

// ./test/core/names.wast:663
assert_return(() => invoke($2, `\u{fe04}`, []), [value("i32", 48)]);

// ./test/core/names.wast:664
assert_return(() => invoke($2, `\u{e0100}`, []), [value("i32", 49)]);

// ./test/core/names.wast:665
assert_return(() => invoke($2, `\u{e01ef}`, []), [value("i32", 50)]);

// ./test/core/names.wast:666
assert_return(() => invoke($2, `\u{308}`, []), [value("i32", 51)]);

// ./test/core/names.wast:667
assert_return(() => invoke($2, `
`, []), [value("i32", 52)]);

// ./test/core/names.wast:668
assert_return(() => invoke($2, `\u{2424}`, []), [value("i32", 53)]);

// ./test/core/names.wast:669
assert_return(() => invoke($2, `\u{2028}`, []), [value("i32", 54)]);

// ./test/core/names.wast:670
assert_return(() => invoke($2, `\x0d`, []), [value("i32", 55)]);

// ./test/core/names.wast:671
assert_return(() => invoke($2, `\x0d
`, []), [value("i32", 56)]);

// ./test/core/names.wast:672
assert_return(() => invoke($2, `
\x0d`, []), [value("i32", 57)]);

// ./test/core/names.wast:673
assert_return(() => invoke($2, `\x1e`, []), [value("i32", 58)]);

// ./test/core/names.wast:674
assert_return(() => invoke($2, `\x0b`, []), [value("i32", 59)]);

// ./test/core/names.wast:675
assert_return(() => invoke($2, `\x0c`, []), [value("i32", 60)]);

// ./test/core/names.wast:676
assert_return(() => invoke($2, `\u{85}`, []), [value("i32", 61)]);

// ./test/core/names.wast:677
assert_return(() => invoke($2, `\u{2029}`, []), [value("i32", 62)]);

// ./test/core/names.wast:678
assert_return(() => invoke($2, `\u{2026}`, []), [value("i32", 63)]);

// ./test/core/names.wast:679
assert_return(() => invoke($2, `\u{23ce}`, []), [value("i32", 64)]);

// ./test/core/names.wast:680
assert_return(() => invoke($2, `\u{8b}`, []), [value("i32", 65)]);

// ./test/core/names.wast:681
assert_return(() => invoke($2, `\u{8c}`, []), [value("i32", 66)]);

// ./test/core/names.wast:682
assert_return(() => invoke($2, `\u{8d}`, []), [value("i32", 67)]);

// ./test/core/names.wast:683
assert_return(() => invoke($2, `\u{21b5}`, []), [value("i32", 68)]);

// ./test/core/names.wast:684
assert_return(() => invoke($2, `\u{21a9}`, []), [value("i32", 69)]);

// ./test/core/names.wast:685
assert_return(() => invoke($2, `\u{2324}`, []), [value("i32", 70)]);

// ./test/core/names.wast:686
assert_return(() => invoke($2, `\u{2936}`, []), [value("i32", 71)]);

// ./test/core/names.wast:687
assert_return(() => invoke($2, `\u{21b2}`, []), [value("i32", 72)]);

// ./test/core/names.wast:688
assert_return(() => invoke($2, `\u{2ba8}`, []), [value("i32", 73)]);

// ./test/core/names.wast:689
assert_return(() => invoke($2, `\u{2bb0}`, []), [value("i32", 74)]);

// ./test/core/names.wast:690
assert_return(() => invoke($2, `\u{fffd}`, []), [value("i32", 75)]);

// ./test/core/names.wast:691
assert_return(() => invoke($2, `\u{fdd0}`, []), [value("i32", 76)]);

// ./test/core/names.wast:692
assert_return(() => invoke($2, `\u{fdd1}`, []), [value("i32", 77)]);

// ./test/core/names.wast:693
assert_return(() => invoke($2, `\u{fdd2}`, []), [value("i32", 78)]);

// ./test/core/names.wast:694
assert_return(() => invoke($2, `\u{fdd3}`, []), [value("i32", 79)]);

// ./test/core/names.wast:695
assert_return(() => invoke($2, `\u{fdd4}`, []), [value("i32", 80)]);

// ./test/core/names.wast:696
assert_return(() => invoke($2, `\u{fdd5}`, []), [value("i32", 81)]);

// ./test/core/names.wast:697
assert_return(() => invoke($2, `\u{fdd6}`, []), [value("i32", 82)]);

// ./test/core/names.wast:698
assert_return(() => invoke($2, `\u{fdd7}`, []), [value("i32", 83)]);

// ./test/core/names.wast:699
assert_return(() => invoke($2, `\u{fdd8}`, []), [value("i32", 84)]);

// ./test/core/names.wast:700
assert_return(() => invoke($2, `\u{fdd9}`, []), [value("i32", 85)]);

// ./test/core/names.wast:701
assert_return(() => invoke($2, `\u{fdda}`, []), [value("i32", 86)]);

// ./test/core/names.wast:702
assert_return(() => invoke($2, `\u{fddb}`, []), [value("i32", 87)]);

// ./test/core/names.wast:703
assert_return(() => invoke($2, `\u{fddc}`, []), [value("i32", 88)]);

// ./test/core/names.wast:704
assert_return(() => invoke($2, `\u{fddd}`, []), [value("i32", 89)]);

// ./test/core/names.wast:705
assert_return(() => invoke($2, `\u{fdde}`, []), [value("i32", 90)]);

// ./test/core/names.wast:706
assert_return(() => invoke($2, `\u{fddf}`, []), [value("i32", 91)]);

// ./test/core/names.wast:707
assert_return(() => invoke($2, `\u{fde0}`, []), [value("i32", 92)]);

// ./test/core/names.wast:708
assert_return(() => invoke($2, `\u{fde1}`, []), [value("i32", 93)]);

// ./test/core/names.wast:709
assert_return(() => invoke($2, `\u{fde2}`, []), [value("i32", 94)]);

// ./test/core/names.wast:710
assert_return(() => invoke($2, `\u{fde3}`, []), [value("i32", 95)]);

// ./test/core/names.wast:711
assert_return(() => invoke($2, `\u{fde4}`, []), [value("i32", 96)]);

// ./test/core/names.wast:712
assert_return(() => invoke($2, `\u{fde5}`, []), [value("i32", 97)]);

// ./test/core/names.wast:713
assert_return(() => invoke($2, `\u{fde6}`, []), [value("i32", 98)]);

// ./test/core/names.wast:714
assert_return(() => invoke($2, `\u{fde7}`, []), [value("i32", 99)]);

// ./test/core/names.wast:715
assert_return(() => invoke($2, `\u{fde8}`, []), [value("i32", 100)]);

// ./test/core/names.wast:716
assert_return(() => invoke($2, `\u{fde9}`, []), [value("i32", 101)]);

// ./test/core/names.wast:717
assert_return(() => invoke($2, `\u{fdea}`, []), [value("i32", 102)]);

// ./test/core/names.wast:718
assert_return(() => invoke($2, `\u{fdeb}`, []), [value("i32", 103)]);

// ./test/core/names.wast:719
assert_return(() => invoke($2, `\u{fdec}`, []), [value("i32", 104)]);

// ./test/core/names.wast:720
assert_return(() => invoke($2, `\u{fded}`, []), [value("i32", 105)]);

// ./test/core/names.wast:721
assert_return(() => invoke($2, `\u{fdee}`, []), [value("i32", 106)]);

// ./test/core/names.wast:722
assert_return(() => invoke($2, `\u{fdef}`, []), [value("i32", 107)]);

// ./test/core/names.wast:723
assert_return(() => invoke($2, `\u{fffe}`, []), [value("i32", 108)]);

// ./test/core/names.wast:724
assert_return(() => invoke($2, `\u{ffff}`, []), [value("i32", 109)]);

// ./test/core/names.wast:725
assert_return(() => invoke($2, `\u{1fffe}`, []), [value("i32", 110)]);

// ./test/core/names.wast:726
assert_return(() => invoke($2, `\u{1ffff}`, []), [value("i32", 111)]);

// ./test/core/names.wast:727
assert_return(() => invoke($2, `\u{2fffe}`, []), [value("i32", 112)]);

// ./test/core/names.wast:728
assert_return(() => invoke($2, `\u{2ffff}`, []), [value("i32", 113)]);

// ./test/core/names.wast:729
assert_return(() => invoke($2, `\u{3fffe}`, []), [value("i32", 114)]);

// ./test/core/names.wast:730
assert_return(() => invoke($2, `\u{3ffff}`, []), [value("i32", 115)]);

// ./test/core/names.wast:731
assert_return(() => invoke($2, `\u{4fffe}`, []), [value("i32", 116)]);

// ./test/core/names.wast:732
assert_return(() => invoke($2, `\u{4ffff}`, []), [value("i32", 117)]);

// ./test/core/names.wast:733
assert_return(() => invoke($2, `\u{5fffe}`, []), [value("i32", 118)]);

// ./test/core/names.wast:734
assert_return(() => invoke($2, `\u{5ffff}`, []), [value("i32", 119)]);

// ./test/core/names.wast:735
assert_return(() => invoke($2, `\u{6fffe}`, []), [value("i32", 120)]);

// ./test/core/names.wast:736
assert_return(() => invoke($2, `\u{6ffff}`, []), [value("i32", 121)]);

// ./test/core/names.wast:737
assert_return(() => invoke($2, `\u{7fffe}`, []), [value("i32", 122)]);

// ./test/core/names.wast:738
assert_return(() => invoke($2, `\u{7ffff}`, []), [value("i32", 123)]);

// ./test/core/names.wast:739
assert_return(() => invoke($2, `\u{8fffe}`, []), [value("i32", 124)]);

// ./test/core/names.wast:740
assert_return(() => invoke($2, `\u{8ffff}`, []), [value("i32", 125)]);

// ./test/core/names.wast:741
assert_return(() => invoke($2, `\u{9fffe}`, []), [value("i32", 126)]);

// ./test/core/names.wast:742
assert_return(() => invoke($2, `\u{9ffff}`, []), [value("i32", 127)]);

// ./test/core/names.wast:743
assert_return(() => invoke($2, `\u{afffe}`, []), [value("i32", 128)]);

// ./test/core/names.wast:744
assert_return(() => invoke($2, `\u{affff}`, []), [value("i32", 129)]);

// ./test/core/names.wast:745
assert_return(() => invoke($2, `\u{bfffe}`, []), [value("i32", 130)]);

// ./test/core/names.wast:746
assert_return(() => invoke($2, `\u{bffff}`, []), [value("i32", 131)]);

// ./test/core/names.wast:747
assert_return(() => invoke($2, `\u{cfffe}`, []), [value("i32", 132)]);

// ./test/core/names.wast:748
assert_return(() => invoke($2, `\u{cffff}`, []), [value("i32", 133)]);

// ./test/core/names.wast:749
assert_return(() => invoke($2, `\u{dfffe}`, []), [value("i32", 134)]);

// ./test/core/names.wast:750
assert_return(() => invoke($2, `\u{dffff}`, []), [value("i32", 135)]);

// ./test/core/names.wast:751
assert_return(() => invoke($2, `\u{efffe}`, []), [value("i32", 136)]);

// ./test/core/names.wast:752
assert_return(() => invoke($2, `\u{effff}`, []), [value("i32", 137)]);

// ./test/core/names.wast:753
assert_return(() => invoke($2, `\u{ffffe}`, []), [value("i32", 138)]);

// ./test/core/names.wast:754
assert_return(() => invoke($2, `\u{fffff}`, []), [value("i32", 139)]);

// ./test/core/names.wast:755
assert_return(() => invoke($2, `\u{10fffe}`, []), [value("i32", 140)]);

// ./test/core/names.wast:756
assert_return(() => invoke($2, `\u{10ffff}`, []), [value("i32", 141)]);

// ./test/core/names.wast:757
assert_return(() => invoke($2, `\u{308}\u{203d}\u{308}\u{309}`, []), [value("i32", 142)]);

// ./test/core/names.wast:758
assert_return(() => invoke($2, `abc`, []), [value("i32", 143)]);

// ./test/core/names.wast:759
assert_return(() => invoke($2, `\u{202d}abc`, []), [value("i32", 144)]);

// ./test/core/names.wast:760
assert_return(() => invoke($2, `\u{202e}cba`, []), [value("i32", 145)]);

// ./test/core/names.wast:761
assert_return(() => invoke($2, `\u{202d}abc\u{202e}`, []), [value("i32", 146)]);

// ./test/core/names.wast:762
assert_return(() => invoke($2, `\u{202e}cba\u{202d}`, []), [value("i32", 147)]);

// ./test/core/names.wast:763
assert_return(() => invoke($2, `\u{1d468}`, []), [value("i32", 148)]);

// ./test/core/names.wast:764
assert_return(() => invoke($2, `\u{1d434}`, []), [value("i32", 149)]);

// ./test/core/names.wast:765
assert_return(() => invoke($2, `\u{1d608}`, []), [value("i32", 150)]);

// ./test/core/names.wast:766
assert_return(() => invoke($2, `\u{1d63c}`, []), [value("i32", 151)]);

// ./test/core/names.wast:767
assert_return(() => invoke($2, `\u{1d400}`, []), [value("i32", 152)]);

// ./test/core/names.wast:768
assert_return(() => invoke($2, `\u{1d4d0}`, []), [value("i32", 153)]);

// ./test/core/names.wast:769
assert_return(() => invoke($2, `\u{1d56c}`, []), [value("i32", 154)]);

// ./test/core/names.wast:770
assert_return(() => invoke($2, `\u{1d5d4}`, []), [value("i32", 155)]);

// ./test/core/names.wast:771
assert_return(() => invoke($2, `\u{1d49c}`, []), [value("i32", 156)]);

// ./test/core/names.wast:772
assert_return(() => invoke($2, `\u{1d504}`, []), [value("i32", 157)]);

// ./test/core/names.wast:773
assert_return(() => invoke($2, `\u{1d538}`, []), [value("i32", 158)]);

// ./test/core/names.wast:774
assert_return(() => invoke($2, `\u{1d5a0}`, []), [value("i32", 159)]);

// ./test/core/names.wast:775
assert_return(() => invoke($2, `\u{1d670}`, []), [value("i32", 160)]);

// ./test/core/names.wast:776
assert_return(() => invoke($2, `\u{1d00}`, []), [value("i32", 161)]);

// ./test/core/names.wast:777
assert_return(() => invoke($2, `\u{1d2c}`, []), [value("i32", 162)]);

// ./test/core/names.wast:778
assert_return(() => invoke($2, `\u{24b6}`, []), [value("i32", 163)]);

// ./test/core/names.wast:779
assert_return(() => invoke($2, `\u{ff21}`, []), [value("i32", 164)]);

// ./test/core/names.wast:780
assert_return(() => invoke($2, `\u{1f110}`, []), [value("i32", 165)]);

// ./test/core/names.wast:781
assert_return(() => invoke($2, `\u{1f130}`, []), [value("i32", 166)]);

// ./test/core/names.wast:782
assert_return(() => invoke($2, `\u{e0041}`, []), [value("i32", 167)]);

// ./test/core/names.wast:783
assert_return(() => invoke($2, `U+0041`, []), [value("i32", 168)]);

// ./test/core/names.wast:784
assert_return(() => invoke($2, `A\u{200b}`, []), [value("i32", 169)]);

// ./test/core/names.wast:785
assert_return(() => invoke($2, `\u{410}`, []), [value("i32", 170)]);

// ./test/core/names.wast:786
assert_return(() => invoke($2, `\u{a656}`, []), [value("i32", 171)]);

// ./test/core/names.wast:787
assert_return(() => invoke($2, `\u{2dfc}`, []), [value("i32", 172)]);

// ./test/core/names.wast:788
assert_return(() => invoke($2, `\u{2df6}`, []), [value("i32", 173)]);

// ./test/core/names.wast:789
assert_return(() => invoke($2, `\u{2c6f}`, []), [value("i32", 174)]);

// ./test/core/names.wast:790
assert_return(() => invoke($2, `\u{1f150}`, []), [value("i32", 175)]);

// ./test/core/names.wast:791
assert_return(() => invoke($2, `\u{1f170}`, []), [value("i32", 176)]);

// ./test/core/names.wast:792
assert_return(() => invoke($2, `\u{2c2d}`, []), [value("i32", 177)]);

// ./test/core/names.wast:793
assert_return(() => invoke($2, `\u{10402}`, []), [value("i32", 178)]);

// ./test/core/names.wast:794
assert_return(() => invoke($2, `\u{10408}`, []), [value("i32", 179)]);

// ./test/core/names.wast:795
assert_return(() => invoke($2, `\u{104b0}`, []), [value("i32", 180)]);

// ./test/core/names.wast:796
assert_return(() => invoke($2, `\u{c0}`, []), [value("i32", 181)]);

// ./test/core/names.wast:797
assert_return(() => invoke($2, `\u{c1}`, []), [value("i32", 182)]);

// ./test/core/names.wast:798
assert_return(() => invoke($2, `\u{c2}`, []), [value("i32", 183)]);

// ./test/core/names.wast:799
assert_return(() => invoke($2, `\u{c3}`, []), [value("i32", 184)]);

// ./test/core/names.wast:800
assert_return(() => invoke($2, `\u{c4}`, []), [value("i32", 185)]);

// ./test/core/names.wast:801
assert_return(() => invoke($2, `\u{100}`, []), [value("i32", 186)]);

// ./test/core/names.wast:802
assert_return(() => invoke($2, `\u{102}`, []), [value("i32", 187)]);

// ./test/core/names.wast:803
assert_return(() => invoke($2, `\u{104}`, []), [value("i32", 188)]);

// ./test/core/names.wast:804
assert_return(() => invoke($2, `\u{1cd}`, []), [value("i32", 189)]);

// ./test/core/names.wast:805
assert_return(() => invoke($2, `\u{1de}`, []), [value("i32", 190)]);

// ./test/core/names.wast:806
assert_return(() => invoke($2, `\u{1e0}`, []), [value("i32", 191)]);

// ./test/core/names.wast:807
assert_return(() => invoke($2, `\u{1fa}`, []), [value("i32", 192)]);

// ./test/core/names.wast:808
assert_return(() => invoke($2, `\u{200}`, []), [value("i32", 193)]);

// ./test/core/names.wast:809
assert_return(() => invoke($2, `\u{202}`, []), [value("i32", 194)]);

// ./test/core/names.wast:810
assert_return(() => invoke($2, `\u{226}`, []), [value("i32", 195)]);

// ./test/core/names.wast:811
assert_return(() => invoke($2, `\u{23a}`, []), [value("i32", 196)]);

// ./test/core/names.wast:812
assert_return(() => invoke($2, `\u{4d0}`, []), [value("i32", 197)]);

// ./test/core/names.wast:813
assert_return(() => invoke($2, `\u{4d2}`, []), [value("i32", 198)]);

// ./test/core/names.wast:814
assert_return(() => invoke($2, `\u{7ca}`, []), [value("i32", 199)]);

// ./test/core/names.wast:815
assert_return(() => invoke($2, `\u{821}`, []), [value("i32", 200)]);

// ./test/core/names.wast:816
assert_return(() => invoke($2, `\u{822}`, []), [value("i32", 201)]);

// ./test/core/names.wast:817
assert_return(() => invoke($2, `\u{823}`, []), [value("i32", 202)]);

// ./test/core/names.wast:818
assert_return(() => invoke($2, `\u{824}`, []), [value("i32", 203)]);

// ./test/core/names.wast:819
assert_return(() => invoke($2, `\u{825}`, []), [value("i32", 204)]);

// ./test/core/names.wast:820
assert_return(() => invoke($2, `\u{904}`, []), [value("i32", 205)]);

// ./test/core/names.wast:821
assert_return(() => invoke($2, `\u{905}`, []), [value("i32", 206)]);

// ./test/core/names.wast:822
assert_return(() => invoke($2, `\u{972}`, []), [value("i32", 207)]);

// ./test/core/names.wast:823
assert_return(() => invoke($2, `\u{985}`, []), [value("i32", 208)]);

// ./test/core/names.wast:824
assert_return(() => invoke($2, `\u{a05}`, []), [value("i32", 209)]);

// ./test/core/names.wast:825
assert_return(() => invoke($2, `\u{a85}`, []), [value("i32", 210)]);

// ./test/core/names.wast:826
assert_return(() => invoke($2, `\u{b05}`, []), [value("i32", 211)]);

// ./test/core/names.wast:827
assert_return(() => invoke($2, `\u{b85}`, []), [value("i32", 212)]);

// ./test/core/names.wast:828
assert_return(() => invoke($2, `\u{c05}`, []), [value("i32", 213)]);

// ./test/core/names.wast:829
assert_return(() => invoke($2, `\u{c85}`, []), [value("i32", 214)]);

// ./test/core/names.wast:830
assert_return(() => invoke($2, `\u{d05}`, []), [value("i32", 215)]);

// ./test/core/names.wast:831
assert_return(() => invoke($2, `\u{e30}`, []), [value("i32", 216)]);

// ./test/core/names.wast:832
assert_return(() => invoke($2, `\u{eb0}`, []), [value("i32", 217)]);

// ./test/core/names.wast:833
assert_return(() => invoke($2, `\u{f01}`, []), [value("i32", 218)]);

// ./test/core/names.wast:834
assert_return(() => invoke($2, `\u{f68}`, []), [value("i32", 219)]);

// ./test/core/names.wast:835
assert_return(() => invoke($2, `\u{fb8}`, []), [value("i32", 220)]);

// ./test/core/names.wast:836
assert_return(() => invoke($2, `\u{1021}`, []), [value("i32", 221)]);

// ./test/core/names.wast:837
assert_return(() => invoke($2, `\u{1022}`, []), [value("i32", 222)]);

// ./test/core/names.wast:838
assert_return(() => invoke($2, `\u{109c}`, []), [value("i32", 223)]);

// ./test/core/names.wast:839
assert_return(() => invoke($2, `\u{1161}`, []), [value("i32", 224)]);

// ./test/core/names.wast:840
assert_return(() => invoke($2, `\u{12a0}`, []), [value("i32", 225)]);

// ./test/core/names.wast:841
assert_return(() => invoke($2, `\u{12d0}`, []), [value("i32", 226)]);

// ./test/core/names.wast:842
assert_return(() => invoke($2, `\u{13a0}`, []), [value("i32", 227)]);

// ./test/core/names.wast:843
assert_return(() => invoke($2, `\u{140a}`, []), [value("i32", 228)]);

// ./test/core/names.wast:844
assert_return(() => invoke($2, `\u{15b3}`, []), [value("i32", 229)]);

// ./test/core/names.wast:845
assert_return(() => invoke($2, `\u{16a8}`, []), [value("i32", 230)]);

// ./test/core/names.wast:846
assert_return(() => invoke($2, `\u{16aa}`, []), [value("i32", 231)]);

// ./test/core/names.wast:847
assert_return(() => invoke($2, `\u{16c6}`, []), [value("i32", 232)]);

// ./test/core/names.wast:848
assert_return(() => invoke($2, `\u{1700}`, []), [value("i32", 233)]);

// ./test/core/names.wast:849
assert_return(() => invoke($2, `\u{1720}`, []), [value("i32", 234)]);

// ./test/core/names.wast:850
assert_return(() => invoke($2, `\u{1740}`, []), [value("i32", 235)]);

// ./test/core/names.wast:851
assert_return(() => invoke($2, `\u{1760}`, []), [value("i32", 236)]);

// ./test/core/names.wast:852
assert_return(() => invoke($2, `\u{1820}`, []), [value("i32", 237)]);

// ./test/core/names.wast:853
assert_return(() => invoke($2, `\u{1887}`, []), [value("i32", 238)]);

// ./test/core/names.wast:854
assert_return(() => invoke($2, `\u{1920}`, []), [value("i32", 239)]);

// ./test/core/names.wast:855
assert_return(() => invoke($2, `\u{1963}`, []), [value("i32", 240)]);

// ./test/core/names.wast:856
assert_return(() => invoke($2, `\u{1a15}`, []), [value("i32", 241)]);

// ./test/core/names.wast:857
assert_return(() => invoke($2, `\u{1a4b}`, []), [value("i32", 242)]);

// ./test/core/names.wast:858
assert_return(() => invoke($2, `\u{1a61}`, []), [value("i32", 243)]);

// ./test/core/names.wast:859
assert_return(() => invoke($2, `\u{1b83}`, []), [value("i32", 244)]);

// ./test/core/names.wast:860
assert_return(() => invoke($2, `\u{1bc0}`, []), [value("i32", 245)]);

// ./test/core/names.wast:861
assert_return(() => invoke($2, `\u{1bc1}`, []), [value("i32", 246)]);

// ./test/core/names.wast:862
assert_return(() => invoke($2, `\u{1c23}`, []), [value("i32", 247)]);

// ./test/core/names.wast:863
assert_return(() => invoke($2, `\u{1e00}`, []), [value("i32", 248)]);

// ./test/core/names.wast:864
assert_return(() => invoke($2, `\u{1ea0}`, []), [value("i32", 249)]);

// ./test/core/names.wast:865
assert_return(() => invoke($2, `\u{1ea2}`, []), [value("i32", 250)]);

// ./test/core/names.wast:866
assert_return(() => invoke($2, `\u{1ea4}`, []), [value("i32", 251)]);

// ./test/core/names.wast:867
assert_return(() => invoke($2, `\u{1ea6}`, []), [value("i32", 252)]);

// ./test/core/names.wast:868
assert_return(() => invoke($2, `\u{1ea8}`, []), [value("i32", 253)]);

// ./test/core/names.wast:869
assert_return(() => invoke($2, `\u{1eaa}`, []), [value("i32", 254)]);

// ./test/core/names.wast:870
assert_return(() => invoke($2, `\u{1eac}`, []), [value("i32", 255)]);

// ./test/core/names.wast:871
assert_return(() => invoke($2, `\u{1eae}`, []), [value("i32", 256)]);

// ./test/core/names.wast:872
assert_return(() => invoke($2, `\u{1eb0}`, []), [value("i32", 257)]);

// ./test/core/names.wast:873
assert_return(() => invoke($2, `\u{1eb2}`, []), [value("i32", 258)]);

// ./test/core/names.wast:874
assert_return(() => invoke($2, `\u{1eb4}`, []), [value("i32", 259)]);

// ./test/core/names.wast:875
assert_return(() => invoke($2, `\u{1eb6}`, []), [value("i32", 260)]);

// ./test/core/names.wast:876
assert_return(() => invoke($2, `\u{3042}`, []), [value("i32", 261)]);

// ./test/core/names.wast:877
assert_return(() => invoke($2, `\u{30a2}`, []), [value("i32", 262)]);

// ./test/core/names.wast:878
assert_return(() => invoke($2, `\u{311a}`, []), [value("i32", 263)]);

// ./test/core/names.wast:879
assert_return(() => invoke($2, `\u{314f}`, []), [value("i32", 264)]);

// ./test/core/names.wast:880
assert_return(() => invoke($2, `\u{320e}`, []), [value("i32", 265)]);

// ./test/core/names.wast:881
assert_return(() => invoke($2, `\u{320f}`, []), [value("i32", 266)]);

// ./test/core/names.wast:882
assert_return(() => invoke($2, `\u{3210}`, []), [value("i32", 267)]);

// ./test/core/names.wast:883
assert_return(() => invoke($2, `\u{3211}`, []), [value("i32", 268)]);

// ./test/core/names.wast:884
assert_return(() => invoke($2, `\u{3212}`, []), [value("i32", 269)]);

// ./test/core/names.wast:885
assert_return(() => invoke($2, `\u{3213}`, []), [value("i32", 270)]);

// ./test/core/names.wast:886
assert_return(() => invoke($2, `\u{3214}`, []), [value("i32", 271)]);

// ./test/core/names.wast:887
assert_return(() => invoke($2, `\u{3215}`, []), [value("i32", 272)]);

// ./test/core/names.wast:888
assert_return(() => invoke($2, `\u{3216}`, []), [value("i32", 273)]);

// ./test/core/names.wast:889
assert_return(() => invoke($2, `\u{3217}`, []), [value("i32", 274)]);

// ./test/core/names.wast:890
assert_return(() => invoke($2, `\u{3218}`, []), [value("i32", 275)]);

// ./test/core/names.wast:891
assert_return(() => invoke($2, `\u{3219}`, []), [value("i32", 276)]);

// ./test/core/names.wast:892
assert_return(() => invoke($2, `\u{321a}`, []), [value("i32", 277)]);

// ./test/core/names.wast:893
assert_return(() => invoke($2, `\u{321b}`, []), [value("i32", 278)]);

// ./test/core/names.wast:894
assert_return(() => invoke($2, `\u{326e}`, []), [value("i32", 279)]);

// ./test/core/names.wast:895
assert_return(() => invoke($2, `\u{326f}`, []), [value("i32", 280)]);

// ./test/core/names.wast:896
assert_return(() => invoke($2, `\u{3270}`, []), [value("i32", 281)]);

// ./test/core/names.wast:897
assert_return(() => invoke($2, `\u{3271}`, []), [value("i32", 282)]);

// ./test/core/names.wast:898
assert_return(() => invoke($2, `\u{3272}`, []), [value("i32", 283)]);

// ./test/core/names.wast:899
assert_return(() => invoke($2, `\u{3273}`, []), [value("i32", 284)]);

// ./test/core/names.wast:900
assert_return(() => invoke($2, `\u{3274}`, []), [value("i32", 285)]);

// ./test/core/names.wast:901
assert_return(() => invoke($2, `\u{3275}`, []), [value("i32", 286)]);

// ./test/core/names.wast:902
assert_return(() => invoke($2, `\u{3276}`, []), [value("i32", 287)]);

// ./test/core/names.wast:903
assert_return(() => invoke($2, `\u{3277}`, []), [value("i32", 288)]);

// ./test/core/names.wast:904
assert_return(() => invoke($2, `\u{3278}`, []), [value("i32", 289)]);

// ./test/core/names.wast:905
assert_return(() => invoke($2, `\u{3279}`, []), [value("i32", 290)]);

// ./test/core/names.wast:906
assert_return(() => invoke($2, `\u{327a}`, []), [value("i32", 291)]);

// ./test/core/names.wast:907
assert_return(() => invoke($2, `\u{327b}`, []), [value("i32", 292)]);

// ./test/core/names.wast:908
assert_return(() => invoke($2, `\u{32d0}`, []), [value("i32", 293)]);

// ./test/core/names.wast:909
assert_return(() => invoke($2, `\u{a00a}`, []), [value("i32", 294)]);

// ./test/core/names.wast:910
assert_return(() => invoke($2, `\u{a4ee}`, []), [value("i32", 295)]);

// ./test/core/names.wast:911
assert_return(() => invoke($2, `\u{a549}`, []), [value("i32", 296)]);

// ./test/core/names.wast:912
assert_return(() => invoke($2, `\u{a6a0}`, []), [value("i32", 297)]);

// ./test/core/names.wast:913
assert_return(() => invoke($2, `\u{a800}`, []), [value("i32", 298)]);

// ./test/core/names.wast:914
assert_return(() => invoke($2, `\u{a823}`, []), [value("i32", 299)]);

// ./test/core/names.wast:915
assert_return(() => invoke($2, `\u{a85d}`, []), [value("i32", 300)]);

// ./test/core/names.wast:916
assert_return(() => invoke($2, `\u{a882}`, []), [value("i32", 301)]);

// ./test/core/names.wast:917
assert_return(() => invoke($2, `\u{a8ea}`, []), [value("i32", 302)]);

// ./test/core/names.wast:918
assert_return(() => invoke($2, `\u{a922}`, []), [value("i32", 303)]);

// ./test/core/names.wast:919
assert_return(() => invoke($2, `\u{a946}`, []), [value("i32", 304)]);

// ./test/core/names.wast:920
assert_return(() => invoke($2, `\u{a984}`, []), [value("i32", 305)]);

// ./test/core/names.wast:921
assert_return(() => invoke($2, `\u{aa00}`, []), [value("i32", 306)]);

// ./test/core/names.wast:922
assert_return(() => invoke($2, `\u{ff71}`, []), [value("i32", 307)]);

// ./test/core/names.wast:923
assert_return(() => invoke($2, `\u{ffc2}`, []), [value("i32", 308)]);

// ./test/core/names.wast:924
assert_return(() => invoke($2, `\u{10000}`, []), [value("i32", 309)]);

// ./test/core/names.wast:925
assert_return(() => invoke($2, `\u{10280}`, []), [value("i32", 310)]);

// ./test/core/names.wast:926
assert_return(() => invoke($2, `\u{102a0}`, []), [value("i32", 311)]);

// ./test/core/names.wast:927
assert_return(() => invoke($2, `\u{10300}`, []), [value("i32", 312)]);

// ./test/core/names.wast:928
assert_return(() => invoke($2, `\u{103a0}`, []), [value("i32", 313)]);

// ./test/core/names.wast:929
assert_return(() => invoke($2, `\u{10496}`, []), [value("i32", 314)]);

// ./test/core/names.wast:930
assert_return(() => invoke($2, `\u{10500}`, []), [value("i32", 315)]);

// ./test/core/names.wast:931
assert_return(() => invoke($2, `\u{10740}`, []), [value("i32", 316)]);

// ./test/core/names.wast:932
assert_return(() => invoke($2, `\u{10800}`, []), [value("i32", 317)]);

// ./test/core/names.wast:933
assert_return(() => invoke($2, `\u{10920}`, []), [value("i32", 318)]);

// ./test/core/names.wast:934
assert_return(() => invoke($2, `\u{10980}`, []), [value("i32", 319)]);

// ./test/core/names.wast:935
assert_return(() => invoke($2, `\u{109a0}`, []), [value("i32", 320)]);

// ./test/core/names.wast:936
assert_return(() => invoke($2, `\u{10a00}`, []), [value("i32", 321)]);

// ./test/core/names.wast:937
assert_return(() => invoke($2, `\u{10b00}`, []), [value("i32", 322)]);

// ./test/core/names.wast:938
assert_return(() => invoke($2, `\u{10c00}`, []), [value("i32", 323)]);

// ./test/core/names.wast:939
assert_return(() => invoke($2, `\u{10c01}`, []), [value("i32", 324)]);

// ./test/core/names.wast:940
assert_return(() => invoke($2, `\u{10c80}`, []), [value("i32", 325)]);

// ./test/core/names.wast:941
assert_return(() => invoke($2, `\u{11005}`, []), [value("i32", 326)]);

// ./test/core/names.wast:942
assert_return(() => invoke($2, `\u{11083}`, []), [value("i32", 327)]);

// ./test/core/names.wast:943
assert_return(() => invoke($2, `\u{11127}`, []), [value("i32", 328)]);

// ./test/core/names.wast:944
assert_return(() => invoke($2, `\u{11150}`, []), [value("i32", 329)]);

// ./test/core/names.wast:945
assert_return(() => invoke($2, `\u{11183}`, []), [value("i32", 330)]);

// ./test/core/names.wast:946
assert_return(() => invoke($2, `\u{11200}`, []), [value("i32", 331)]);

// ./test/core/names.wast:947
assert_return(() => invoke($2, `\u{11280}`, []), [value("i32", 332)]);

// ./test/core/names.wast:948
assert_return(() => invoke($2, `\u{112b0}`, []), [value("i32", 333)]);

// ./test/core/names.wast:949
assert_return(() => invoke($2, `\u{11305}`, []), [value("i32", 334)]);

// ./test/core/names.wast:950
assert_return(() => invoke($2, `\u{11370}`, []), [value("i32", 335)]);

// ./test/core/names.wast:951
assert_return(() => invoke($2, `\u{11400}`, []), [value("i32", 336)]);

// ./test/core/names.wast:952
assert_return(() => invoke($2, `\u{11481}`, []), [value("i32", 337)]);

// ./test/core/names.wast:953
assert_return(() => invoke($2, `\u{11580}`, []), [value("i32", 338)]);

// ./test/core/names.wast:954
assert_return(() => invoke($2, `\u{11600}`, []), [value("i32", 339)]);

// ./test/core/names.wast:955
assert_return(() => invoke($2, `\u{11680}`, []), [value("i32", 340)]);

// ./test/core/names.wast:956
assert_return(() => invoke($2, `\u{11712}`, []), [value("i32", 341)]);

// ./test/core/names.wast:957
assert_return(() => invoke($2, `\u{11720}`, []), [value("i32", 342)]);

// ./test/core/names.wast:958
assert_return(() => invoke($2, `\u{118a1}`, []), [value("i32", 343)]);

// ./test/core/names.wast:959
assert_return(() => invoke($2, `\u{11ad5}`, []), [value("i32", 344)]);

// ./test/core/names.wast:960
assert_return(() => invoke($2, `\u{11c00}`, []), [value("i32", 345)]);

// ./test/core/names.wast:961
assert_return(() => invoke($2, `\u{11c8f}`, []), [value("i32", 346)]);

// ./test/core/names.wast:962
assert_return(() => invoke($2, `\u{11caf}`, []), [value("i32", 347)]);

// ./test/core/names.wast:963
assert_return(() => invoke($2, `\u{12000}`, []), [value("i32", 348)]);

// ./test/core/names.wast:964
assert_return(() => invoke($2, `\u{169d5}`, []), [value("i32", 349)]);

// ./test/core/names.wast:965
assert_return(() => invoke($2, `\u{16a46}`, []), [value("i32", 350)]);

// ./test/core/names.wast:966
assert_return(() => invoke($2, `\u{16ae7}`, []), [value("i32", 351)]);

// ./test/core/names.wast:967
assert_return(() => invoke($2, `\u{16f54}`, []), [value("i32", 352)]);

// ./test/core/names.wast:968
assert_return(() => invoke($2, `\u{1bc41}`, []), [value("i32", 353)]);

// ./test/core/names.wast:969
assert_return(() => invoke($2, `\u{1bc64}`, []), [value("i32", 354)]);

// ./test/core/names.wast:970
assert_return(() => invoke($2, `\u{1e823}`, []), [value("i32", 355)]);

// ./test/core/names.wast:971
assert_return(() => invoke($2, `\u{1f1e6}`, []), [value("i32", 356)]);

// ./test/core/names.wast:972
assert_return(() => invoke($2, `\u{2c6d}`, []), [value("i32", 357)]);

// ./test/core/names.wast:973
assert_return(() => invoke($2, `\u{39b}`, []), [value("i32", 358)]);

// ./test/core/names.wast:974
assert_return(() => invoke($2, `\u{2c70}`, []), [value("i32", 359)]);

// ./test/core/names.wast:975
assert_return(() => invoke($2, `\u{aa}`, []), [value("i32", 360)]);

// ./test/core/names.wast:976
assert_return(() => invoke($2, `\u{2200}`, []), [value("i32", 361)]);

// ./test/core/names.wast:977
assert_return(() => invoke($2, `\u{20b3}`, []), [value("i32", 362)]);

// ./test/core/names.wast:978
assert_return(() => invoke($2, `\u{10900}`, []), [value("i32", 363)]);

// ./test/core/names.wast:979
assert_return(() => invoke($2, `\u{2c80}`, []), [value("i32", 364)]);

// ./test/core/names.wast:980
assert_return(() => invoke($2, `\u{10330}`, []), [value("i32", 365)]);

// ./test/core/names.wast:981
assert_return(() => invoke($2, `\u{386}`, []), [value("i32", 366)]);

// ./test/core/names.wast:982
assert_return(() => invoke($2, `\u{391}`, []), [value("i32", 367)]);

// ./test/core/names.wast:983
assert_return(() => invoke($2, `\u{1f08}`, []), [value("i32", 368)]);

// ./test/core/names.wast:984
assert_return(() => invoke($2, `\u{1f09}`, []), [value("i32", 369)]);

// ./test/core/names.wast:985
assert_return(() => invoke($2, `\u{1f0a}`, []), [value("i32", 370)]);

// ./test/core/names.wast:986
assert_return(() => invoke($2, `\u{1f0b}`, []), [value("i32", 371)]);

// ./test/core/names.wast:987
assert_return(() => invoke($2, `\u{1f0c}`, []), [value("i32", 372)]);

// ./test/core/names.wast:988
assert_return(() => invoke($2, `\u{1f0d}`, []), [value("i32", 373)]);

// ./test/core/names.wast:989
assert_return(() => invoke($2, `\u{1f0e}`, []), [value("i32", 374)]);

// ./test/core/names.wast:990
assert_return(() => invoke($2, `\u{1f0f}`, []), [value("i32", 375)]);

// ./test/core/names.wast:991
assert_return(() => invoke($2, `\u{1f88}`, []), [value("i32", 376)]);

// ./test/core/names.wast:992
assert_return(() => invoke($2, `\u{1f89}`, []), [value("i32", 377)]);

// ./test/core/names.wast:993
assert_return(() => invoke($2, `\u{1f8a}`, []), [value("i32", 378)]);

// ./test/core/names.wast:994
assert_return(() => invoke($2, `\u{1f8b}`, []), [value("i32", 379)]);

// ./test/core/names.wast:995
assert_return(() => invoke($2, `\u{1f8c}`, []), [value("i32", 380)]);

// ./test/core/names.wast:996
assert_return(() => invoke($2, `\u{1f8d}`, []), [value("i32", 381)]);

// ./test/core/names.wast:997
assert_return(() => invoke($2, `\u{1f8e}`, []), [value("i32", 382)]);

// ./test/core/names.wast:998
assert_return(() => invoke($2, `\u{1f8f}`, []), [value("i32", 383)]);

// ./test/core/names.wast:999
assert_return(() => invoke($2, `\u{1fb8}`, []), [value("i32", 384)]);

// ./test/core/names.wast:1000
assert_return(() => invoke($2, `\u{1fb9}`, []), [value("i32", 385)]);

// ./test/core/names.wast:1001
assert_return(() => invoke($2, `\u{1fba}`, []), [value("i32", 386)]);

// ./test/core/names.wast:1002
assert_return(() => invoke($2, `\u{1fbb}`, []), [value("i32", 387)]);

// ./test/core/names.wast:1003
assert_return(() => invoke($2, `\u{1fbc}`, []), [value("i32", 388)]);

// ./test/core/names.wast:1004
assert_return(() => invoke($2, `\u{1d6a8}`, []), [value("i32", 389)]);

// ./test/core/names.wast:1005
assert_return(() => invoke($2, `\u{1d6e2}`, []), [value("i32", 390)]);

// ./test/core/names.wast:1006
assert_return(() => invoke($2, `\u{1d71c}`, []), [value("i32", 391)]);

// ./test/core/names.wast:1007
assert_return(() => invoke($2, `\u{1d756}`, []), [value("i32", 392)]);

// ./test/core/names.wast:1008
assert_return(() => invoke($2, `\u{1d790}`, []), [value("i32", 393)]);

// ./test/core/names.wast:1009
assert_return(() => invoke($2, `\u{2376}`, []), [value("i32", 394)]);

// ./test/core/names.wast:1010
assert_return(() => invoke($2, `\u{237a}`, []), [value("i32", 395)]);

// ./test/core/names.wast:1011
assert_return(() => invoke($2, `\u{2a5c}`, []), [value("i32", 396)]);

// ./test/core/names.wast:1012
assert_return(() => invoke($2, `\u{15c5}`, []), [value("i32", 397)]);

// ./test/core/names.wast:1013
assert_return(() => invoke($2, `\u{13aa}`, []), [value("i32", 398)]);

// ./test/core/names.wast:1014
assert_return(
  () => invoke($2,
    `)\u{2fa}\u{2fc}\u{145cf}\u{1d174}\u{1d176}\u{1d178}\u{1d17a}\u{207e}\u{208e}\u{2769}\u{276b}\u{27ef}\u{fd3f}\u{fe36}\u{fe5a}\u{ff09}\u{ff60}\u{e0029}\u{2773}\u{2775}\u{27e7}\u{27e9}\u{27eb}\u{27ed}\u{2988}\u{298a}\u{2996}\u{2e23}\u{2e25}\u{fe18}\u{fe38}\u{fe3a}\u{fe3c}\u{fe3e}\u{fe40}\u{fe42}\u{fe44}\u{fe48}\u{fe5c}\u{fe5e}\u{ff3d}\u{ff5d}\u{ff63}\u{e005d}\u{e007d}\u{bb}\u{2019}\u{201d}\u{203a}\u{276f}`,
    [],
  ),
  [value("i32", 399)],
);

// ./test/core/names.wast:1015
assert_return(
  () => invoke($2,
    `(\u{2f9}\u{2fb}\u{145ce}\u{1d173}\u{1d175}\u{1d177}\u{1d179}\u{207d}\u{208d}\u{2768}\u{276a}\u{27ee}\u{fd3e}\u{fe35}\u{fe59}\u{ff08}\u{ff5f}\u{e0028}\u{2772}\u{2774}\u{27e6}\u{27e8}\u{27ea}\u{27ec}\u{2987}\u{2989}\u{2995}\u{2e22}\u{2e24}\u{fe17}\u{fe37}\u{fe39}\u{fe3b}\u{fe3d}\u{fe3f}\u{fe41}\u{fe43}\u{fe47}\u{fe5b}\u{fe5d}\u{ff3b}\u{ff5b}\u{ff62}\u{e005b}\u{e007b}\u{ab}\u{2018}\u{201c}\u{2039}\u{276e}`,
    [],
  ),
  [value("i32", 400)],
);

// ./test/core/names.wast:1016
assert_return(() => invoke($2, `\u{1da8b}\u{1daa4}`, []), [value("i32", 401)]);

// ./test/core/names.wast:1017
assert_return(() => invoke($2, `\u{1da8b}`, []), [value("i32", 402)]);

// ./test/core/names.wast:1018
assert_return(() => invoke($2, `\u{bd}`, []), [value("i32", 403)]);

// ./test/core/names.wast:1019
assert_return(() => invoke($2, `1\u{2044}2`, []), [value("i32", 404)]);

// ./test/core/names.wast:1020
assert_return(() => invoke($2, `1/2`, []), [value("i32", 405)]);

// ./test/core/names.wast:1021
assert_return(() => invoke($2, `\u{b73}`, []), [value("i32", 406)]);

// ./test/core/names.wast:1022
assert_return(() => invoke($2, `\u{d74}`, []), [value("i32", 407)]);

// ./test/core/names.wast:1023
assert_return(() => invoke($2, `\u{2cfd}`, []), [value("i32", 408)]);

// ./test/core/names.wast:1024
assert_return(() => invoke($2, `\u{a831}`, []), [value("i32", 409)]);

// ./test/core/names.wast:1025
assert_return(() => invoke($2, `\u{10141}`, []), [value("i32", 410)]);

// ./test/core/names.wast:1026
assert_return(() => invoke($2, `\u{10175}`, []), [value("i32", 411)]);

// ./test/core/names.wast:1027
assert_return(() => invoke($2, `\u{10176}`, []), [value("i32", 412)]);

// ./test/core/names.wast:1028
assert_return(() => invoke($2, `\u{109bd}`, []), [value("i32", 413)]);

// ./test/core/names.wast:1029
assert_return(() => invoke($2, `\u{10e7b}`, []), [value("i32", 414)]);

// ./test/core/names.wast:1030
assert_return(() => invoke($2, `\u{ff02}`, []), [value("i32", 415)]);

// ./test/core/names.wast:1031
assert_return(() => invoke($2, `\x7f`, []), [value("i32", 416)]);

// ./test/core/names.wast:1032
assert_return(() => invoke($2, `\x08`, []), [value("i32", 417)]);

// ./test/core/names.wast:1033
assert_return(() => invoke($2, `\u{232b}`, []), [value("i32", 418)]);

// ./test/core/names.wast:1034
assert_return(() => invoke($2, `\u{2326}`, []), [value("i32", 419)]);

// ./test/core/names.wast:1035
assert_return(() => invoke($2, `\u{2408}`, []), [value("i32", 420)]);

// ./test/core/names.wast:1036
assert_return(() => invoke($2, `\u{2421}`, []), [value("i32", 421)]);

// ./test/core/names.wast:1037
assert_return(() => invoke($2, `\u{1dfb}`, []), [value("i32", 422)]);

// ./test/core/names.wast:1038
assert_return(() => invoke($2, `\x0f`, []), [value("i32", 423)]);

// ./test/core/names.wast:1039
assert_return(() => invoke($2, `\u{2190}`, []), [value("i32", 424)]);

// ./test/core/names.wast:1040
assert_return(() => invoke($2, `\u{2327}`, []), [value("i32", 425)]);

// ./test/core/names.wast:1041
assert_return(() => invoke($2, `\u{2352}`, []), [value("i32", 426)]);

// ./test/core/names.wast:1042
assert_return(() => invoke($2, `\u{2354}`, []), [value("i32", 427)]);

// ./test/core/names.wast:1043
assert_return(() => invoke($2, `\u{2362}`, []), [value("i32", 428)]);

// ./test/core/names.wast:1044
assert_return(() => invoke($2, `\u{236b}`, []), [value("i32", 429)]);

// ./test/core/names.wast:1045
assert_return(() => invoke($2, `\x1a`, []), [value("i32", 430)]);

// ./test/core/names.wast:1046
assert_return(() => invoke($2, `\u{2426}`, []), [value("i32", 431)]);

// ./test/core/names.wast:1047
assert_return(() => invoke($2, `\u{241a}`, []), [value("i32", 432)]);

// ./test/core/names.wast:1048
assert_return(() => invoke($2, `\u{fffc}`, []), [value("i32", 433)]);

// ./test/core/names.wast:1049
assert_return(() => invoke($2, `?`, []), [value("i32", 434)]);

// ./test/core/names.wast:1050
assert_return(() => invoke($2, `\u{bf}`, []), [value("i32", 435)]);

// ./test/core/names.wast:1051
assert_return(() => invoke($2, `\u{1945}`, []), [value("i32", 436)]);

// ./test/core/names.wast:1052
assert_return(() => invoke($2, `\u{37e}`, []), [value("i32", 437)]);

// ./test/core/names.wast:1053
assert_return(() => invoke($2, `\u{55e}`, []), [value("i32", 438)]);

// ./test/core/names.wast:1054
assert_return(() => invoke($2, `\u{61f}`, []), [value("i32", 439)]);

// ./test/core/names.wast:1055
assert_return(() => invoke($2, `\u{1367}`, []), [value("i32", 440)]);

// ./test/core/names.wast:1056
assert_return(() => invoke($2, `\u{2047}`, []), [value("i32", 441)]);

// ./test/core/names.wast:1057
assert_return(() => invoke($2, `\u{2370}`, []), [value("i32", 442)]);

// ./test/core/names.wast:1058
assert_return(() => invoke($2, `\u{2753}`, []), [value("i32", 443)]);

// ./test/core/names.wast:1059
assert_return(() => invoke($2, `\u{2754}`, []), [value("i32", 444)]);

// ./test/core/names.wast:1060
assert_return(() => invoke($2, `\u{2cfa}`, []), [value("i32", 445)]);

// ./test/core/names.wast:1061
assert_return(() => invoke($2, `\u{2cfb}`, []), [value("i32", 446)]);

// ./test/core/names.wast:1062
assert_return(() => invoke($2, `\u{2e2e}`, []), [value("i32", 447)]);

// ./test/core/names.wast:1063
assert_return(() => invoke($2, `\u{3244}`, []), [value("i32", 448)]);

// ./test/core/names.wast:1064
assert_return(() => invoke($2, `\u{a60f}`, []), [value("i32", 449)]);

// ./test/core/names.wast:1065
assert_return(() => invoke($2, `\u{a6f7}`, []), [value("i32", 450)]);

// ./test/core/names.wast:1066
assert_return(() => invoke($2, `\u{fe16}`, []), [value("i32", 451)]);

// ./test/core/names.wast:1067
assert_return(() => invoke($2, `\u{fe56}`, []), [value("i32", 452)]);

// ./test/core/names.wast:1068
assert_return(() => invoke($2, `\u{ff1f}`, []), [value("i32", 453)]);

// ./test/core/names.wast:1069
assert_return(() => invoke($2, `\u{11143}`, []), [value("i32", 454)]);

// ./test/core/names.wast:1070
assert_return(() => invoke($2, `\u{1e95f}`, []), [value("i32", 455)]);

// ./test/core/names.wast:1071
assert_return(() => invoke($2, `\u{e003f}`, []), [value("i32", 456)]);

// ./test/core/names.wast:1072
assert_return(() => invoke($2, `\u{16844}`, []), [value("i32", 457)]);

// ./test/core/names.wast:1073
assert_return(() => invoke($2, `\u{2bd1}`, []), [value("i32", 458)]);

// ./test/core/names.wast:1074
assert_return(() => invoke($2, `\u{b6}`, []), [value("i32", 459)]);

// ./test/core/names.wast:1075
assert_return(() => invoke($2, `\u{204b}`, []), [value("i32", 460)]);

// ./test/core/names.wast:1076
assert_return(() => invoke($2, `\u{700}`, []), [value("i32", 461)]);

// ./test/core/names.wast:1077
assert_return(() => invoke($2, `\u{10fb}`, []), [value("i32", 462)]);

// ./test/core/names.wast:1078
assert_return(() => invoke($2, `\u{1368}`, []), [value("i32", 463)]);

// ./test/core/names.wast:1079
assert_return(() => invoke($2, `\u{3037}`, []), [value("i32", 464)]);

// ./test/core/names.wast:1080
assert_return(() => invoke($2, `\u{2761}`, []), [value("i32", 465)]);

// ./test/core/names.wast:1081
assert_return(() => invoke($2, `\u{2e0f}`, []), [value("i32", 466)]);

// ./test/core/names.wast:1082
assert_return(() => invoke($2, `\u{2e10}`, []), [value("i32", 467)]);

// ./test/core/names.wast:1083
assert_return(() => invoke($2, `\u{2e11}`, []), [value("i32", 468)]);

// ./test/core/names.wast:1084
assert_return(() => invoke($2, `\u{2e0e}`, []), [value("i32", 469)]);

// ./test/core/names.wast:1085
assert_return(() => invoke($2, `\x14`, []), [value("i32", 470)]);

// ./test/core/names.wast:1086
assert_return(() => invoke($2, `\u{2619}`, []), [value("i32", 471)]);

// ./test/core/names.wast:1087
assert_return(() => invoke($2, `\u{2e3f}`, []), [value("i32", 472)]);

// ./test/core/names.wast:1088
assert_return(() => invoke($2, `\u{3007}`, []), [value("i32", 473)]);

// ./test/core/names.wast:1089
assert_return(() => invoke($2, `\u{e5b}`, []), [value("i32", 474)]);

// ./test/core/names.wast:1090
assert_return(() => invoke($2, `\u{a66e}`, []), [value("i32", 475)]);

// ./test/core/names.wast:1091
assert_return(() => invoke($2, `\u{3d3}`, []), [value("i32", 476)]);

// ./test/core/names.wast:1092
assert_return(() => invoke($2, `\u{3d4}`, []), [value("i32", 477)]);

// ./test/core/names.wast:1093
assert_return(() => invoke($2, `\u{1e9b}`, []), [value("i32", 478)]);

// ./test/core/names.wast:1095
let $3 = instantiate(`(module
  ;; Test that we can use indices instead of names to reference imports,
  ;; exports, functions and parameters.
  (import "spectest" "print_i32" (func (param i32)))
  (func (import "spectest" "print_i32") (param i32))
  (func (param i32) (param i32)
    (call 0 (local.get 0))
    (call 1 (local.get 1))
  )
  (export "print32" (func 2))
)`);

// ./test/core/names.wast:1107
assert_return(() => invoke($3, `print32`, [42, 123]), []);
