var BUGNUMBER = 1290655;
var summary = "String.prototype.search should call GetMethod.";

print(BUGNUMBER + ": " + summary);

function create(value) {
    return {
        [Symbol.search]: value,
        toString() {
            return "-";
        }
    };
}

for (let v of [null, undefined]) {
    assertEq("a-a".search(create(v)), 1);
}

for (let v of [1, true, Symbol.iterator, "", {}, []]) {
    assertThrowsInstanceOf(() => "a-a".search(create(v)), TypeError);
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
