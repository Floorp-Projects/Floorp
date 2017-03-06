// |jit-test| need-for-each

var otherGlobal = newGlobal();

function test(str, arg, result)
{
    arg = arg || 'ponies';
    if (arguments.length < 3)
        result = 'ponies';

    var fun = new Function('x', str);

    var got = fun.toSource();
    var expect = '(function anonymous(x\n) {\n' + str + '\n})';
    if (got !== expect) {
        print("GOT:    " + got);
        print("EXPECT: " + expect);
        assertEq(got, expect);
    }

    // test reflection logic
    Reflect.parse(got);

    // test xdr by cloning a cross-compartment function
    var code = "(function (x) { " + str + " })";
    var c = clone(otherGlobal.evaluate(code));
    assertEq(c.toSource(), eval(code).toSource());

    var got = fun(arg);
    var expect = result;
    if (got !== expect) {
        print("GOT:" + got);
        print("EXPECT: " + expect);
        assertEq(got, expect);
    }
}

function isParseError(str)
{
    var caught = false;
    try {
        new Function(str);
    } catch(e) {
        assertEq(e instanceof TypeError || e instanceof SyntaxError, true);
        caught = true;
    }
    assertEq(caught, true);
}

function isRuntimeParseError(str, arg)
{
    var caught = false;
    try {
        (new Function("x", str))(arg);
    } catch(e) {
        assertEq(e instanceof TypeError || e instanceof SyntaxError, true);
        caught = true;
    }
    assertEq(caught, true);
}

function isReferenceError(str)
{
    var caught = false;
    try {
        (new Function(str))();
    } catch(e) {
        assertEq(e instanceof ReferenceError, true);
        caught = true;
    }
    assertEq(caught, true);
}

// var declarations
test('var y;return x;');
test('var y = x;return x;');
test('var [] = x;return x;');
test('var [, ] = x;return x;');
test('var [, , , , ] = x;return x;');
test('var x = x;return x;');
test('var y = y;return "" + y;', 'unicorns', 'undefined');
test('var x = eval("x");return x;');
test('var X = x + 1, y = x;return y;');
test('var X = x + 1, [] = X, [[, , ]] = X, y = x;return y;');
test('var [{a: X}] = x, [, {b: y}] = x;var X = X + 1, y = y + 2;return X + y;', [{a:"p"},{b:"p"}], "p1p2");
test('var [x] = [x];return x;');
test('var [[a, [b, c]]] = [[x, []]];return a;');
test('var [y] = [x];return y;');
test('var [a] = (1, [x]);return a;');
test('var [a] = (1, x, 1, x);return a;', ['ponies']);
test('var [x, y] = [x, x + 1];return x + y;', 1, 3);
test('var [x, y, z] = [x, x + 1, x + 2];return x + y + z;', 1, 6);
test('var [[x]] = [[x]];return x;');
test('var [x, y] = [x, x + 1];return x;');
test('var [x, [y, z]] = [x, x + 1];return x;');
test('var [{x: [x]}, {y1: y, z1: z}] = [x, x + 1];return x;',{x:['ponies']});
test('if (x) {var y = x;return x;}');
test('if (x) {y = x;var y = y;return y;}');
test('if (x) {var z = y;var [y] = x;z += y;}return z;', ['-'], 'undefined-');

// let declaration in context
test('if (x) {let y;return x;}');
test('if (x) {let x;return "" + x;}', 'unicorns', 'undefined');
test('if (x) {let y = x;return x;}');
test('if (x) {let y = x;return x;}');
test('if (x) {let [] = x;return x;}');
test('if (x) {let [, ] = x;return x;}');
test('if (x) {let [, , , , ] = x;return x;}');
test('if (x) {let X = x + 1, y = x;return y;}');
test('if (x) {let X = x + 1, [] = X, [[, , ]] = X, y = x;return y;}');
test('if (x) {let [{a: X}] = x, [, {b: Y}] = x;var XX = X + 1, YY = Y + 2;return XX + YY;}', [{a:"p"},{b:"p"}], "p1p2");
test('if (x) {let [[a, [b, c]]] = [[x, []]];return a;}');
test('if (x) {let [X] = [x];return X;}');
test('if (x) {let [y] = [x];return y;}');
test('if (x) {let [a] = (1, [x]);return a;}');
test('if (x) {let [a] = (1, x, 1, x);return a;}', ['ponies']);
test('if (x) {let [X, y] = [x, x + 1];return X + y;}', 1, 3);
test('if (x) {let [X, y, z] = [x, x + 1, x + 2];return X + y + z;}', 1, 6);
test('if (x) {let [[X]] = [[x]];return X;}');
test('if (x) {let [X, y] = [x, x + 1];return X;}');
test('if (x) {let [X, [y, z]] = [x, x + 1];return X;}');
test('if (x) {let [{x: [X]}, {y1: y, z1: z}] = [x, x + 1];return X;}',{x:['ponies']});
test('if (x) {let y = x;try {let x = 1;throw 2;} catch (e) {return y;}}');
test('try {let x = 1;throw 2;} catch (e) {return x;}');
test('x.foo;{let y = x;return y;}');
test('x.foo;if (x) {x.bar;let y = x;return y;}');
test('if (x) {let y = x;return function () {return eval("y");}();}');
test('return eval("let y = x; y");');
isRuntimeParseError('if (x) {let y = x;eval("var y = 2");return y;}', 'ponies');
test('"use strict";if (x) {let y = x;eval("var y = 2");return y;}');
test('"use strict";if (x) {let y = x;eval("let y = 2");return y;}');
test('"use strict";if (x) {let y = 1;return eval("let y = x;y;");}');
test('this.y = x;if (x) {let y = 1;return this.eval("y");}');

// for(;;)
test('for (;;) {return x;}');
test('for (let y = 1;;) {return x;}');
test('for (let y = 1;; ++y) {return x;}');
test('for (let y = 1; ++y;) {return x;}');
test('for (let [[a, [b, c]]] = [[x, []]];;) {return a;}');
test('var sum = 0;for (let y = x; y < 4; ++y) {sum += y;}return sum;', 1, 6);
test('var sum = 0;for (let x = 1; eval("x") < 4; ++x) {sum += eval("x");}return sum;', 1, 6);
test('for (var y = 1;;) {return x;}');
test('for (var y = 1;; ++y) {return x;}');
test('for (var y = 1; ++y;) {return x;}');
test('for (var X = 1, [y, z] = x, a = x; z < 4; ++z) {return X + y;}', [2,3], 3);
test('var sum = 0;for (var y = x; y < 4; ++y) {sum += y;}return sum;', 1, 6);
test('var sum = 0;for (var X = x, y = 10; X < 4; ++X) {sum += X;}return sum;', 1, 6);
test('var sum = 0;for (var X = x; X < 4; ++X) {sum += X;}return x;', 1, 1);
test('var sum = 0;for (var X = eval("x"); X < 4; ++X) {sum += X;}return sum;', 1, 6);
test('var sum = 0;for (var X = x; eval("X") < 4; ++X) {sum += eval("X");}return sum;', 1, 6);
test('var sum = 0;for (var X = eval("x"); eval("X") < 4; ++X) {sum += eval("X");}return sum;', 1, 6);
test('for (let y = x;;) {let x;return y;}');
test('for (let y = x;;) {let y;return x;}');
test('for (let y;;) {let y;return x;}');
test('for (let a = x;;) {let c = x, d = x;return c;}');
test('for (let [a, b] = x;;) {let c = x, d = x;return c;}');
test('for (let [a] = (1, [x]);;) {return a;}');
test('for (let [a] = (1, x, 1, x);;) {return a;}', ['ponies']);
isParseError('for (let x = 1, x = 2;;) {}');
isParseError('for (let [x, y] = a, {a:x} = b;;) {}');
isParseError('for (let [x, y, x] = a;;) {}');
isParseError('for (let [x, [y, [x]]] = a;;) {}');

// for(in)
test('for (let i in x) {return x;}');
test('for (let i in x) {let y;return x;}');
test('for each (let [a, b] in x) {let y;return x;}');
test('for (let i in x) {let i = x;return i;}');
test('var s = "";for (let a in x) {for (let b in x) {s += a + b;}}return s;', [1,2], '00011011');
test('var res = "";for (let i in x) {res += x[i];}return res;');
test('var res = "";for (var i in x) {res += x[i];}return res;');
isParseError('for ((let (x = {y: true}) x).y in eval("x")) {return eval("x");}');
test('for (let i in x) {break;}return x;');
test('for (let i in x) {break;}return eval("x");');
test('a:for (let i in x) {for (let j in x) {break a;}}return x;');
test('a:for (let i in x) {for (let j in x) {break a;}}return eval("x");');
test('var j;for (let i in x) {j = i;break;}return j;', {ponies:true});
isParseError('for (let [x, x] in o) {}');
isParseError('for (let [x, y, x] in o) {}');
isParseError('for (let [x, [y, [x]]] in o) {}');

// for(let ... in ...) scoping bugs (bug 1069480)
isReferenceError('for each (let [x, y] in x) {return x + y;}', [['ponies', '']]);
isReferenceError('for each (let [{0: x, 1: y}, z] in x) {return x + y + z;}', [[['po','nies'], '']], undefined);
isReferenceError('for (let x in eval("x")) {return x;}', {ponies:true}, undefined);
isReferenceError('for (let x in x) {return eval("x");}', {ponies:true}, undefined);
isReferenceError('for (let x in eval("x")) {return eval("x");}', {ponies:true}, undefined);
isReferenceError('for (let x in x) {break;}return x;');
isReferenceError('for (let x in x) {break;}return eval("x");');
isReferenceError('for (let x in eval("throw x")) {}', undefined, undefined);
isReferenceError('for each (let x in x) {eval("throw x");}', ['ponies'], undefined);
isReferenceError('for each (let {x: y, y: x} in [{x: x, y: x}]) {return y;}', undefined, undefined);

// don't forget about switch craziness
test('var y = 3;switch (function () {return eval("y");}()) {case 3:let y;return x;default:;}');
test('switch (x) {case 3:let y;return 3;case 4:let z;return 4;default:return x;}');
isParseError('switch (x) {case 3:let y;return 3;case 4:let y;return 4;default:;}');

// TDZ checks
isReferenceError('x + 1; let x = 42;');
isReferenceError('x = 42; let x;');
isReferenceError('inner(); function inner() { x++; } let x;');
isReferenceError('inner(); let x; function inner() { x++; }');
isReferenceError('inner(); let x; function inner() { function innerer() { x++; } innerer(); }');
isReferenceError('let x; var inner = function () { y++; }; inner(); let y;');
isReferenceError('let x = x;');
isReferenceError('let [x] = [x];');
isReferenceError('let {x} = {x:x};');
isReferenceError('switch (x) {case 3:let x;break;default:if (x === undefined) {return "ponies";}}');
isReferenceError('let x = function() {} ? x() : function() {}');
isReferenceError('(function() { let x = (function() { return x }()); }())');
isReferenceError('var sum = 0;for (let x = x, y = 10; x < 4; ++x) {sum += x;}return sum;');
isReferenceError('var sum = 0;for (let x = x; x < 4; ++x) {sum += x;}return x;');
isReferenceError('var sum = 0;for (let x = eval("x"); x < 4; ++x) {sum += x;}return sum;');
isReferenceError('var sum = 0;for (let x = x; eval("x") < 4; ++x) {sum += eval("x");}return sum;');
isReferenceError('var sum = 0;for (let x = eval("x"); eval("x") < 4; ++x) {sum += eval("x");}return sum;');
isReferenceError('for (let x = eval("throw x");;) {}');
isReferenceError('for (let x = x + "s"; eval("throw x");) {}');

// redecl with function statements
isParseError('let a; function a() {}');
