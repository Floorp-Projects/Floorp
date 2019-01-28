load(libdir + "asserts.js");

var g = newGlobal();

function testArrayOf() {
    var a = Array.of.call(g.Array);
    assertEq(a instanceof g.Array, true);
}
testArrayOf();

function testPromiseThen() {
    var p = Promise.resolve(0);
    p.constructor = g.Promise;
    var r = p.then(() => {});
    assertEq(r instanceof g.Promise, true);
}
testPromiseThen();

function testPromiseCatch() {
    Boolean.prototype.then = g.Promise.prototype.then;
    assertThrowsInstanceOf(() => Promise.prototype.catch.call(false),
                           g.TypeError);
}
testPromiseCatch();
