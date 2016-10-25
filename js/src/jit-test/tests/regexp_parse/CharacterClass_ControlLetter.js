if (typeof parseRegExp === 'undefined')
    quit();

load(libdir + "regexp_parse.js");

test("[\\ca]", all_flags,
     CharacterClass([
         ["\u0001", "\u0001"]
     ]));
test("[\\cz]", all_flags,
     CharacterClass([
         ["\u001a", "\u001a"]
     ]));
test("[\\cA]", all_flags,
     CharacterClass([
         ["\u0001", "\u0001"]
     ]));
test("[\\cZ]", all_flags,
     CharacterClass([
         ["\u001a", "\u001a"]
     ]));

// Invalid

test("[\\c]", no_unicode_flags,
     CharacterClass([
         ["\\", "\\"],
         ["c", "c"]
     ]));
test("[\\c=]", no_unicode_flags,
     CharacterClass([
         ["\\", "\\"],
         ["c", "c"],
         ["=", "="]
     ]));
