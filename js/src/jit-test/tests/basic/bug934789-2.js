load(libdir + "asserts.js");
assertThrowsInstanceOf(
    function() {
        function foo() {}
        foo = null;
        (function bar(e) {
            try {
                (function() { e; });
                throw 1;
            } catch (e) {
                foo();
            }
        })();
    },
    TypeError);
