var test = `

function testBuiltin(builtin, ...args) {
    class inst extends builtin {
        constructor(...args) {
            super(...args);
            this.called = true;
        }
    }

    let instance = new inst(...args);
    assertEq(instance instanceof inst, true);
    assertEq(instance instanceof builtin, true);
    assertEq(instance.called, true);
}


testBuiltin(Function);
testBuiltin(Object);
testBuiltin(Boolean);
testBuiltin(Error);
testBuiltin(EvalError);
testBuiltin(RangeError);
testBuiltin(ReferenceError);
testBuiltin(SyntaxError);
testBuiltin(TypeError);
testBuiltin(URIError);
testBuiltin(Number);
testBuiltin(Date);
testBuiltin(Date, 5);
testBuiltin(Date, 5, 10);
testBuiltin(RegExp);
testBuiltin(RegExp, /Regexp Argument/);
testBuiltin(RegExp, "String Argument");
testBuiltin(Map);
testBuiltin(Set);

`;

if (classesEnabled())
    eval(test);

if (typeof reportCompare === 'function')
    reportCompare(0,0,"OK");
