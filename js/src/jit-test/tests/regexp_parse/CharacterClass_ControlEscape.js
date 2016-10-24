if (typeof parseRegExp === 'undefined')
    quit();

load(libdir + "regexp_parse.js");

test("[\\f]", all_flags,
     CharacterClass([
         ["\u000c", "\u000c"]
     ]));

test("[\\n]", all_flags,
     CharacterClass([
         ["\u000a", "\u000a"]
     ]));

test("[\\r]", all_flags,
     CharacterClass([
         ["\u000d", "\u000d"]
     ]));

test("[\\t]", all_flags,
     CharacterClass([
         ["\u0009", "\u0009"]
     ]));

test("[\\v]", all_flags,
     CharacterClass([
         ["\u000b", "\u000b"]
     ]));
