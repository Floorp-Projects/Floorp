(function f() {
    let x = (new function() {})
    __defineGetter__("x", function() {
        ({
            e: x
        })
    })
})();
print(x)
