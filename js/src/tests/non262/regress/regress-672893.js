// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

function f() {
    return function () { return function () { return function () {
    return function () { return function () { return function () {
    return function () { return function () { return function () {
    return function () { return function () { return function () {
    return function () { return function () { return function (a) {
        var v = a;
	assertEq(v, 42);
	return function() { return v; };
    }; }; }; }; }; }; }; }; }; }; }; }; }; }; };
};

assertEq(f()()()()()()()()()()()()()()()(42)(), 42);

reportCompare(0, 0, 'ok');
