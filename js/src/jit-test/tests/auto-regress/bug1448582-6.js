// Overview:
// - The outer function is an IIFE which gets marked as a singleton.
// - The |o[index]| inner function is then also marked as a singleton.
// - The |o[index]| inner function has a dynamic name from a computed property name.
// - The |self| inner function uses |Function.prototype.caller| to reinvoke the outer function.

(function(index) {
    var o = {
        [index]: class {
            constructor() {}

            // The static method named "name" is added after assigning the
            // inferred name.
            static [(index === 0 ? "not-name" : "name")]() {}
        }
    }

    // The inferred name matches the current index.
    assertEq(displayName(o[index]), String(index));

    if (index === 0) {
        (function self() {
            self.caller(1);
        })();
    }
})(0);
