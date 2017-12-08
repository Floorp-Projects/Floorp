(function() {
    var rx = /a/g;
    var b = {
        get a() {
            rx.compile("b");
            return "A";
        }
    };

    // Replacer function which is applicable for the elem-base optimization in
    // RegExp.prototype.@@replace.
    function replacer(a) {
        return b[a];
    }

    var result = rx[Symbol.replace]("aaa", replacer);

    assertEq(result, "AAA");
})();

if (typeof reportCompare === "function")
    reportCompare(true, true);
