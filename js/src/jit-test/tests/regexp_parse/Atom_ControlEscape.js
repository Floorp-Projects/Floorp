if (typeof parseRegExp === 'undefined')
    quit();

load(libdir + "regexp_parse.js");

test_mix("\\f", all_flags,
         Atom("\u000c"));

test_mix("\\n", all_flags,
         Atom("\u000a"));

test_mix("\\r", all_flags,
         Atom("\u000d"));

test_mix("\\t", all_flags,
         Atom("\u0009"));

test_mix("\\v", all_flags,
         Atom("\u000b"));
