if (typeof parseRegExp === 'undefined')
    quit();

load(libdir + "regexp_parse.js");

// LeadSurrogate TrailSurrogate

test("[X\\uD83D\\uDC38Y]", no_unicode_flags,
     CharacterClass([
         ["X", "X"],
         ["\uD83D", "\uD83D"],
         ["\uDC38", "\uDC38"],
         ["Y", "Y"]
     ]));
test("[X\\uD83D\\uDC38Y]", unicode_flags,
     Disjunction([
         CharacterClass([
             ["X", "X"],
             ["Y", "Y"],
         ]),
         Atom("\uD83D\uDC38")
     ]));

// LeadSurrogate

test("[X\\uD83DY]", no_unicode_flags,
     CharacterClass([
         ["X", "X"],
         ["\uD83D", "\uD83D"],
         ["Y", "Y"]
     ]));
test("[X\\uD83DY]", unicode_flags,
     Disjunction([
         CharacterClass([
             ["X", "X"],
             ["Y", "Y"]
         ]),
         Alternative([
             CharacterClass([
                 ["\uD83D", "\uD83D"]
             ]),
             NegativeLookahead(CharacterClass([["\uDC00", "\uDFFF"]]))
         ])
     ]));

// TrailSurrogate

test("[X\\uDC38Y]", no_unicode_flags,
     CharacterClass([
         ["X", "X"],
         ["\uDC38", "\uDC38"],
         ["Y", "Y"]
     ]));
test("[X\\uDC38Y]", unicode_flags,
     Disjunction([
         CharacterClass([
             ["X", "X"],
             ["Y", "Y"]
         ]),
         Alternative([
             Assertion("NOT_AFTER_LEAD_SURROGATE"),
             CharacterClass([
                 ["\uDC38", "\uDC38"]
             ])
         ])
     ]));

// NonSurrogate / Hex4Digits

test("[X\\u0000Y]", all_flags,
     CharacterClass([
         ["X", "X"],
         ["\u0000", "\u0000"],
         ["Y", "Y"]
     ]));
test("[X\\uFFFFY]", all_flags,
     CharacterClass([
         ["X", "X"],
         ["\uFFFF", "\uFFFF"],
         ["Y", "Y"]
     ]));

// braced HexDigits

test("[X\\u{0000}Y]", unicode_flags,
     CharacterClass([
         ["X", "X"],
         ["\u0000", "\u0000"],
         ["Y", "Y"]
     ]));
test("[X\\uFFFFY]", unicode_flags,
     CharacterClass([
         ["X", "X"],
         ["\uFFFF", "\uFFFF"],
         ["Y", "Y"]
     ]));

test("[X\\u{1F438}Y]", unicode_flags,
     Disjunction([
         CharacterClass([
             ["X", "X"],
             ["Y", "Y"],
         ]),
         Atom("\uD83D\uDC38")
     ]));

test("[X\\u{D83D}Y]", unicode_flags,
     Disjunction([
         CharacterClass([
             ["X", "X"],
             ["Y", "Y"]
         ]),
         Alternative([
             CharacterClass([
                 ["\uD83D", "\uD83D"]
             ]),
             NegativeLookahead(CharacterClass([["\uDC00", "\uDFFF"]]))
         ])
     ]));
test("[X\\u{DC38}Y]", unicode_flags,
     Disjunction([
         CharacterClass([
             ["X", "X"],
             ["Y", "Y"]
         ]),
         Alternative([
             Assertion("NOT_AFTER_LEAD_SURROGATE"),
             CharacterClass([
                 ["\uDC38", "\uDC38"]
             ])
         ])
     ]));

// Invalid

test("[\\u]", no_unicode_flags,
     CharacterClass([
         ["u", "u"]
     ]));

test("[\\uG]", no_unicode_flags,
     CharacterClass([
         ["u", "u"],
         ["G", "G"]
     ]));

test("[\\uD83]", no_unicode_flags,
     CharacterClass([
         ["u", "u"],
         ["D", "D"],
         ["8", "8"],
         ["3", "3"]
     ]));

test("[\\uD83G]", no_unicode_flags,
     CharacterClass([
         ["u", "u"],
         ["D", "D"],
         ["8", "8"],
         ["3", "3"],
         ["G", "G"]
     ]));
