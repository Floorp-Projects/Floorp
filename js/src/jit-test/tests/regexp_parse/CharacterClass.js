if (typeof parseRegExp === 'undefined')
    quit();

load(libdir + "regexp_parse.js");

test_mix("[]", all_flags,
         NegativeCharacterClass([
             ["\u0000", "\uFFFF"]
         ]));

test("[a]", all_flags,
     CharacterClass([
         ["a", "a"]
     ]));

test("[abc\u3042\u3044\u3046]", all_flags,
     CharacterClass([
         ["a", "a"],
         ["b", "b"],
         ["c", "c"],
         ["\u3042", "\u3042"],
         ["\u3044", "\u3044"],
         ["\u3046", "\u3046"],
     ]));

test("[a-c\u3042-\u3046]", all_flags,
     CharacterClass([
         ["a", "c"],
         ["\u3042", "\u3046"]
     ]));

test("[-]", all_flags,
     CharacterClass([
         ["-", "-"]
     ]));

// raw surrogate pair

test("[X\uD83D\uDC38Y]", unicode_flags,
     Disjunction([
         CharacterClass([
             ["X", "X"],
             ["Y", "Y"],
         ]),
         Atom("\uD83D\uDC38")
     ]));

test("[X\uD83DY]", unicode_flags,
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

test("[X\uDC38Y]", unicode_flags,
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
