if (typeof parseRegExp === 'undefined')
    quit();

load(libdir + "regexp_parse.js");

test("", all_flags,
     Empty());
