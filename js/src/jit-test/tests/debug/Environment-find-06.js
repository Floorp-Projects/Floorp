// Environment.prototype.find finds bindings that are function arguments, 'let'
// bindings, or FunctionExpression names.

var g = newGlobal();
g.eval("function h() { debugger; }");

var dbg = new Debugger(g);

function test1(code) {
    var hits = 0;
    dbg.onDebuggerStatement = function (frame) {
        var env = frame.older.environment.find('X');
        assertEq(env.names().indexOf('X') !== -1, true);
        assertEq(env.type, 'declarative');
        assertEq(env.parent !== null, true);
        hits++;
    };
    g.eval(code);
    assertEq(hits, 1);
}

var manyNames = '';
for (var i = 0; i < 2048; i++)
    manyNames += 'x' + i + ', ';
manyNames += 'X';

function test2(code) {
    print(code + " : one");
    test1(code.replace('@@', 'X'));
    print(code + " : many");
    test1(code.replace('@@', manyNames));
}

test2('function f(@@) { h(); }  f(1);');
test2('function f(@@) { h(); }  f();');
test2('function f(@@) { return function g() { h(X); }; }  f(1)();');
test2('function f(@@) { return function g() { h(X); }; }  f()();');

test2('                    { let @@ = 0; h(); }');
test2('function f(a, b, c) { let @@ = 0; h(); }  f(1, 2, 3);');
test2('             { let @@ = 0; { let y = 0; h(); } }');
test2('function f() { let @@ = 0; { let y = 0; h(); } }  f();');
test2('             { for (let @@ = 0; X < 1; X++) h(); }');
test2('function f() { for (let @@ = 0; X < 1; X++) h(); }  f();');
test2('             {        (let (@@ = 0) let (y = 2, z = 3) h()); }');
test2('function f() { return (let (@@ = 0) let (y = 2, z = 3) h()); }  f();');

test1('(function X() { h(); })();');
test1('(function X(a, b, c) { h(); })(1, 2, 3);');
