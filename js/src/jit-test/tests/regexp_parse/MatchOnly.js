if (typeof parseRegExp === 'undefined')
    quit();

load(libdir + "regexp_parse.js");

// Leading and trailing .* are ignored if match_only==true.

test_match_only(".*abc", all_flags,
                Atom("abc"));
test_match_only("abc.*", all_flags,
                Atom("abc"));

test_match_only(".*?abc", no_unicode_flags,
                Alternative([
                    Quantifier(0, kInfinity, "NON_GREEDY",
                               CharacterClass([
                                   ["\u0000", "\u0009"],
                                   ["\u000b", "\u000c"],
                                   ["\u000e", "\u2027"],
                                   ["\u202A", "\uFFFF"]
                               ])),
                    Atom("abc")
                ]));
test_match_only(".*?abc", unicode_flags,
                Alternative([
                    Quantifier(0, kInfinity, "NON_GREEDY",
                               AllSurrogateAndCharacterClass([
                                   ["\u0000", "\u0009"],
                                   ["\u000b", "\u000c"],
                                   ["\u000e", "\u2027"],
                                   ["\u202A", "\uD7FF"],
                                   ["\uE000", "\uFFFF"]
                               ])),
                    Atom("abc")
                ]));
