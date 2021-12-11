// Deleting the .next method of an iterator in the middle of a for-of loop
// doesn't cause a TypeError at the next iteration.

load(libdir + "asserts.js");
load(libdir + "iteration.js");

var iterProto = Object.getPrototypeOf([][Symbol.iterator]());
var s = '';
for (var v of ['duck', 'duck', 'duck', 'goose', 'and now you\'re it']) {
    s += v;
    if (v === 'goose')
        delete iterProto.next;
    s += '.';
}
assertEq(s, 'duck.duck.duck.goose.and now you\'re it.');
