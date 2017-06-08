function makeArray(array) {
    var log = [];
    Object.setPrototypeOf(array, new Proxy(Array.prototype, new Proxy({
        has(t, pk) {
            log.push(`Has:${String(pk)}`);
            return Reflect.has(t, pk);
        },
    }, {
        get(t, pk, r) {
            if (pk in t)
                return Reflect.get(t, pk, r);
            throw new Error(`Unexpected trap "${pk}" called`);
        }
    })));
    return {array, log};
}


var {array, log} = makeArray([1, null, 3]);
Array.prototype.indexOf.call(array, 100, {
    valueOf() {
        array.length = 0;
        return 0;
    }
});
assertEqArray(log, ["Has:0", "Has:1", "Has:2"]);


var {array, log} = makeArray([5, undefined, 7]);
Array.prototype.lastIndexOf.call(array, 100, {
    valueOf() {
        array.length = 0;
        return 2;
    }
});
assertEqArray(log, ["Has:2", "Has:1", "Has:0"]);


if (typeof reportCompare === "function")
    reportCompare(true, true);
