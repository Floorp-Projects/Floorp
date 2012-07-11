(function() {
    var s = "__proto__";
    assertEq(arguments[s], Object.prototype);   
})();

Object.defineProperty(Object.prototype, "foo", {
    get:function() {
        this.bar = 42;
        return 41
    }
});

(function() {
    var s = "foo";
    assertEq(arguments[s], 41);
    s = "bar";
    assertEq(arguments[s], 42);
    assertEq("bar" in Object.prototype, false);
})();
