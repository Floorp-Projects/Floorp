var BUGNUMBER = 1290655;
var summary = "String.prototype.match should call GetMethod.";

print(BUGNUMBER + ": " + summary);

function create(value) {
    return {
        [Symbol.match]: value,
        toString() {
            return "-";
        }
    };
}

var expected = ["-"];
expected.index = 1;
expected.input = "a-a";

for (let v of [null, undefined]) {
    assertDeepEq("a-a".match(create(v)), expected);
}

for (let v of [1, true, Symbol.iterator, "", {}, []]) {
    assertThrowsInstanceOf(() => "a-a".match(create(v)), TypeError);
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
