// Overview:
// - The outer function is an IIFE which gets marked as a singleton.
// - The |fn| inner function is then also marked as a singleton.
// - The |self| inner function uses |Function.prototype.caller| to reinvoke the outer function.
//
// When we reinvoke outer, we end up cloning a previously reused, i.e. non-cloned,
// function.

(function(index) {
    var fn = function() {};

    // Accessing |.name| sets the resolved-name flag, which should not be
    // copied over to the function clone.
    assertEq(fn.name, "fn");

    // Reinvoke the IIFE through |Function.prototype.caller|.
    if (index === 0) {
        (function self() {
            self.caller(1);
        })();
    }
})(0);
