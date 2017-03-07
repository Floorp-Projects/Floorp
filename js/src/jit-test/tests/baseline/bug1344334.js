if (!('oomTest' in this))
    quit();

function f(s) {
    s + "x";
    s.indexOf("y") === 0;
    oomTest(new Function(s));
}
var s = `
    class TestClass { constructor() {} }
    for (var fun of hasPrototype) {}
`;
if (s.length)
    f(s);
