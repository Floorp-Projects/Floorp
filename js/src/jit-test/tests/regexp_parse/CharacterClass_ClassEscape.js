if (typeof parseRegExp === 'undefined')
    quit();

load(libdir + "regexp_parse.js");

test("[\b]", all_flags,
     CharacterClass([
         ["\u0008", "\u0008"]
     ]));
test("[\-]", all_flags,
     CharacterClass([
         ["-", "-"]
     ]));
