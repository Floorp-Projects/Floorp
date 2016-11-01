if (typeof parseRegExp === 'undefined')
    quit();

load(libdir + "regexp_parse.js");

test("[\\x00]", all_flags,
     CharacterClass([
         ["\u0000", "\u0000"]
     ]));
test("[\\xff]", all_flags,
     CharacterClass([
         ["\u00FF", "\u00FF"]
     ]));

// Invalid

test("[\\x]", no_unicode_flags,
     CharacterClass([
         ["x", "x"]
     ]));

test("[\\xG]", no_unicode_flags,
     CharacterClass([
         ["x", "x"],
         ["G", "G"]
     ]));

test("[\\x0]", no_unicode_flags,
     CharacterClass([
         ["x", "x"],
         ["0", "0"]
     ]));

test("[\\x0G]", no_unicode_flags,
     CharacterClass([
         ["x", "x"],
         ["0", "0"],
         ["G", "G"],
     ]));
