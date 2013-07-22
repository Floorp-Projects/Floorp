// ParallelArray methods throw when passed a this-value that isn't a ParallelArray.

load(libdir + "asserts.js");

function testcase(obj, fn) {
    assertEq(typeof fn, "function");
    var args = Array.slice(arguments, 2);
    assertThrowsInstanceOf(function () { fn.apply(obj, args); }, TypeError);
}

function test(obj) {
    function f() {}
    testcase(obj, ParallelArray.prototype.map, f);
    testcase(obj, ParallelArray.prototype.reduce, f);
    testcase(obj, ParallelArray.prototype.scan, f);
    testcase(obj, ParallelArray.prototype.scatter, [0]);
    testcase(obj, ParallelArray.prototype.filter, [0]);
    testcase(obj, ParallelArray.prototype.flatten);
    testcase(obj, ParallelArray.prototype.partition, 2);
    testcase(obj, ParallelArray.prototype.get, [1]);
}

// FIXME(bug 844887) check type of this
// if (getBuildConfiguration().parallelJS) {
// test(ParallelArray.prototype);
// test(Object.create(new ParallelArray));
// test({});
// test(null);
// test(undefined);
// }
