if (typeof parseRegExp === 'undefined')
    quit();

load(libdir + "regexp_parse.js");

test("(?:)", all_flags,
     Empty());
test("(?:a)", all_flags,
     Atom("a"));
test("X(?:a)Y", all_flags,
     Text([
         Atom("X"),
         Atom("a"),
         Atom("Y")
     ]));
