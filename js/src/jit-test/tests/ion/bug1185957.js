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

if (classesEnabled())
    eval(test);
