// A map iterator can cope with removing the current entry.

function test(pairs) {
    print(uneval(pairs));
    var map = new Map(pairs);
    
    var all_keys = '';
    var false_keys = '';
    for (let [k, v] of map) {
        all_keys += k;
        if (!v)
            false_keys += k;
    }

    var log = '';
    for (let [k, remove] of map) {
        log += k;
        if (remove)
            map.delete(k);
    }
    assertEq(log, all_keys);

    var remaining_keys = [k for ([k] of map)].join('');
    assertEq(remaining_keys, false_keys);
}

// removing the only entry
test([['a', true]]);

// removing the first entry
test([['a', true], ['b', false], ['c', false]]);

// removing a middle entry
test([['a', false], ['b', true], ['c', false]]);

// removing the last entry
test([['a', false], ['b', false], ['c', true]]);

// removing all entries
test([['a', true], ['b', true], ['c', true]]);
