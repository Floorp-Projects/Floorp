if (typeof parseRegExp === 'undefined')
    quit();

load(libdir + "regexp_parse.js");

test_mix("a", all_flags,
         Atom("a"));
test_mix("abc\u3042\u3044\u3046", all_flags,
         Atom("abc\u3042\u3044\u3046"));

// raw brace

test("{", no_unicode_flags,
     Atom("{"));
test("{a", no_unicode_flags,
     Atom("{a"));
test("a{b", no_unicode_flags,
     Atom("a{b"));

test("}", no_unicode_flags,
     Atom("}"));
test("}a", no_unicode_flags,
     Atom("}a"));
test("a}b", no_unicode_flags,
     Atom("a}b"));

// raw surrogate pair

test("X\uD83D\uDC38Y", unicode_flags,
     Text([
         Atom("X"),
         Atom("\uD83D\uDC38"),
         Atom("Y")
     ]));

test("X\uD83DY", unicode_flags,
     Alternative([
         Atom("X"),
         Alternative([
             Atom("\uD83D"),
             NegativeLookahead(CharacterClass([["\uDC00", "\uDFFF"]]))
         ]),
         Atom("Y")
     ]));

test("X\uDC38Y", unicode_flags,
     Alternative([
         Atom("X"),
         Alternative([
             Assertion("NOT_AFTER_LEAD_SURROGATE"),
             Atom("\uDC38"),
         ]),
         Atom("Y")
     ]));
