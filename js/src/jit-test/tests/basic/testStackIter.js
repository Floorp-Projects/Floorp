function stackToString(stack) {
    var str = "|";
    for (var i = 0; i < stack.length; ++i) {
        if (typeof stack[i] === "string")
            str += stack[i];
        else
            str += stack[i].name;
        str += "|";
    }
    return str;
}

function assertStackIs(s1) {
    var s2 = dumpStack();
    var me = s2.shift();
    assertEq(me, assertStackIs);
    try {
        if (s1.length != s2.length)
            throw "Length " + s1.length + " not equal " + s2.length;
        for (var i = 0; i < s1.length; ++i) {
            var match;
            if (typeof s1[i] === "string" && (m = s1[i].match(/bound\((.*)\)/))) {
                if (s2[i].name != m[1] || s2[i].toSource().indexOf("[native code]") < 0)
                    throw "Element " + i + " not bound function";
            } else if (s1[i] != s2[i]) {
                throw "Element " + i + " not equal";
            }
        }
    }
    catch (e) {
        print("Given     = " + stackToString(s1));
        print("dumpStack = " + stackToString(s2));
        throw e;
    }
}

/***********/

assertStackIs(["global-code"]);
(function f() { assertStackIs([f, "global-code"]) })();
eval("assertStackIs(['eval-code', 'global-code'])");
(function f() { eval("assertStackIs(['eval-code', f, 'global-code'])"); })();
(function f() { eval("(function g() { assertStackIs([g, 'eval-code', f, 'global-code']); })()"); })();
(function f() { assertStackIs([f, 'bound(f)', 'global-code']); }).bind()()
this['eval']("assertStackIs(['eval-code', eval, 'global-code'])");
eval.bind(null, "assertStackIs(['eval-code', eval, 'bound(eval)', 'global-code'])")();
(function f() { assertStackIs([f, Function.prototype.call, 'global-code']) }).call(null);
(function f() { (function g(x,y,z) { assertStackIs([g,f,'global-code']); })() })(1);

/***********/

var gen = (function g() { assertStackIs([g, gen.next, fun, 'global-code']); yield; })();
var fun = function f() { gen.next() };
fun();

var gen = (function g(x) { assertStackIs([g, gen.next, fun, 'global-code']); yield; })(1,2,3);
var fun = function f() { gen.next() };
fun();

var gen = (function g(x) { assertStackIs([g, gen.next, 'eval-code', fun, 'global-code']); yield; })(1,2,3);
var fun = function f() { eval('gen.next()') };
fun();

/***********/

const N = 100;

(function f(x) {
    if (x === 0) {
        var s = dumpStack();
        for (var i = 0; i < N; ++i)
            assertEq(s[i], f);
        return;
    }
    f(x-1);
})(N);

/***********/

"abababab".replace(/b/g, function g() {
    assertStackIs([g, String.prototype.replace, "global-code"]);
});

/***********/

var obj = {
    toString:function toString() {
        assertStackIs([toString, String.prototype.concat, "global-code"]);
        return "";
    }
}
"a".concat(obj);

(function f() {
    var obj = {
        toString:(Array.prototype.sort.bind([1,2], function cb() {
            assertStackIs([cb, Array.prototype.sort, "bound(sort)",
                           String.prototype.concat, f, "global-code"]);
            throw "OK";
        }))
    }

    try {
        "a".concat(obj);
    } catch(e) {
        assertEq(e, "OK");
    }
})();

/***********/

var obj = { valueOf:function valueOf() {
    assertStackIs([valueOf, Math.sin, Array.prototype.sort, "global-code"]);
} };
[obj, obj].sort(Math.sin);

var obj = { valueOf:(function valueOf() {
    assertStackIs([valueOf, "bound(valueOf)", Math.sin, Array.prototype.sort, "global-code"]);
}).bind() };
[obj, obj].sort(Math.sin);

var obj = { valueOf:(function valueOf() {
    assertStackIs([valueOf, "bound(valueOf)", "bound(valueOf)", "bound(valueOf)",
                   Math.sin, Array.prototype.sort, "global-code"]);
}).bind().bind().bind() };
[obj, obj].sort(Math.sin);

/***********/

var proxy = Proxy.createFunction({}, function f() { assertStackIs([f, "global-code"]) });
proxy();
new proxy();

/***********/

for (var i = 0; i < 10; ++i) {
    /* No loss for scripts. */
    (function f() {
        assertStackIs([f, Function.prototype.call, 'global-code']);
    }).call(null);

    /* Loss for natives. */
    (function f() {
        var stack = dumpStack();
        assertEq(stack[0], f);
        if (stack.length === 4) {
            assertEq(stack[1].name, 'f');
            assertEq(stack[2], Function.prototype.call);
        } else {
            assertEq(stack.length, 3);
            assertEq(stack[1], Function.prototype.call);
        }
    }).bind().call(null);
}
