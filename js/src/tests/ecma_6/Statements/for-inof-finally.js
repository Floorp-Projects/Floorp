var BUGNUMBER = 1332881;
var summary =
  "Leaving for-in and try should handle stack value in correct order";

print(BUGNUMBER + ": " + summary);

var a = (function () {
    for (var x in [0]) {
        try {} finally {
            return 11;
        }
    }
})();
assertEq(a, 11);

var b = (function () {
    for (var x of [0]) {
        try {} finally {
            return 12;
        }
    }
})();
assertEq(b, 12);

var c = (function () {
    for (var x in [0]) {
        for (var y of [0]) {
            try {} finally {
                return 13;
            }
        }
    }
})();
assertEq(c, 13);

var d = (function () {
    for (var x in [0]) {
        for (var y of [0]) {
            try {} finally {
                for (var z in [0]) {
                    for (var w of [0]) {
                        try {} finally {
                            return 14;
                        }
                    }
                }
            }
        }
    }
})();
assertEq(d, 14);

if (typeof reportCompare === "function")
  reportCompare(true, true);
