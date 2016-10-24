if (typeof parseRegExp === 'undefined')
    quit();

load(libdir + "regexp_parse.js");

// LegacyOctalEscapeSequence

test_mix("\\1", no_unicode_flags,
         Atom("\u0001"));
test_mix("\\2", no_unicode_flags,
         Atom("\u0002"));
test_mix("\\3", no_unicode_flags,
         Atom("\u0003"));
test_mix("\\4", no_unicode_flags,
         Atom("\u0004"));
test_mix("\\5", no_unicode_flags,
         Atom("\u0005"));
test_mix("\\6", no_unicode_flags,
         Atom("\u0006"));
test_mix("\\7", no_unicode_flags,
         Atom("\u0007"));
test_mix("\\8", no_unicode_flags,
         Atom("8"));
test_mix("\\9", no_unicode_flags,
         Atom("9"));

test_mix("\\10", no_unicode_flags,
         Atom("\u0008"));
test_mix("\\11", no_unicode_flags,
         Atom("\u0009"));

test_mix("\\189", no_unicode_flags,
         Atom("\u{0001}89"));
test_mix("\\1089", no_unicode_flags,
         Atom("\u{0008}89"));
test_mix("\\10189", no_unicode_flags,
         Atom("A89"));
test_mix("\\101189", no_unicode_flags,
         Atom("A189"));

// BackReference

test_mix("()\\1", no_unicode_flags,
         Alternative([
             Capture(1, Empty()),
             BackReference(1)
         ]));
test_mix("()\\1", unicode_flags,
         Alternative([
             Capture(1, Empty()),
             Alternative([
                 BackReference(1),
                 Assertion("NOT_IN_SURROGATE_PAIR")
             ])
         ]));

test_mix("()()()()()()()()()()\\10", no_unicode_flags,
         Alternative([
             Capture(1, Empty()),
             Capture(2, Empty()),
             Capture(3, Empty()),
             Capture(4, Empty()),
             Capture(5, Empty()),
             Capture(6, Empty()),
             Capture(7, Empty()),
             Capture(8, Empty()),
             Capture(9, Empty()),
             Capture(10, Empty()),
             BackReference(10)
         ]));
test_mix("()()()()()()()()()()\\10", unicode_flags,
         Alternative([
             Capture(1, Empty()),
             Capture(2, Empty()),
             Capture(3, Empty()),
             Capture(4, Empty()),
             Capture(5, Empty()),
             Capture(6, Empty()),
             Capture(7, Empty()),
             Capture(8, Empty()),
             Capture(9, Empty()),
             Capture(10, Empty()),
             Alternative([
                 BackReference(10),
                 Assertion("NOT_IN_SURROGATE_PAIR")
             ])
         ]));
