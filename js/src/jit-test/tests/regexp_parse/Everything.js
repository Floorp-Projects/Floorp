if (typeof parseRegExp === 'undefined')
    quit();

load(libdir + "regexp_parse.js");

test_mix(".", no_unicode_flags,
         CharacterClass([
             ["\u0000", "\u0009"],
             ["\u000b", "\u000c"],
             ["\u000e", "\u2027"],
             ["\u202A", "\uFFFF"]
         ]));

test_mix(".", unicode_flags,
         AllSurrogateAndCharacterClass([
             ["\u0000", "\u0009"],
             ["\u000b", "\u000c"],
             ["\u000e", "\u2027"],
             ["\u202A", "\uD7FF"],
             ["\uE000", "\uFFFF"]
         ]));
