(function f() {
    let x = (new function() {})
    this.__defineGetter__("x", function() {
        ({
            e: x
        })
    })
})();
print(x)
