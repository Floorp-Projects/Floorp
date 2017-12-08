// It's invalid to eval super.prop inside a nested non-method, even if it
// appears inside a method definition
assertThrowsInstanceOf(() =>
({
    method() {
        (function () {
            eval('super.toString');
        })();
    }
}).method(), SyntaxError);

if (typeof reportCompare === 'function')
    reportCompare(0,0,"OK");
