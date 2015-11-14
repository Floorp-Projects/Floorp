var test = `

function testBuiltin(builtin) {
    class inst extends builtin {
        constructor() {
            super();
            this.called = true;
        }
    }

    let instance = new inst();
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

`;

if (classesEnabled())
    eval(test);

if (typeof reportCompare === 'function')
    reportCompare(0,0,"OK");
