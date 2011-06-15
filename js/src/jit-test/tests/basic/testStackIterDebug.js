// |jit-test| mjitalways;debug

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

/*********************************************/

(function f() { evalInFrame(0, "assertStackIs(['eval-code', evalInFrame, f, 'global-code'])"); })();
(function f() { (function g() { evalInFrame(1, "assertStackIs(['eval-code', f, 'global-code'])"); })() })();
(function f() { (function g() { evalInFrame(1, "assertStackIs(['eval-code', f, 'bound(f)', 'global-code'])"); })() }).bind()();

(function f() { evalInFrame(0, "assertStackIs(['eval-code', evalInFrame, f, 'global-code'])", true); })();
(function f() { (function g() { evalInFrame(1, "assertStackIs(['eval-code', f, 'global-code'])", true); })() })();
(function f() { (function g() { evalInFrame(1, "assertStackIs(['eval-code', f, 'bound(f)', 'global-code'])", true); })() }).bind()();
(function f() { (function g() { evalInFrame(1, "assertStackIs(['eval-code', f, 'bound(f)', 'global-code'])", true); }).bind()() }).bind()();
(function f() { (function g() { evalInFrame(1, "assertStackIs(['eval-code', f, 'bound(f)', 'global-code'])", true); }).bind().bind()() }).bind()();

(function f() { var o = { toString:function() { evalInFrame(1, "assertStackIs(['eval-code', f, 'global-code'])"); }}; [o,o].sort() })();
(function f() { var o = { toString:function() { evalInFrame(1, "assertStackIs(['eval-code', f, 'global-code'])", true); }}; [o,o].sort() })();

function inner() {
    (function puppies() {
        evalInFrame(1, "assertStackIs(['eval-code', inner, String.prototype.replace, outer, String.prototype.replace, 'global-code'])");
    })();
}
function outer() {
    "bbb".replace(/b/g, inner);
}
"aaa".replace(/a/g, outer);
