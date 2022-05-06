function defaultValue() { return 123; }

// 2 environment objects: Call => Var. The lambda uses both of them.
function testBasic(p = defaultValue()) {
    for (var i = 0; i < 2000; i++) {}
    return () => i + p;
}
assertEq(testBasic()(), 2123);

function testBailout(p = defaultValue()) {
    for (var i = 0; i < 2000; i++) {
        if (i > 1950) {
            bailout();
        }
    }
    return () => i + p;
}
assertEq(testBailout()(), 2123);

// 3 environment objects: Call => Var => Lexical. The lambda uses all of them.
let escaped;
function testVarAndLexical(p = defaultValue()) {
    var q = p + 1;
    let i = 0;
    for (; i < 2000; i++) {
        escaped = () => i + p + q;
    }
}
testVarAndLexical();
assertEq(escaped(), 2247);

function testVarAndLexicalBailout(p = defaultValue()) {
    var q = p + 1;
    let i = 0;
    for (; i < 2000; i++) {
        escaped = () => i + p - q;
        if (i > 1950) {
            bailout();
        }
    }
}
testVarAndLexicalBailout();
assertEq(escaped(), 1999);
