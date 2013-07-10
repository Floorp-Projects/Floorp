// The property assignments in Array.prototype.sort are strict assignments.

load(libdir + "asserts.js");

var a = ["A", , "B", "C", "D"];
var normalArrayElementDesc = Object.getOwnPropertyDescriptor(a, 0);
var getterDesc = {
    configurable: false,
    enumerable: true,
    get: function () { return "F"; },
    set: undefined
};
Object.defineProperty(a, 1, getterDesc);

// a.sort is permitted to try to delete a[1] or to try to assign a[1], but it
// must try one or the other. Either one will fail, throwing a TypeError.
assertThrowsInstanceOf(() => a.sort(), TypeError);

// a.sort() is not permitted to delete the nonconfigurable property.
assertDeepEq(Object.getOwnPropertyDescriptor(a, 1), getterDesc);

// The values left in the other elements of a are unspecified; some or all may
// have been deleted.
for (var i = 0; i < a.length; i++) {
    if (i !== 1 && a.hasOwnProperty(i)) {
        normalArrayElementDesc.value = a[i];
        assertDeepEq(Object.getOwnPropertyDescriptor(a, i), normalArrayElementDesc);
    }
}
