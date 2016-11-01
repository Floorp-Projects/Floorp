if (typeof parseRegExp === 'undefined')
    quit();

load(libdir + "regexp_parse.js");

test_mix("\\x00", all_flags,
         Atom("\u0000"));
test_mix("\\xFF", all_flags,
         Atom("\u00FF"));

test_mix("\\x0", no_unicode_flags,
         Atom("x0"));
test_mix("\\x000", all_flags,
         Atom("\u{0000}0"));

test_mix("\\xG", no_unicode_flags,
         Atom("xG"));
test_mix("\\x0G", no_unicode_flags,
         Atom("x0G"));
