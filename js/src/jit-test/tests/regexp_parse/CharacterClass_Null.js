if (typeof parseRegExp === 'undefined')
    quit();

load(libdir + "regexp_parse.js");

test("[\\0]", all_flags,
     CharacterClass([
         ["\u0000", "\u0000"]
     ]));
