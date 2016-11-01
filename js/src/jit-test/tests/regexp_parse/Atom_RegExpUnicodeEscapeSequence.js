if (typeof parseRegExp === 'undefined')
    quit();

load(libdir + "regexp_parse.js");

// LeadSurrogate TrailSurrogate

test("\\uD83D\\uDC38", all_flags,
     Atom("\uD83D\uDC38"));
test("X\\uD83D\\uDC38Y", no_unicode_flags,
     Atom("X\uD83D\uDC38Y"));
test("X\\uD83D\\uDC38Y", unicode_flags,
     Text([
         Atom("X"),
         Atom("\uD83D\uDC38"),
         Atom("Y")
     ]));

// LeadSurrogate

test_mix("\\uD83D", no_unicode_flags,
         Atom("\uD83D"));
test("\\uD83D", unicode_flags,
     Alternative([
         Atom("\uD83D"),
         NegativeLookahead(CharacterClass([["\uDC00", "\uDFFF"]]))
     ]));
test("X\\uD83DY", unicode_flags,
     Alternative([
         Atom("X"),
         Alternative([
             Atom("\uD83D"),
             NegativeLookahead(CharacterClass([["\uDC00", "\uDFFF"]]))
         ]),
         Atom("Y")
     ]));

// TrailSurrogate

test_mix("\\uDC38", no_unicode_flags,
         Atom("\uDC38"));
test("\\uDC38", unicode_flags,
     Alternative([
         Assertion("NOT_AFTER_LEAD_SURROGATE"),
         Atom("\uDC38"),
     ]));
test("X\\uDC38Y", unicode_flags,
     Alternative([
         Atom("X"),
         Alternative([
             Assertion("NOT_AFTER_LEAD_SURROGATE"),
             Atom("\uDC38"),
         ]),
         Atom("Y")
     ]));

// NonSurrogate / Hex4Digits

test_mix("\\u0000", all_flags,
         Atom("\u0000"));
test_mix("\\uFFFF", all_flags,
         Atom("\uFFFF"));

// braced HexDigits

test_mix("\\u{0000}", unicode_flags,
         Atom("\u0000"));
test_mix("\\u{FFFF}", unicode_flags,
         Atom("\uFFFF"));

test("\\u{1F438}", unicode_flags,
     Atom("\uD83D\uDC38"));
test("X\\u{1F438}Y", unicode_flags,
     Text([
         Atom("X"),
         Atom("\uD83D\uDC38"),
         Atom("Y")
     ]));

test("\\u{D83D}", unicode_flags,
     Alternative([
         Atom("\uD83D"),
         NegativeLookahead(CharacterClass([["\uDC00", "\uDFFF"]]))
     ]));
test("X\\u{D83D}Y", unicode_flags,
     Alternative([
         Atom("X"),
         Alternative([
             Atom("\uD83D"),
             NegativeLookahead(CharacterClass([["\uDC00", "\uDFFF"]]))
         ]),
         Atom("Y")
     ]));

test("\\u{DC38}", unicode_flags,
     Alternative([
         Assertion("NOT_AFTER_LEAD_SURROGATE"),
         Atom("\uDC38"),
     ]));
test("X\\u{DC38}Y", unicode_flags,
     Alternative([
         Atom("X"),
         Alternative([
             Assertion("NOT_AFTER_LEAD_SURROGATE"),
             Atom("\uDC38"),
         ]),
         Atom("Y")
     ]));
