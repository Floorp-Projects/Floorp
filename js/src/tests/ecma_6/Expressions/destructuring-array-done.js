var BUGNUMBER = 1184922;
var summary = "iterator.next() should not be called when after iterator completes";

print(BUGNUMBER + ": " + summary);

var log;
function reset() {
    log = "";
}
var obj = new Proxy({}, {
    set(that, name, value) {
        var v;
        if (value instanceof Function || value instanceof RegExp)
            v = value.toString();
        else
            v = JSON.stringify(value);
        log += "set:" + name + "=" + v + ",";
    }
});
function createIterable(n) {
    return {
        i: 0,
        [Symbol.iterator]() {
            return this;
        },
        next() {
            log += "next,";
            this.i++;
            if (this.i <= n)
                return {value: this.i, done: false};
            return {value: 0, done: true};
        }
    };
}

// Simple pattern.

reset();
[obj.a, obj.b, obj.c] = createIterable(0);
assertEq(log,
         "next," +
         "set:a=undefined," +
         "set:b=undefined," +
         "set:c=undefined,");

reset();
[obj.a, obj.b, obj.c] = createIterable(1);
assertEq(log,
         "next," +
         "set:a=1," +
         "next," +
         "set:b=undefined," +
         "set:c=undefined,");

reset();
[obj.a, obj.b, obj.c] = createIterable(2);
assertEq(log,
         "next," +
         "set:a=1," +
         "next," +
         "set:b=2," +
         "next," +
         "set:c=undefined,");

reset();
[obj.a, obj.b, obj.c] = createIterable(3);
assertEq(log,
         "next," +
         "set:a=1," +
         "next," +
         "set:b=2," +
         "next," +
         "set:c=3,");

// Elision.

reset();
[obj.a, , obj.b, , , obj.c, ,] = createIterable(0);
assertEq(log,
         "next," +
         "set:a=undefined," +
         "set:b=undefined," +
         "set:c=undefined,");

reset();
[obj.a, , obj.b, , , obj.c, ,] = createIterable(1);
assertEq(log,
         "next," +
         "set:a=1," +
         "next," +
         "set:b=undefined," +
         "set:c=undefined,");

reset();
[obj.a, , obj.b, , , obj.c, ,] = createIterable(2);
assertEq(log,
         "next," +
         "set:a=1," +
         "next," +
         "next," +
         "set:b=undefined," +
         "set:c=undefined,");

reset();
[obj.a, , obj.b, , , obj.c, ,] = createIterable(3);
assertEq(log,
         "next," +
         "set:a=1," +
         "next," +
         "next," +
         "set:b=3," +
         "next," +
         "set:c=undefined,");

reset();
[obj.a, , obj.b, , , obj.c, ,] = createIterable(4);
assertEq(log,
         "next," +
         "set:a=1," +
         "next," +
         "next," +
         "set:b=3," +
         "next," +
         "next," +
         "set:c=undefined,");

reset();
[obj.a, , obj.b, , , obj.c, ,] = createIterable(5);
assertEq(log,
         "next," +
         "set:a=1," +
         "next," +
         "next," +
         "set:b=3," +
         "next," +
         "next," +
         "next," +
         "set:c=undefined,");

reset();
[obj.a, , obj.b, , , obj.c, ,] = createIterable(6);
assertEq(log,
         "next," +
         "set:a=1," +
         "next," +
         "next," +
         "set:b=3," +
         "next," +
         "next," +
         "next," +
         "set:c=6," +
         "next,");

reset();
[obj.a, , obj.b, , , obj.c, ,] = createIterable(7);
assertEq(log,
         "next," +
         "set:a=1," +
         "next," +
         "next," +
         "set:b=3," +
         "next," +
         "next," +
         "next," +
         "set:c=6," +
         "next,");

// Rest.

reset();
[...obj.r] = createIterable(0);
assertEq(log,
         "next," +
         "set:r=[],");

reset();
[...obj.r] = createIterable(1);
assertEq(log,
         "next," +
         "next," +
         "set:r=[1],");

reset();
[obj.a, ...obj.r] = createIterable(0);
assertEq(log,
         "next," +
         "set:a=undefined," +
         "set:r=[],");

reset();
[obj.a, ...obj.r] = createIterable(1);
assertEq(log,
         "next," +
         "set:a=1," +
         "next," +
         "set:r=[],");

reset();
[obj.a, ...obj.r] = createIterable(2);
assertEq(log,
         "next," +
         "set:a=1," +
         "next," +
         "next," +
         "set:r=[2],");

reset();
[obj.a, obj.b, ...obj.r] = createIterable(0);
assertEq(log,
         "next," +
         "set:a=undefined," +
         "set:b=undefined," +
         "set:r=[],");

reset();
[obj.a, obj.b, ...obj.r] = createIterable(1);
assertEq(log,
         "next," +
         "set:a=1," +
         "next," +
         "set:b=undefined," +
         "set:r=[],");

reset();
[obj.a, obj.b, ...obj.r] = createIterable(2);
assertEq(log,
         "next," +
         "set:a=1," +
         "next," +
         "set:b=2," +
         "next," +
         "set:r=[],");

reset();
[obj.a, obj.b, ...obj.r] = createIterable(3);
assertEq(log,
         "next," +
         "set:a=1," +
         "next," +
         "set:b=2," +
         "next," +
         "next," +
         "set:r=[3],");

// Rest and elision.

reset();
[, ...obj.r] = createIterable(0);
assertEq(log,
         "next," +
         "set:r=[],");

reset();
[, ...obj.r] = createIterable(1);
assertEq(log,
         "next," +
         "next," +
         "set:r=[],");

reset();
[, ...obj.r] = createIterable(2);
assertEq(log,
         "next," +
         "next," +
         "next," +
         "set:r=[2],");

reset();
[obj.a, obj.b, , ...obj.r] = createIterable(0);
assertEq(log,
         "next," +
         "set:a=undefined," +
         "set:b=undefined," +
         "set:r=[],");

reset();
[obj.a, obj.b, , ...obj.r] = createIterable(1);
assertEq(log,
         "next," +
         "set:a=1," +
         "next," +
         "set:b=undefined," +
         "set:r=[],");

reset();
[obj.a, obj.b, , ...obj.r] = createIterable(2);
assertEq(log,
         "next," +
         "set:a=1," +
         "next," +
         "set:b=2," +
         "next," +
         "set:r=[],");

reset();
[obj.a, obj.b, , ...obj.r] = createIterable(3);
assertEq(log,
         "next," +
         "set:a=1," +
         "next," +
         "set:b=2," +
         "next," +
         "next," +
         "set:r=[],");

reset();
[obj.a, obj.b, , ...obj.r] = createIterable(4);
assertEq(log,
         "next," +
         "set:a=1," +
         "next," +
         "set:b=2," +
         "next," +
         "next," +
         "next," +
         "set:r=[4],");

if (typeof reportCompare === "function")
  reportCompare(true, true);
