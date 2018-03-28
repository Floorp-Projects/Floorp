// Overview:
// - The outer function is an IIFE which gets marked as a singleton.
// - The |o[index]| inner function is then also marked as a singleton.
// - The |o[index]| inner function has a dynamic name from a computed property name.
// - The |self| inner function uses |Function.prototype.caller| to reinvoke the outer function.

(function(index) {
    var o = {
        [index]: class {
            constructor() {}

            // Prevent adding an inferred name at index = 1 by creating a
            // static method named "name".
            static [(index === 0 ? "not-name" : "name")]() {}
        }
    }

    // At index = 0 the class will get the inferred name "0".
    // At index = 1 the class should have no inferred name.
    assertEq(displayName(o[index]), index === 0 ? "0" : "");

    if (index === 0) {
        (function self() {
            self.caller(1);
        })();
    }
})(0);
