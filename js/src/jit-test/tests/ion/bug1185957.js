// |jit-test| error: TypeError

load(libdir + "class.js");

var test = `
class test {
    constructor() {};
}
(function() {
    test()
})();
`;

// Throw, even if we cannot run the test
if (classesEnabled())
    eval(test);
else
    throw new TypeError();
