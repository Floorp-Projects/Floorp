if (typeof parseRegExp === 'undefined')
    quit();

load(libdir + "regexp_parse.js");

// SyntaxCharacter

test("\\^", all_flags,
     Atom("^"));
test("\\$", all_flags,
     Atom("$"));
test("\\\\", all_flags,
     Atom("\\"));
test("\\.", all_flags,
     Atom("."));
test("\\*", all_flags,
     Atom("*"));
test("\\+", all_flags,
     Atom("+"));
test("\\?", all_flags,
     Atom("?"));
test("\\(", all_flags,
     Atom("("));
test("\\)", all_flags,
     Atom(")"));
test("\\[", all_flags,
     Atom("["));
test("\\]", all_flags,
     Atom("]"));
test("\\{", all_flags,
     Atom("{"));
test("\\}", all_flags,
     Atom("}"));
test("\\|", all_flags,
     Atom("|"));

// Slash

test("\\/", all_flags,
     Atom("/"));

// SourceCharacter

test("\\P", no_unicode_flags,
     Atom("P"));

test("\\uX", no_unicode_flags,
     Atom("uX"));

test("\\u{0000}", no_unicode_flags,
     Quantifier(0, 0, "GREEDY", Atom("u")));

test("\\c_", no_unicode_flags,
     Atom("\\c_"));

