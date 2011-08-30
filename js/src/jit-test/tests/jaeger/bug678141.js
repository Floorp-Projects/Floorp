
c = {}.__proto__[1] = 3;
(function() {
    function b(a) {
        return a
    }
    for each(let z in [{}]) {
        print(new b(z))
    }
})()
