// |reftest| skip-if(!xulRuntime.shell)

// Nearly every %TypedArray%.prototype method should throw a TypeError when called
// atop a detached array buffer. Here we check verify that this holds true for
// all relevant functions.
let buffer = new ArrayBuffer(32);
let array  = new Int32Array(buffer);
neuter(buffer, "change-data");

// A nice poisoned callable to ensure that we fail on a detached buffer check
// before a method attempts to do anything with its arguments.
var POISON = (function() {
    var internalTarget = {};
    var throwForAllTraps =
    new Proxy(internalTarget, { get(target, prop, receiver) {
        assertEq(target, internalTarget);
        assertEq(receiver, throwForAllTraps);
        throw "FAIL: " + prop + " trap invoked";
    }});
    return new Proxy(throwForAllTraps, throwForAllTraps);
});


assertThrowsInstanceOf(() => {
    array.copyWithin(POISON);
}, TypeError);

assertThrowsInstanceOf(() => {
    array.entries();
}, TypeError);

assertThrowsInstanceOf(() => {
    array.fill(POISON);
}, TypeError);

assertThrowsInstanceOf(() => {
    array.filter((x) => x);
}, TypeError);

assertThrowsInstanceOf(() => {
    array.find(POISON);
}, TypeError);

assertThrowsInstanceOf(() => {
    array.findIndex(POISON);
}, TypeError);

assertThrowsInstanceOf(() => {
    array.forEach(POISON);
}, TypeError);

assertThrowsInstanceOf(() => {
    array.indexOf(POISON);
}, TypeError);

assertThrowsInstanceOf(() => {
    array.includes(POISON);
}, TypeError);

assertThrowsInstanceOf(() => {
    array.join();
}, TypeError);

assertThrowsInstanceOf(() => {
    array.keys();
}, TypeError);

assertThrowsInstanceOf(() => {
    array.lastIndexOf(POISON);
}, TypeError);

assertThrowsInstanceOf(() => {
    array.map(POISON);
}, TypeError);

assertThrowsInstanceOf(() => {
    array.reduce(POISON);
}, TypeError);

assertThrowsInstanceOf(() => {
    array.reduceRight(POISON);
}, TypeError);

assertThrowsInstanceOf(() => {
    array.reverse();
}, TypeError);

assertThrowsInstanceOf(() => {
    array.slice(POISON, POISON);
}, TypeError);

assertThrowsInstanceOf(() => {
    array.some(POISON);
}, TypeError);

assertThrowsInstanceOf(() => {
    array.values();
}, TypeError);

assertThrowsInstanceOf(() => {
    array.every(POISON);
}, TypeError);

assertThrowsInstanceOf(() => {
    array.sort(POISON);
}, TypeError);

if (typeof reportCompare === "function")
    reportCompare(true, true);
