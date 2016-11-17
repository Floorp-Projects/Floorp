if (typeof parseRegExp === 'undefined')
    quit();

load(libdir + "regexp_parse.js");

test("[\\d]", all_flags,
     CharacterClass([["0", "9"]]));

test("[\\D]", no_unicode_flags,
     CharacterClass([
         ["\u0000", "/"],
         [":", "\uFFFF"]
     ]));
test("[\\D]", unicode_flags,
     AllSurrogateAndCharacterClass([
         ["\u0000", "/"],
         [":", "\uD7FF"],
         ["\uE000", "\uFFFF"]
     ]));

test("[\\s]", all_flags,
     CharacterClass([
         ["\t", "\r"],
         [" ", " "],
         ["\u00A0", "\u00A0"],
         ["\u1680", "\u1680"],
         ["\u2000", "\u200A"],
         ["\u2028", "\u2029"],
         ["\u202F", "\u202F"],
         ["\u205F", "\u205F"],
         ["\u3000", "\u3000"],
         ["\uFEFF", "\uFEFF"]
     ]));
test("[\\S]", no_unicode_flags,
     CharacterClass([
         ["\u0000", "\u0008"],
         ["\u000E", "\u001F"],
         ["!", "\u009F"],
         ["\u00A1", "\u167F"],
         ["\u1681", "\u1FFF"],
         ["\u200B", "\u2027"],
         ["\u202A", "\u202E"],
         ["\u2030", "\u205E"],
         ["\u2060", "\u2FFF"],
         ["\u3001", "\uFEFE"],
         ["\uFF00", "\uFFFF"]
     ]));
test("[\\S]", unicode_flags,
     AllSurrogateAndCharacterClass([
         ["\u0000", "\u0008"],
         ["\u000E", "\u001F"],
         ["!", "\u009F"],
         ["\u00A1", "\u167F"],
         ["\u1681", "\u1FFF"],
         ["\u200B", "\u2027"],
         ["\u202A", "\u202E"],
         ["\u2030", "\u205E"],
         ["\u2060", "\u2FFF"],
         ["\u3001", "\uD7FF"],
         ["\uE000", "\uFEFE"],
         ["\uFF00", "\uFFFF"]
     ]));

test("[\\w]", no_unicode_flags,
     CharacterClass([
         ["0", "9"],
         ["A", "Z"],
         ["_", "_"],
         ["a", "z"]
     ]));
test("[\\w]", ["u", "mu"],
     CharacterClass([
         ["0", "9"],
         ["A", "Z"],
         ["_", "_"],
         ["a", "z"]
     ]));
test("[\\w]", ["iu", "imu"],
     CharacterClass([
         ["0", "9"],
         ["A", "Z"],
         ["_", "_"],
         ["a", "z"],
         ["\u017F", "\u017F"],
         ["\u212A", "\u212A"]
     ]));

test("[\\W]", no_unicode_flags,
     CharacterClass([
         ["\u0000", "/"],
         [":", "@"],
         ["[", "^"],
         ["`", "`"],
         ["{", "\uFFFF"]
     ]));
test("[\\W]", ["u", "mu"],
     AllSurrogateAndCharacterClass([
         ["\u0000", "/"],
         [":", "@"],
         ["[", "^"],
         ["`", "`"],
         ["{", "\uD7FF"],
         ["\uE000", "\uFFFF"]
     ]));
test("[\\W]", ["iu", "imu"],
     AllSurrogateAndCharacterClass([
         ["\u0000", "/"],
         [":", "@"],
         ["[", "^"],
         ["`", "`"],
         ["{", "\u017E"],
         ["\u0180", "\u2129"],
         ["\u212B", "\uD7FF"],
         ["\uE000", "\uFFFF"]
     ]));
