if (typeof parseRegExp === 'undefined')
    quit();

load(libdir + "regexp_parse.js");

test_mix("\\0", all_flags,
         Atom("\u0000"));
