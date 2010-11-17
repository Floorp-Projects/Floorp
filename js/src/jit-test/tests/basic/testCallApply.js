function script1() { return arguments.length; }
function script2(x) { return x; }
function script3(x) { var o = arguments; return o[0]; }
function genClosure() { var x = 3; eval("x = 4"); return function(y) { return x + y } };
var closed1 = genClosure();
var closed2 = genClosure();
var closed3 = genClosure();
var native1 = String.prototype.search;
var native2 = String.prototype.match;
var tricky1 = { call:function(x,y) { return y }, apply:function(x,y) { return y } };

test0();
test1();
test2();
test3();

function test0() {
    assertEq(script1.call(null), 0);
    assertEq(script1.call(null, 1), 1);
    assertEq(script1.call(null, 1,2), 2);
    assertEq(native1.call("aabc", /b/), 2);
    assertEq(native1.call("abc"), -1);
    assertEq(tricky1.call(null, 9), 9);
    assertEq(script1.apply(null), 0);
    assertEq(script1.apply(null, [1]), 1);
    assertEq(script1.apply(null, [1,2]), 2);
    assertEq(native1.apply("aabc", [/b/]), 2);
    assertEq(native1.apply("abc"), -1);
    assertEq(tricky1.apply(null, 1), 1);
}
test0();

function test1() {
    function f(arr) {
        for (var i = 0; i < 10; ++i) {
            for (var j = 0; j < arr.length; ++j) {
                arr[j].call('a');
                arr[j].apply('a', []);
                var arg0 = [];
                arr[j].apply('a', arg0);
                (function() { arr[j].apply('a', arguments); })();

                arr[j].call('a', 1);
                arr[j].apply('a', [1]);
                var arg0 = [1];
                arr[j].apply('a', arg0);
                (function() { arr[j].apply('a', arguments); })(1);

                arr[j].call('a', 1,'g');
                arr[j].apply('a', [1,'g']);
                var arg0 = [1,'g'];
                arr[j].apply('a', arg0);
                (function() { arr[j].apply('a', arguments); })(1,'g');

                arr[j].call('a', 1,'g',3,4,5,6,7,8,9);
                arr[j].apply('a', [1,'g',3,4,5,6,7,8,9]);
                var arg0 = [1,'g',3,4,5,6,7,8,9];
                arr[j].apply('a', arg0);
                (function() { arr[j].apply('a', arguments); })(1,'g',3,4,5,6,7,8,9);
            }
        }
    }

    f([script1, script1, script1, script1, script2, script2, script1, script2]);
    f([script1, script2, script3, script1, script2, script3, script3, script3]);
    f([script1, script2, script2, script2, script2, script3, script1, script2]);
    f([script1, script1, script1, native1, native1, native1, native1, script1]);
    f([native1, native1, native1, native2, native2, native2, native2, native1]);
    f([native1, native2, native1, native2, native1, native2, native1, native2]);
    f([native1, native1, native1, script1, script2, script2, native1, script3]);
    f([closed1, closed1, closed1, closed2, closed2, closed2, script3, script3]);
    f([closed1, closed2, closed1, closed2, closed1, closed2, closed1, closed2]);
    f([closed1, closed2, closed3, closed1, closed2, closed3, script1, script2]);
    f([closed1, closed1, closed1, closed2, closed2, closed2, native1, native2]);
    f([closed1, closed1, closed1, closed2, closed2, closed2, native1, native2]);
    f([native1, native1, native1, closed1, closed2, script1, script2, native2]);
}

// test things that break our speculation
function test2() {
    var threw = false;
    try {
        (3).call(null, 1,2);
    } catch (e) {
        threw = true;
    }
    assertEq(threw, true);

    var threw = false;
    try {
        (3).apply(null, [1,2]);
    } catch (e) {
        threw = true;
    }
    assertEq(threw, true);

    var threw = false;
    try {
        var arr = [1,2];
        (3).apply(null, arr);
    } catch (e) {
        threw = true;
    }
    assertEq(threw, true);

    function tryAndFail(o) {
        var threw = false;
        try {
            o.call(null, 1,2);
        } catch(e) {
            threw = true;
        }
        assertEq(threw, true);
        threw = false;
        try {
            o.apply(null, [1,2]);
        } catch(e) {
            threw = true;
        }
        assertEq(threw, true);
    }

    tryAndFail(1);
    tryAndFail({});
    tryAndFail({call:{}, apply:{}});
    tryAndFail({call:function() { throw "not js_fun_call"}, apply:function(){ throw "not js_fun_apply" }});
}

// hit the stubs::CompileFunction path
function test3() {
    function genFreshFunction(s) { return new Function(s, "return " + s); }

    function callIt(f) {
        assertEq(f.call(null, 1,2), 1);
    }
    callIt(script2); callIt(script2); callIt(script2); callIt(script2);
    callIt(genFreshFunction("x"));
    callIt(genFreshFunction("y"));
    callIt(genFreshFunction("z"));

    function applyIt(f) {
        var arr = [1,2];
        assertEq(f.apply(null, arr), 1);
    }
    applyIt(script2); applyIt(script2); applyIt(script2); applyIt(script2);
    applyIt(genFreshFunction("x"));
    applyIt(genFreshFunction("y"));
    applyIt(genFreshFunction("z"));

    function applyIt1(f) {
        function g() {
            assertEq(f.apply(null, arguments), 1);
        }
        g(1,2);
    }
    applyIt1(script2); applyIt1(script2); applyIt1(script2); applyIt1(script2);
    applyIt1(genFreshFunction("x"));
    applyIt1(genFreshFunction("y"));
    applyIt1(genFreshFunction("z"));

    function applyIt2(f) {
        assertEq(f.apply(null, [1,2]), 1);
    }
    applyIt2(script2); applyIt2(script2); applyIt2(script2); applyIt2(script2);
    applyIt2(genFreshFunction("x"));
    applyIt2(genFreshFunction("y"));
    applyIt2(genFreshFunction("z"));
}
