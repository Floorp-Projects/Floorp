// |jit-test| error: TypeError

class test {
    constructor() {};
}

(function() {
    test()
})();
