if (typeof parseRegExp === 'undefined')
    quit();

load(libdir + "regexp_parse.js");

test_mix("^", no_multiline_flags,
         Assertion("START_OF_INPUT"));
test_mix("^", multiline_flags,
         Assertion("START_OF_LINE"));

test_mix("$", no_multiline_flags,
         Assertion("END_OF_INPUT"));
test_mix("$", multiline_flags,
         Assertion("END_OF_LINE"));

test_mix("\\b", all_flags,
         Assertion("BOUNDARY"));

test_mix("\\B", all_flags,
         Assertion("NON_BOUNDARY"));
