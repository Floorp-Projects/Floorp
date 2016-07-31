var BUGNUMBER = 1290655;
var summary = "String.prototype.split should call GetMethod.";

print(BUGNUMBER + ": " + summary);

function create(value) {
    return {
        [Symbol.split]: value,
        toString() {
            return "-";
        }
    };
}

for (let v of [null, undefined]) {
    assertDeepEq("a-a".split(create(v)), ["a", "a"]);
}

for (let v of [1, true, Symbol.iterator, "", {}, []]) {
    assertThrowsInstanceOf(() => "a-a".split(create(v)), TypeError);
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
