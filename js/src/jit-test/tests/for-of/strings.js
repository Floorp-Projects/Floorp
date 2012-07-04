// for-of works on strings and String objects.

function test(s) {
    var copy = '';
    for (var v of s) {
        assertEq(typeof v, 'string');
        assertEq(v.length, 1);
        copy += v;
    }
    assertEq(copy, String(s));
}

test('');
test('abc');
test('a \0 \ufffe \ufeff');

// Non-BMP characters are generally passed to JS in UTF-16, as surrogate pairs.
// ES requires that such pairs be treated as two 16-bit "characters" in pretty
// much every circumstance, including string indexing. We anticipate the same
// requirement will be imposed here, though it's not a sure thing.
test('\ud808\udf45');

test(new String(''));
test(new String('abc'));
test(new String('a \0 \ufffe \ufeff'));
test(new String('\ud808\udf45'));
