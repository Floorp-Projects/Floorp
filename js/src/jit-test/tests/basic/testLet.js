var otherGlobal = newGlobal('new-compartment');

function test(str, arg, result)
{
    arg = arg || 'ponies';
    result = result || 'ponies';

    var fun = new Function('x', str);

    var got = fun.toSource().replace(/\n/g,'');
    var expect = '(function anonymous(x) {' + str + '})';
    if (got !== expect) {
        print("GOT:    " + got);
        print("EXPECT: " + expect);
        assertEq(got, expect);
    }

    // test reflection logic
    Reflect.parse(got);

    // test xdr by cloning a cross-compartment function
    otherGlobal.str = str;
    var c = clone(otherGlobal.eval("new Function('x', str)"));
    assertEq(c.toSource(), fun.toSource());

    var got = fun(arg);
    var expect = result;
    if (got !== expect) {
        print("GOT:" + got);
        print("EXPECT: " + expect);
        assertEq(got, expect);
    }
}

function isError(str)
{
    var caught = false;
    try {
        new Function(str);
    } catch(e) {
        assertEq(String(e).indexOf('TypeError') == 0 || String(e).indexOf('SyntaxError') == 0, true);
        caught = true;
    }
    assertEq(caught, true);
}

// let expr
test('return let (y) x;');
test('return let (x) "" + x;', 'unicorns', 'undefined');
test('return let (y = x) (y++, "" + y);', 'unicorns', 'NaN');
test('return let (y = 1) (y = x, y);');
test('return let ([] = x) x;');
test('return let (x = {a: x}) x.a;');
test('return let ({a: x} = {a: x}) x;');
test('return let ([x] = {0: x}) x;');
test('return let ({0: x} = [x]) x;');
test('return let ({0: []} = []) x;');
test('return let ([, ] = x) x;');
test('return let ([, , , , ] = x) x;');
test('return let ([[]] = x) x;');
test('return let ([[[[[[[[[[[[[]]]]]]]]]]]]] = x) x;');
test('return let ([[], []] = x) x;');
test('return let ([[[[]]], [], , [], [[]]] = x) x;');
test('return let ({x: []} = x) x;');
test('return let ({x: [], y: {x: []}} = x) "ponies";', {y:{}});
test('return let ({x: []} = x, [{x: []}] = x) "ponies";');
test('return let (x = x) x;');
test('return let (x = eval("x")) x;');
test('return let (x = (let (x = x + 1) x) + 1) x;', 1, 3);
test('return let (x = (let (x = eval("x") + 1) eval("x")) + 1) eval("x");', 1, 3);
test('return let (x = x + 1, y = x) y;');
test('return let (x = x + 1, [] = x, [[, , ]] = x, y = x) y;');
test('return let ([{a: x}] = x, [, {b: y}] = x) let (x = x + 1, y = y + 2) x + y;', [{a:"p"},{b:"p"}], "p1p2");
test('return let ([] = []) x;');
test('return let ([] = [x]) x;');
test('return let ([a] = (1, [x])) a;');
test('return let ([a] = (1, x, 1, x)) a;', ['ponies']);
test('return let ([x] = [x]) x;');
test('return let ([[a, [b, c]]] = [[x, []]]) a;');
test('return let ([x, y] = [x, x + 1]) x + y;', 1, 3);
test('return let ([x, y, z] = [x, x + 1, x + 2]) x + y + z;', 1, 6);
test('return let ([[x]] = [[x]]) x;');
test('return let ([x, y] = [x, x + 1]) x;');
test('return let ([x, [y, z]] = [x, x + 1]) x;');
test('return let ([{x: [x]}, {y1: y, z1: z}] = [x, x + 1]) x;',{x:['ponies']});
test('return let (x = (3, x)) x;');
test('return let (x = x + "s") x;', 'ponie');
test('return let ([x] = (3, [x])) x;');
test('return let ([] = [[]] = {}) x;');
test('return let (y = x) function () {return eval("y");}();');
test('return eval("let (y = x) y");');
test('return let (y = x) (eval("var y = 2"), y);', 'ponies', 2);
test('"use strict";return let (y = x) (eval("var y = 2"), y);');
test('this.y = x;return let (y = 1) this.eval("y");');
test('try {let (x = x) eval("throw x");} catch (e) {return e;}');
test('try {return let (x = eval("throw x")) x;} catch (e) {return e;}');
isError('let (x = 1, x = 2) x');
isError('let ([x, y] = a, {a:x} = b) x');
isError('let ([x, y, x] = a) x');
isError('let ([x, [y, [x]]] = a) x');
isError('let (x = function() { return x}) x()return x;');
isError('(let (x = function() { return x}) x())return x;');

// let block
test('let (y) {return x;}');
test('let (y = x) {y++;return "" + y;}', 'unicorns', 'NaN');
test('let (y = 1) {y = x;return y;}');
test('let (x) {return "" + x;}', 'unicorns', 'undefined');
test('let ([] = x) {return x;}');
test('let (x) {}return x;');
test('let (x = {a: x}) {return x.a;}');
test('let ({a: x} = {a: x}) {return x;}');
test('let ([x] = {0: x}) {return x;}');
test('let ({0: x} = [x]) {return x;}');
test('let ({0: []} = []) {return x;}');
test('let ([, ] = x) {return x;}');
test('let ([, , , , ] = x) {return x;}');
test('let ([[]] = x) {return x;}');
test('let ([[[[[[[[[[[[[]]]]]]]]]]]]] = x) {return x;}');
test('let ([[], []] = x) {return x;}');
test('let ([[[[]]], [], , [], [[]]] = x) {return x;}');
test('let ({x: []} = x) {return x;}');
test('let ({x: [], y: {x: []}} = x) {return "ponies";}', {y:{}});
test('let ({x: []} = x, [{x: []}] = x) {return "ponies";}');
test('let (x = x) {return x;}');
test('let (x = eval("x")) {return x;}');
test('let (x = (let (x = x + 1) x) + 1) {return x;}', 1, 3);
test('let (x = (let (x = eval("x") + 1) eval("x")) + 1) {return eval("x");}', 1, 3);
test('let (x = x + 1, y = x) {return y;}');
test('let (x = x + 1, [] = x, [[, , ]] = x, y = x) {return y;}');
test('let ([{a: x}] = x, [, {b: y}] = x) {let (x = x + 1, y = y + 2) {return x + y;}}', [{a:"p"},{b:"p"}], "p1p2");
test('let ([] = []) {return x;}');
test('let ([] = [x]) {return x;}');
test('let ([a] = (1, [x])) {return a;}');
test('let ([a] = (1, x, 1, x)) {return a;}', ['ponies']);
test('let ([x] = [x]) {return x;}');
test('let ([[a, [b, c]]] = [[x, []]]) {return a;}');
test('let ([x, y] = [x, x + 1]) {return x + y;}', 1, 3);
test('let ([x, y, z] = [x, x + 1, x + 2]) {return x + y + z;}', 1, 6);
test('let ([[x]] = [[x]]) {return x;}');
test('let ([x, y] = [x, x + 1]) {return x;}');
test('let ([x, [y, z]] = [x, x + 1]) {return x;}');
test('let ([{x: [x]}, {y1: y, z1: z}] = [x, x + 1]) {return x;}',{x:['ponies']});
test('let (y = x[1]) {let (x = x[0]) {try {let (y = "unicorns") {throw y;}} catch (e) {return x + y;}}}', ['pon','ies']);
test('let (x = x) {try {let (x = "unicorns") eval("throw x");} catch (e) {return x;}}');
test('let ([] = [[]] = {}) {return x;}');
test('let (y = x) {return function () {return eval("y");}();}');
test('return eval("let (y = x) {y;}");');
test('let (y = x) {eval("var y = 2");return y;}', 'ponies', 2);
test('"use strict";let (y = x) {eval("var y = 2");return y;}');
test('this.y = x;let (y = 1) {return this.eval("y");}');
isError('let (x = 1, x = 2) {x}');
isError('let ([x, y] = a, {a:x} = b) {x}');
isError('let ([x, y, x] = a) {x}');
isError('let ([x, [y, [x]]] = a) {x}');

// var declarations
test('var y;return x;');
test('var y = x;return x;');
test('var [] = x;return x;');
test('var [, ] = x;return x;');
test('var [, , , , ] = x;return x;');
test('var [[]] = x;return x;');
test('var [[[[[[[[[[[[[]]]]]]]]]]]]] = x;return x;');
test('var [[], []] = x;return x;');
test('var [[[[]]], [], , [], [[]]] = x;return x;');
test('var {x: []} = x;return x;');
test('var {x: [], y: {x: []}} = x;return "ponies";', {y:{}});
test('var {x: []} = x, [{x: []}] = x;return "ponies";');
test('var x = x;return x;');
test('var y = y;return "" + y;', 'unicorns', 'undefined');
test('var x = eval("x");return x;');
test('var x = (let (x = x + 1) x) + 1;return x;', 1, 3);
test('var x = (let (x = eval("x") + 1) eval("x")) + 1;return eval("x");', 1, 3);
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
test('var [] = [[]] = {};return x;');
test('if (x) {var y = x;return x;}');
test('if (x) {y = x;var y = y;return y;}');
test('if (x) {var z = y;var [y] = x;z += y;}return z;', ['-'], 'undefined-');

// let declaration in context
test('if (x) {let y;return x;}');
test('if (x) {let x;return "" + x;}', 'unicorns', 'undefined');
test('if (x) {let y = x;return x;}');
test('if (x) {y = x;let y = y;return y;}');
test('if (x) {var z = y;let [y] = x;z += y;}return z;', ['-'], 'undefined-');
test('if (x) {let y = x;return x;}');
test('if (x) {let [] = x;return x;}');
test('if (x) {let [, ] = x;return x;}');
test('if (x) {let [, , , , ] = x;return x;}');
test('if (x) {let [[]] = x;return x;}');
test('if (x) {let [[[[[[[[[[[[[]]]]]]]]]]]]] = x;return x;}');
test('if (x) {let [[], []] = x;return x;}');
test('if (x) {let [[[[]]], [], , [], [[]]] = x;return x;}');
test('if (x) {let {x: []} = x;return x;}');
test('if (x) {let {x: [], y: {x: []}} = x;return "ponies";}', {y:{}});
test('if (x) {let {x: []} = x, [{x: []}] = x;return "ponies";}');
test('if (x) {let x = x;return "" + x;}', 'unicorns', 'undefined');
test('if (x) {let y = y;return "" + y;}', 'unicorns', 'undefined');
test('if (x) {let x = eval("x");return "" + x;}', 'unicorns', 'undefined');
test('if (x) {let y = (let (x = x + 1) x) + 1;return y;}', 1, 3);
test('if (x) {let y = (let (x = eval("x") + 1) eval("x")) + 1;return eval("y");}', 1, 3);
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
test('if (x) {let [] = [[]] = {};return x;}');
test('let (y, [] = x) {}try {let a = b(), b;} catch (e) {return x;}');
test('try {let x = 1;throw 2;} catch (e) {return x;}');
test('let (y = x) {let x;return y;}');
test('let (y = x) {let x = y;return x;}');
test('let ([y, z] = x) {let a = x, b = y;return a;}');
test('let ([y, z] = x, a = x, [] = x) {let b = x, c = y;return a;}');
test('function f() {return unicorns;}try {let (x = 1) {let a, b;f();}} catch (e) {return x;}');
test('function f() {return unicorns;}try {let (x = 1) {let a, b;}f();} catch (e) {return x;}');
test('x.foo;{let y = x;return y;}');
test('x.foo;if (x) {x.bar;let y = x;return y;}');
test('if (x) {let y = x;return function () {return eval("y");}();}');
test('return eval("let y = x; y");');
test('if (x) {let y = x;eval("var y = 2");return y;}', 'ponies', 2);
test('"use strict";if (x) {let y = x;eval("var y = 2");return y;}');
test('"use strict";if (x) {let y = x;eval("let y = 2");return y;}');
test('"use strict";if (x) {let y = 1;return eval("let y = x;y;");}');
test('this.y = x;if (x) {let y = 1;return this.eval("y");}');
isError('if (x) {let (x = 1, x = 2) {x}}');
isError('if (x) {let ([x, y] = a, {a:x} = b) {x}}');
isError('if (x) {let ([x, y, x] = a) {x}}');
isError('if (x) {let ([x, [y, [x]]] = a) {x}}');
isError('let ([x, y] = x) {let x;}');

// for(;;)
test('for (;;) {return x;}');
test('for (let y = 1;;) {return x;}');
test('for (let y = 1;; ++y) {return x;}');
test('for (let y = 1; ++y;) {return x;}');
test('for (let (x = 1) x; x != 1; ++x) {return x;}');
test('for (let [, {a: [], b: []}] = x, [] = x; x;) {return x;}');
test('for (let x = 1, [y, z] = x, a = x; z < 4; ++z) {return x + y;}', [2,3], 3);
test('for (let (x = 1, [{a: b, c: d}] = [{a: 1, c: 2}]) x; x != 1; ++x) {return x;}');
test('for (let [[a, [b, c]]] = [[x, []]];;) {return a;}');
test('var sum = 0;for (let y = x; y < 4; ++y) {sum += y;}return sum;', 1, 6);
test('var sum = 0;for (let x = x, y = 10; x < 4; ++x) {sum += x;}return sum;', 1, 6);
test('var sum = 0;for (let x = x; x < 4; ++x) {sum += x;}return x;', 1, 1);
test('var sum = 0;for (let x = eval("x"); x < 4; ++x) {sum += x;}return sum;', 1, 6);
test('var sum = 0;for (let x = x; eval("x") < 4; ++x) {sum += eval("x");}return sum;', 1, 6);
test('var sum = 0;for (let x = eval("x"); eval("x") < 4; ++x) {sum += eval("x");}return sum;', 1, 6);
test('for (var y = 1;;) {return x;}');
test('for (var y = 1;; ++y) {return x;}');
test('for (var y = 1; ++y;) {return x;}');
test('for (var [, {a: [], b: []}] = x, [] = x; x;) {return x;}');
test('for (var X = 1, [y, z] = x, a = x; z < 4; ++z) {return X + y;}', [2,3], 3);
test('var sum = 0;for (var y = x; y < 4; ++y) {sum += y;}return sum;', 1, 6);
test('var sum = 0;for (var X = x, y = 10; X < 4; ++X) {sum += X;}return sum;', 1, 6);
test('var sum = 0;for (var X = x; X < 4; ++X) {sum += X;}return x;', 1, 1);
test('var sum = 0;for (var X = eval("x"); X < 4; ++X) {sum += X;}return sum;', 1, 6);
test('var sum = 0;for (var X = x; eval("X") < 4; ++X) {sum += eval("X");}return sum;', 1, 6);
test('var sum = 0;for (var X = eval("x"); eval("X") < 4; ++X) {sum += eval("X");}return sum;', 1, 6);
test('try {for (let x = eval("throw x");;) {}} catch (e) {return e;}');
test('try {for (let x = x + "s"; eval("throw x");) {}} catch (e) {return e;}', 'ponie');
test('for (let y = x;;) {let x;return y;}');
test('for (let y = x;;) {let y;return x;}');
test('for (let y;;) {let y;return x;}');
test('for (let a = x;;) {let c = x, d = x;return c;}');
test('for (let [a, b] = x;;) {let c = x, d = x;return c;}');
test('for (let [] = [[]] = {};;) {return x;}');
test('for (let [a] = (1, [x]);;) {return a;}');
test('for (let [a] = (1, x, 1, x);;) {return a;}', ['ponies']);
isError('for (let x = 1, x = 2;;) {}');
isError('for (let [x, y] = a, {a:x} = b;;) {}');
isError('for (let [x, y, x] = a;;) {}');
isError('for (let [x, [y, [x]]] = a;;) {}');

// for(in)
test('for (let i in x) {return x;}');
test('for (let i in x) {let y;return x;}');
test('for each (let [a, b] in x) {let y;return x;}');
test('for (let i in x) {let (i = x) {return i;}}');
test('for (let i in x) {let i = x;return i;}');
test('for each (let [x, y] in x) {return x + y;}', [['ponies', '']]);
test('for each (let [{0: x, 1: y}, z] in x) {return x + y + z;}', [[['po','nies'], '']]);
test('var s = "";for (let a in x) {for (let b in x) {s += a + b;}}return s;', [1,2], '00011011');
test('var res = "";for (let i in x) {res += x[i];}return res;');
test('var res = "";for (var i in x) {res += x[i];}return res;');
test('for each (let {x: y, y: x} in [{x: x, y: x}]) {return y;}');
test('for (let x in eval("x")) {return x;}', {ponies:true});
test('for (let x in x) {return eval("x");}', {ponies:true});
test('for (let x in eval("x")) {return eval("x");}', {ponies:true});
test('for ((let (x = {y: true}) x).y in eval("x")) {return eval("x");}');
test('for (let i in x) {break;}return x;');
test('for (let i in x) {break;}return eval("x");');
test('for (let x in x) {break;}return x;');
test('for (let x in x) {break;}return eval("x");');
test('a:for (let i in x) {for (let j in x) {break a;}}return x;');
test('a:for (let i in x) {for (let j in x) {break a;}}return eval("x");');
test('var j;for (let i in x) {j = i;break;}return j;', {ponies:true});
test('try {for (let x in eval("throw x")) {}} catch (e) {return e;}');
test('try {for each (let x in x) {eval("throw x");}} catch (e) {return e;}', ['ponies']);
isError('for (let [x, x] in o) {}');
isError('for (let [x, y, x] in o) {}');
isError('for (let [x, [y, [x]]] in o) {}');

// genexps
test('return (i for (i in x)).next();', {ponies:true});
test('return (eval("i") for (i in x)).next();', {ponies:true});
test('return (eval("i") for (i in eval("x"))).next();', {ponies:true});
test('try {return (eval("throw i") for (i in x)).next();} catch (e) {return e;}', {ponies:true});

// array comprehension
test('return [i for (i in x)][0];', {ponies:true});
test('return [eval("i") for (i in x)][0];', {ponies:true});
test('return [eval("i") for (i in eval("x"))][0];', {ponies:true});
test('try {return [eval("throw i") for (i in x)][0];} catch (e) {return e;}', {ponies:true});

// don't forget about switch craziness
test('var y = 3;switch (function () {return eval("y");}()) {case 3:let y;return x;default:;}');
test('switch (x) {case 3:let y;return 3;case 4:let z;return 4;default:return x;}');
test('switch (x) {case 3:let x;break;default:if (x === undefined) {return "ponies";}}');
test('switch (x) {case 3:default:let y;let (y = x) {return y;}}');
isError('switch (x) {case 3:let y;return 3;case 4:let y;return 4;default:;}');

// test weird cases where the decompiler changes tokens
function testWeird(str, printedAs, arg, result)
{
    var fun = new Function('x', str);

    // this is lame and doesn't normalize whitespace so if an assert fails
    // here, see if its just whitespace and fix the caller
    assertEq(fun.toSource(), '(function anonymous(x) {' + printedAs + '})');

    test(printedAs, arg, result);
}

testWeird('let y = x;return x;', 'var y = x;return x;');
testWeird('let y = 1, y = x;return y;', 'var y = 1, y = x;return y;');
testWeird('return let ({x:x, y:y} = x) x + y', 'return let ({x, y} = x) x + y;', {x:'pon', y:'ies'});
testWeird('let ({x:x, y:y} = x) {return x + y;}', 'let ({x, y} = x) {return x + y;}', {x:'pon', y:'ies'});
