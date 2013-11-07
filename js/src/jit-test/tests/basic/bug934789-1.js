if (typeof dis === "function") {
    (function() {
        function foo() {}

        dis(function bar(e) {
            try {
                (function() { e; });
            } catch (e) {
                foo();
            }
        });
    }());
}
