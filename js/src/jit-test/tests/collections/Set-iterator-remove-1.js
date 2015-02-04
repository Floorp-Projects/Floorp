// A set iterator can cope with removing the current entry.

function test(letters, toRemove) {
    var set = new Set(letters);
    toRemove = new Set(toRemove);

    var leftovers = [x for (x of set) if (!toRemove.has(x))].join("");

    var log = "";
    for (let x of set) {
        log += x;
        if (toRemove.has(x))
            set.delete(x);
    }
    assertEq(log, letters);

    var remaining = [x for (x of set)].join("");
    assertEq(remaining, leftovers);
}

test('a', 'a');    // removing the only entry
test('abc', 'a');  // removing the first entry
test('abc', 'b');  // removing a middle entry
test('abc', 'c');  // removing the last entry
test('abc', 'abc') // removing all entries

