function IsASCIIAlphaString_CharCodeAt(s) {
    for (var i = 0; i < s.length; i++) {
        var c = s.charCodeAt(i);
        if (!((0x41 <= c && c <= 0x5A) || (0x61 <= c && c <= 0x7A)))
            return false;
    }
    return true;
}

function IsASCIIAlphaString_CharAt(s) {
    for (var i = 0; i < s.length; i++) {
        var c = s.charAt(i);
        if (!(("A" <= c && c <= "Z") || ("a" <= c && c <= "z")))
            return false;
    }
    return true;
}

function IsASCIIAlphaString_GetElem(s) {
    for (var i = 0; i < s.length; i++) {
        var c = s[i];
        if (!(("A" <= c && c <= "Z") || ("a" <= c && c <= "z")))
            return false;
    }
    return true;
}

function IsASCIIAlphaString_GetElem_GetElem(s) {
    var range = "AZaz";
    for (var i = 0; i < s.length; i++) {
        var c = s[i];
        if (!((range[0] <= c && c <= range[1]) || (range[2] <= c && c <= range[3])))
            return false;
    }
    return true;
}

function IsGreekOrCyrillicString_CharCodeAt(s) {
    // U+0370 (GREEK CAPITAL LETTER HETA)
    // U+03FF (GREEK CAPITAL REVERSED DOTTED LUNATE SIGMA SYMBOL)
    // U+0400 (CYRILLIC CAPITAL LETTER IE WITH GRAVE)
    // U+052F (CYRILLIC SMALL LETTER EL WITH DESCENDER)
    for (var i = 0; i < s.length; i++) {
        var c = s.charCodeAt(i);
        if (!((0x0370 <= c && c <= 0x03FF) || (0x400 <= c && c <= 0x052F)))
            return false;
    }
    return true;
}

function IsGreekOrCyrillicString_CharAt(s) {
    for (var i = 0; i < s.length; i++) {
        var c = s.charAt(i);
        if (!(("Ͱ" <= c && c <= "Ͽ") || ("Ѐ" <= c && c <= "ԯ")))
            return false;
    }
    return true;
}

function IsGreekOrCyrillicString_GetElem(s) {
    for (var i = 0; i < s.length; i++) {
        var c = s[i];
        if (!(("Ͱ" <= c && c <= "Ͽ") || ("Ѐ" <= c && c <= "ԯ")))
            return false;
    }
    return true;
}

function IsGreekOrCyrillicString_GetElem_GetElem(s) {
    var range = "ͰϿЀԯ";
    for (var i = 0; i < s.length; i++) {
        var c = s[i];
        if (!((range[0] <= c && c <= range[1]) || (range[2] <= c && c <= range[3])))
            return false;
    }
    return true;
}

function main() {
    function compareLatin1() {
        var strings = ["ABCABCABC", "abcabcabc"];
        var compare = "aAbD";
        var q = 0;
        for (var i = 0; i < 200; ++i) {
            var str = strings[i & 1];
            for (var j = 0; j < str.length; ++j) {
                if (str[j] === "a")
                    q++;
                if ("A" == str[j])
                    q++;
                if (str[j] != "b")
                    q++;
                if ("D" !== str[j])
                    q++;
                if (str[j] === compare[0])
                    q++;
                if (compare[1] == str[j])
                    q++;
                if (str[j] != compare[2])
                    q++;
                if (compare[3] !== str[j])
                    q++;
            }
        }
        assertEq(q, 100*3*2 + 100*3*2 + 100*15*2 + 100*18*2);
    }
    function compareTwoByte() {
        var strings = ["āĉœāĉœāĉœ", "abcabcabc"];
        var compare = "œĉāƉ";
        var q = 0;
        for (var i = 0; i < 200; ++i) {
            var str = strings[i & 1];
            for (var j = 0; j < str.length; ++j) {
                if ("œ" === str[j])
                    q++;
                if (str[j] == "ĉ")
                    q++;
                if ("ā" != str[j])
                    q++;
                if (str[j] !== "Ɖ")
                    q++;
                if (compare[0] === str[j])
                    q++;
                if (str[j] == compare[1])
                    q++;
                if (compare[2] != str[j])
                    q++;
                if (str[j] !== compare[3])
                    q++;
            }
        }
        assertEq(q, 100*3*2 + 100*3*2 + 100*15*2 + 100*18*2);
    }
    function compareRangeLatin1() {
        var strings = [
            "ABCABCABC", // all upper
            "abcabcabc", // all lower
            "abcABCabc", // lower and upper
            "abcabc123", // characters below limit
            "abc[_]ABC", // characters between limit
            "ABC{|}abc", // characters above limit
            "!#$456_~ÿ", // no matches at all
            "aBcZyyZUU", // -
        ];
        for (var i = 0; i < 200; ++i) {
            var str = strings[i & 7];
            var resultCharCodeAt = IsASCIIAlphaString_CharCodeAt(str);
            var resultCharAt = IsASCIIAlphaString_CharAt(str);
            var resultGetElem = IsASCIIAlphaString_GetElem(str);
            var resultGetElemGetElem = IsASCIIAlphaString_GetElem_GetElem(str);

            assertEq(resultCharAt, resultCharCodeAt);
            assertEq(resultGetElem, resultCharCodeAt);
            assertEq(resultGetElemGetElem, resultCharCodeAt);
        }
    }
    function compareRangeTwoByte() {
        var strings = [
            "αβγΑΒΓαβγ", // all Greek
            "АБВабвАБВ", // all Cyrillic
            "αβγабвАБΓ", // Greek and Cyrillic
            "αβγāēōАБВ", // characters below limit
            "αβγԱԲԳАБВ", // characters above limit
            "abcāēōԱԲԳ", // no matches at all
            "𝐀𝐁𝐂𝐀𝐁𝐂𝐀𝐁𝐂", // (non-BMP)
            "abcabcabc", // -
        ];
        for (var i = 0; i < 200; ++i) {
            var str = strings[i & 7];
            var resultCharCodeAt = IsGreekOrCyrillicString_CharCodeAt(str);
            var resultCharAt = IsGreekOrCyrillicString_CharAt(str);
            var resultGetElem = IsGreekOrCyrillicString_GetElem(str);
            var resultGetElemGetElem = IsGreekOrCyrillicString_GetElem_GetElem(str);

            assertEq(resultCharAt, resultCharCodeAt);
            assertEq(resultGetElem, resultCharCodeAt);
            assertEq(resultGetElemGetElem, resultCharCodeAt);
        }
    }

    compareLatin1();
    compareTwoByte();
    compareRangeLatin1();
    compareRangeTwoByte();
}

for (var i = 0; i < 15; ++i) {
    main();
}
