function assertEqArray(actual, expected) {
    if (actual.length != expected.length) {
        throw new Error(
            "array lengths not equal: got " +
            uneval(actual) + ", expected " + uneval(expected));
    }

    for (var i = 0; i < actual.length; ++i) {
        if (actual[i] != expected[i]) {
        throw new Error(
            "arrays not equal at element " + i + ": got " +
            uneval(actual) + ", expected " + uneval(expected));
        }
    }
}

assertEqArray(/(?:(?:(")(c)")?)*/.exec('"c"'), [ '"c"', '"', "c" ]);
assertEqArray(/(?:(?:a*?(")(c)")?)*/.exec('"c"'), [ '"c"', '"', "c" ]);
assertEqArray(/<script\s*(?![^>]*type=['"]?(?:dojo\/|text\/html\b))(?:[^>]*?(?:src=(['"]?)([^>]*?)\1[^>]*)?)*>([\s\S]*?)<\/script>/gi.exec('<script type="text/javascript" src="..."></script>'), ['<script type="text/javascript" src="..."></script>', '"', "...", ""]);
