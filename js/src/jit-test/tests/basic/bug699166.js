a = "".__proto__
b = uneval().__proto__
for (var i = 0; i < 2; i++) {
    a.__defineSetter__("valueOf", function() {})
    a + ""
    delete b.valueOf
}
