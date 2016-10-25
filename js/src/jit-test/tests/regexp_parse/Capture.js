if (typeof parseRegExp === 'undefined')
    quit();

load(libdir + "regexp_parse.js");

test("()", all_flags,
     Capture(1, Empty()));

test("(a)", all_flags,
     Capture(1, Atom("a")));

test("((a()b))c(d)", all_flags,
     Alternative([
         Capture(1, Capture(2, Alternative([
             Atom("a"),
             Capture(3, Empty()),
             Atom("b")
         ]))),
         Atom("c"),
         Capture(4, Atom("d"))
     ]));
