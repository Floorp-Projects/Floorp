// Overview:
// - The outer function is an IIFE which gets marked as a singleton.
// - The |o[index]| inner function is then also marked as a singleton.
// - The |o[index]| inner function has a dynamic name from a computed property name.
// - The |self| inner function uses |Function.prototype.caller| to reinvoke the outer function.
//
// When we reinvoke outer, we end up cloning a previously reused, i.e. non-cloned,
// function which triggered an assertion in js::SetFunctionNameIfNoOwnName().

(function(index) {
    var o = {
        [index]: function() {}
    };

    // Reinvoke the IIFE through |Function.prototype.caller|.
    if (index === 0) {
        (function self() {
            self.caller(1);
        })();
    }
})(0);
