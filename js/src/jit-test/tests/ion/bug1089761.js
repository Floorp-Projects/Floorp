var hits = 0;
for (var j = 0; j < 9; ++j) {
    try {
        (function() {
            (function() {
                eval("x")
                let x
            })()
        })()
    } catch (e) {
      hits++;
    }
}
assertEq(hits, 9);
