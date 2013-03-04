load(libdir + "parallelarray-helpers.js");

function buildSimple() {

    assertParallelArrayModesCommute(["seq", "par"], function(m) {
        return new ParallelArray([256], function(i) {
            return [i, i+1, i+2, i+3];
        }, m);
    });

    assertParallelArrayModesCommute(["seq", "par"], function(m) {
        return new ParallelArray([256], function(i) {
            var x = [];
            for (var i = 0; i < 4; i++) {
                x[i] = i;
            }
            return x;
        }, m);
    });

    assertParallelArrayModesCommute(["seq", "par"], function(m) {
        return new ParallelArray([256], function(i) {
            var x = [];
            for (var i = 0; i < 99; i++) {
                x[i] = i;
            }
            return x;
        }, m);
    });
}

buildSimple();
