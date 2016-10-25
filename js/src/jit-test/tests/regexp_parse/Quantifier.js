if (typeof parseRegExp === 'undefined')
    quit();

load(libdir + "regexp_parse.js");

test_mix("a*", all_flags,
         Quantifier(0, kInfinity, "GREEDY", Atom("a")));
test_mix("a*?", all_flags,
         Quantifier(0, kInfinity, "NON_GREEDY", Atom("a")));

test_mix("a+", all_flags,
         Quantifier(1, kInfinity, "GREEDY", Atom("a")));
test_mix("a+?", all_flags,
         Quantifier(1, kInfinity, "NON_GREEDY", Atom("a")));

test_mix("a?", all_flags,
         Quantifier(0, 1, "GREEDY", Atom("a")));
test_mix("a??", all_flags,
         Quantifier(0, 1, "NON_GREEDY", Atom("a")));

test_mix("a{3}", all_flags,
         Quantifier(3, 3, "GREEDY", Atom("a")));
test_mix("a{3}?", all_flags,
         Quantifier(3, 3, "NON_GREEDY", Atom("a")));

test_mix("a{3,}", all_flags,
         Quantifier(3, kInfinity, "GREEDY", Atom("a")));
test_mix("a{3,}?", all_flags,
         Quantifier(3, kInfinity, "NON_GREEDY", Atom("a")));

test_mix("a{3,5}", all_flags,
         Quantifier(3, 5, "GREEDY", Atom("a")));
test_mix("a{3,5}?", all_flags,
         Quantifier(3, 5, "NON_GREEDY", Atom("a")));

// Surrogate Pair and Quantifier

test("\\uD83D\\uDC38+", no_unicode_flags,
     Alternative([
         Atom("\uD83D"),
         Quantifier(1, kInfinity, "GREEDY", Atom("\uDC38"))
     ]));
test("\\uD83D\\uDC38+", unicode_flags,
     Quantifier(1, kInfinity, "GREEDY", Atom("\uD83D\uDC38")));

// Invalid

test_mix("a{", no_unicode_flags,
         Atom("a{"));
test_mix("a{1", no_unicode_flags,
         Atom("a{1"));
test_mix("a{1,", no_unicode_flags,
         Atom("a{1,"));
test_mix("a{1,2", no_unicode_flags,
         Atom("a{1,2"));

test_mix("a{,", no_unicode_flags,
         Atom("a{,"));
