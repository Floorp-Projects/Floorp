var BUGNUMBER = 1290655;
var summary = "String.prototype.replace should call GetMethod.";

print(BUGNUMBER + ": " + summary);

function create(value) {
    return {
        [Symbol.replace]: value,
        toString() {
            return "-";
        }
    };
}

for (let v of [null, undefined]) {
    assertEq("a-a".replace(create(v), "+"), "a+a");
}

for (let v of [1, true, Symbol.iterator, "", {}, []]) {
    assertThrowsInstanceOf(() => "a-a".replace(create(v)), TypeError);
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
